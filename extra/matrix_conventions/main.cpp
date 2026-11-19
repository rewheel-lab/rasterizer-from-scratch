// 番外編「OpenGL と DirectX」の実験用プログラム
// ラスタライザー編 #07 の太陽・地球・月に、決まりの違う行列を差し替えて、絵がどう変わるかを確かめる
// 行列の関数は #07 のものをそのまま使い、「ネットのサンプルから写した」行列を足してある
// （translate_row、perspective_rh、perspective_minus_one。transpose は、行と列を入れ替える道具）
//
// 操作
//   1 … このチャンネルの決まり（左手系、M * v、奥行き 0〜1）。#07 と同じ絵
//   2 … 実験1-a：右手系の透視投影を、そのまま使う（宇宙は z = +9.5 のまま）
//   3 … 実験1-b：右手系の透視投影で、宇宙を z = -9.5 に置き直す
//   4 … 実験2-a：「右の位置で、その場で回る地球」を、行ベクトル用の translate_row で作る。M * v のまま使う
//   5 … 実験2-b：translate は直したうえで、行ベクトルのサンプルの掛ける順番（R * T）を、そのまま写す
//         （4 と 5 は、太陽と地球だけ。月は描かない）
//   6 … 実験3：近づいてくる立方体を、このチャンネルの perspective（奥行き 0〜1）で描く。深度が 0 より小さいところは赤
//   7 … 実験3：同じ場面を、perspective_minus_one（奥行き -1〜1）で描く
//   V     … 左右に並べる（右：今のモード。左：1〜3 と 6・7 はこのチャンネルの決まり、4・5 はサンプルが作りたかった絵）
//   A     … 世界の軸を出す（x が橙、y が緑、z が紫。1〜5 のとき）
//   T     … 距離ごとの深度の表を、コンソールに出す
//   Space … 止める、動かす
//   R     … 時間を 0 に戻す
//   Esc   … 終わる
// Experiment program for the extra episode “OpenGL and DirectX.”
// Use the renderer from Rasterizer #07 to draw the Sun, Earth, and Moon, then swap in matrices with different conventions to see how the image changes.
// The matrix functions from #07 are kept as they are. This program adds matrices copied from online examples: translate_row, perspective_rh, and perspective_minus_one. transpose swaps rows and columns.
// 
// Controls
// 1 … This channel’s conventions: left-handed coordinates, M * v, and depth 0 to 1. Same image as #07.
// 2 … Experiment 1-a: use a right-handed perspective matrix as-is (keep the solar system at z = +9.5).
// 3 … Experiment 1-b: move the solar system to z = -9.5 for the right-handed camera.
// 4 … Experiment 2-a: build an Earth that rotates in place at the right-hand position, using the row-vector translate_row matrix. Keep M * v.
// 5 … Experiment 2-b: fix translate, then copy the sample’s row-vector multiplication order (R * T).
// (4 and 5 draw only the Sun and Earth, not the Moon.)
// 6 … Experiment 3: draw an approaching cube with this channel’s perspective matrix (depth 0 to 1). Show depths below 0 in red.
// 7 … Experiment 3: draw the same scene with perspective_minus_one (depth -1 to 1).
// V … Split the screen. Right: current mode. Left: this channel’s conventions for 1–3 and 6–7; the image the sample intended for 4–5.
// A … Show world axes: x orange, y green, z purple (modes 1–5).
// T … Print a table of depth by distance to the console.
// Space … Pause or resume.
// R … Reset time to 0.
// Esc … Exit.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <numbers>
#include <vector>

#include "framework/canvas.h"

// 画面上の位置。ピクセルの間の位置も表せるように、小数で持つ
// Screen position stored as decimals so it can represent positions between pixels.
struct Vec2 {
    float x;
    float y;
};

// 3D の位置や向き。x が右、y が上、z が奥
// A 3D position or direction. x points right, y points up, and z points away.
struct Vec3 {
    float x;
    float y;
    float z;
};

// 0xRRGGBB の形の数を Color にする。お絵かきソフトのカラーコードと同じ並びで、左の2桁が赤、真ん中が緑、右の2桁が青
// Convert a number in 0xRRGGBB format to Color. It uses the same order as color codes in drawing software: the left two digits are red, the middle two green, and the right two blue.
Color from_hex(uint32_t hex) {
    const uint8_t r = static_cast<uint8_t>((hex >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((hex >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(hex & 0xFF);
    return Color{r, g, b};
}

// 色の明るさをずらす。選ばれていない印を、暗くするために使う
// Shift color brightness. Used to darken unselected markers.
Color shade(Color color, int delta) {
    return Color{
        static_cast<uint8_t>(std::clamp(color.r + delta, 0, 255)),
        static_cast<uint8_t>(std::clamp(color.g + delta, 0, 255)),
        static_cast<uint8_t>(std::clamp(color.b + delta, 0, 255)),
    };
}

// (x, y) のピクセルを color にする
// Set the pixel at (x, y) to color.
void set_pixel(Canvas& canvas, int x, int y, Color color) {
    // 画面の外は何もしない。はみ出して pixels の外を書くと、プログラムが壊れるため
    // Do nothing outside the screen. Writing past pixels would break the program.
    if (x < 0 || x >= canvas.width || y < 0 || y >= canvas.height) {
        return;
    }
    canvas.pixels[y * canvas.width + x] = color;
}

// (x0, y0) から (x1, y1) まで、1ピクセルずつ進みながら線を引く
// Draw a line from (x0, y0) to (x1, y1), moving one pixel at a time.
void draw_line(Canvas& canvas, int x0, int y0, int x1, int y1, Color color) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;

    // x と y のうち長いほうの軸で数える。短いほうは1歩で1マス以下しか動かないので、間が空かない
    // Count along the longer of the x and y axes. The shorter axis moves by at most one cell per step, so there are no gaps.
    const int steps = std::max(std::abs(dx), std::abs(dy));

    // 始点と終点が同じ場所だと steps が 0 になり、下の割り算ができない。点を1つ打って終わる
    // If the start and end are the same, steps is 0 and the division below is not possible. Draw one point and return.
    if (steps == 0) {
        set_pixel(canvas, x0, y0, color);
        return;
    }

    for (int i = 0; i <= steps; i++) {
        // t は 0（始点）から 1（終点）まで進む割合
        // t is the fraction along the line, from 0 (start) to 1 (end).
        const float t = static_cast<float>(i) / static_cast<float>(steps);

        // 切り捨て（static_cast<int> だけ）だと、いつも 0 の向きに削れて線が片側にずれる。最も近いピクセルに丸める
        // Truncating with static_cast<int> always rounds toward 0 and shifts the line to one side. Round to the nearest pixel.
        const int x = x0 + static_cast<int>(std::round(static_cast<float>(dx) * t));
        const int y = y0 + static_cast<int>(std::round(static_cast<float>(dy) * t));
        set_pixel(canvas, x, y, color);
    }
}

// 点が入っているピクセルを調べてから、線を引く。位置は小数で受け取る
// Check which pixels contain the endpoints before drawing the line. Receive positions as floating-point values.
void draw_line(Canvas& canvas, Vec2 a, Vec2 b, Color color) {
    // 画面から遠く離れた点を含む線は、引かずに捨てる
    // z がほぼ 0 だと、割り算の答えがとても大きくなる。そのまま引くと、何億歩も進んで固まる。int に入りきらない大きさだと、壊れる
    // カメラの近くを正しく扱う方法は、#12 で作る
    // NaN は、どんな数と比べても false になる。だから「範囲の中」を調べて ! を付ける形にすると、NaN もここで捨てられる
    // Skip lines that contain points far outside the screen.
    // When z is almost 0, dividing by it produces a very large number. Drawing the line as is can take hundreds of millions of steps and freeze the program. If the value is too large for int, it can break.
    // We will handle points near the camera correctly in #12.
    // NaN compares false with every number. So checking whether a point is in range and negating the result also skips NaN here.
    const float limit = 10000.0f;
    if (!(std::abs(a.x) < limit && std::abs(a.y) < limit && std::abs(b.x) < limit && std::abs(b.y) < limit)) {
        return;
    }

    // ピクセル (x, y) は、x 〜 x+1、y 〜 y+1 の範囲を受け持ち、真ん中が +0.5 の位置にある
    // だから、小数の位置が入っているピクセルの番号は、切り捨て（floor）で決まる。四捨五入だと半ピクセルずれる
    // Pixel (x, y) covers the range from x to x+1 and y to y+1, with its center at +0.5.
    // So find the pixel containing a fractional position by truncating (floor). Rounding would shift it by half a pixel.
    const int x0 = static_cast<int>(std::floor(a.x));
    const int y0 = static_cast<int>(std::floor(a.y));
    const int x1 = static_cast<int>(std::floor(b.x));
    const int y1 = static_cast<int>(std::floor(b.y));
    draw_line(canvas, x0, y0, x1, y1, color);
}

// 度をラジアンに直す。sin と cos はラジアンで受け取るため
// 半周が 180 度で、π ラジアン。だから 180 で割って π を掛ける
// Convert degrees to radians because sin and cos use radians.
// A half turn is 180 degrees, or pi radians, so divide by 180 and multiply by pi.
float to_radians(float degrees) {
    return degrees * std::numbers::pi_v<float> / 180.0f;
}

// 4つの数の組。3D の点（x, y, z）に、4つ目の数 w を足したもの
// 点を表すときは、w を 1 にしておく。そうすると、行列で「移動」も掛け算で書ける
// A group of four numbers. Add a fourth value w to a 3D point (x, y, z).
// Set w to 1 for a point. This lets us write translation as matrix multiplication.
struct Vec4 {
    float x;
    float y;
    float z;
    float w;
};

// 4×4 の数の表。m[行][列]
// A 4x4 matrix of numbers. m[row][column]
struct Mat4 {
    float m[4][4];
};

// 行列 × 点。点を、行列が表す動きで動かす。行列は左に書く（M * v）
// 結果の上から i 番目の数は、行列の i 行目と、点の4つの数を、掛けて足したもの
// Matrix times point. Apply the motion represented by the matrix to the point. Write the matrix on the left (M * v).
// The i-th result is the sum of each value in row i multiplied by the corresponding one of the point's four values.
Vec4 operator*(const Mat4& a, Vec4 v) {
    const float in[4] = {v.x, v.y, v.z, v.w};
    float out[4] = {};
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            out[row] += a.m[row][col] * in[col];
        }
    }
    return Vec4{out[0], out[1], out[2], out[3]};
}

// 行列 × 行列。2つの動きを、1つの行列にまとめる
// (a * b) * v は、a * (b * v) と同じ。つまり、先に b の動きをして、次に a の動きをする
// 右側の行列から順に効くので、書いた順番と、動く順番は逆になる
// Matrix times matrix. Combine two transformations into one matrix.
// (a * b) * v is the same as a * (b * v). In other words, apply b first, then a.
// Matrices take effect from the right, so the order of written matrices is the reverse of the order of motion.
Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r = {};
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            for (int k = 0; k < 4; k++) {
                r.m[row][col] += a.m[row][k] * b.m[k][col];
            }
        }
    }
    return r;
}

// 何も動かさない行列。掛けても相手が変わらない（数の 1 のようなもの）
// The identity matrix. Multiplying by it leaves the other value unchanged, like multiplying a number by 1.
Mat4 identity() {
    return Mat4{{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

// 点を (x, y, z) だけ動かす行列
// A matrix that moves a point by (x, y, z).
Mat4 translate(float x, float y, float z) {
    Mat4 r = identity();  // 何もしない行列から始める / Start with the identity matrix.
    // 一番右の列に、動かす量を入れる。点の w（= 1）に掛かって、足し算になる
    // Put the translation in the rightmost column. Multiplying the point's w (= 1) adds it to the position.
    r.m[0][3] = x;  
    r.m[1][3] = y;
    r.m[2][3] = z;
    return r;
}

// 大きさを s 倍にする行列。原点からの距離が s 倍になる
// A matrix that scales by s. The distance from the origin becomes s times larger.
Mat4 scale(float s) {
    Mat4 r = identity();
    r.m[0][0] = s;
    r.m[1][1] = s;
    r.m[2][2] = s;
    return r;
}

// y 軸のまわりに angle だけ回す行列。angle はラジアン
// 中身は #06 の rotate_y の式を、4×4 の表に並べ直しただけ。回る向きも同じ
// A matrix that rotates by angle around the y axis. angle is in radians.
// It is the #06 rotate_y formula rearranged into a 4x4 matrix. It turns the same way.
Mat4 rotate_y(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return Mat4{{
        {c, 0.0f, s, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {-s, 0.0f, c, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

// x 軸のまわりに angle だけ回す行列。angle はラジアン
// こちらも #06 の rotate_x と同じ向き。プラスの角度だと、手前が上がって、奥が下がる
// A matrix that rotates by angle around the x axis. angle is in radians.
// It turns the same way as rotate_x in #06. A positive angle raises the front and lowers the back.
Mat4 rotate_x(float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return Mat4{{
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, c, -s, 0.0f},
        {0.0f, s, c, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    }};
}

// 透視投影の行列。カメラは原点にいて、+Z（奥）を向いている
// fov_y は縦の視野角（ラジアン）、aspect は画面の横 ÷ 縦
// near_z より近いものと、far_z より遠いものは映さない。この行列を掛けて w で割ると、z が near_z で 0、far_z で 1 になる
// （near と far は Windows のヘッダーで別の意味に使われている名前なので、避けている）
// Perspective projection matrix. The camera is at the origin and faces +Z (away).
// fov_y is the vertical field of view (in radians), and aspect is screen width / height.
// Objects closer than near_z or farther than far_z are not shown. After multiplying by this matrix and dividing by w, z is 0 at near_z and 1 at far_z.
// (We avoid the names near and far because Windows headers use them for other things.)
Mat4 perspective(float fov_y, float aspect, float near_z, float far_z) {
    // 視野角が狭いほど、f は大きくなって、絵が拡大される。縦 90 度なら f は 1
    // The narrower the field of view, the larger f becomes and the more the image is magnified. At a 90-degree vertical field of view, f is 1.
    const float f = 1.0f / std::tan(fov_y * 0.5f);

    Mat4 r = {};
    // 画面は横長なので、x は縦横比で割って、縦と同じ縮尺にそろえる
    // The image is wide, so divide x by the aspect ratio to use the same scale as the vertical axis.
    r.m[0][0] = f / aspect;  
    r.m[1][1] = f;
    r.m[2][2] = far_z / (far_z - near_z);
    r.m[2][3] = -near_z * far_z / (far_z - near_z);
    // 一番下の行で、z の値を w にコピーする。あとで w で割ると、z で割ったことになる
    // Copy the z value to w in the bottom row. Dividing by w later is then equivalent to dividing by z.
    r.m[3][2] = 1.0f;  
    return r;
}

// w で割ってから、画面のピクセルの位置にする。clip は、透視投影まで掛け終わった点
// カメラの後ろ（w が 0 以下）の点は、ここでは正しく扱えない。#12 で片付ける
// Divide by w to get screen pixel positions. clip is the point after perspective projection.
// Points behind the camera (w <= 0) are not handled correctly here. We will fix this in #12.
Vec2 to_screen(const Canvas& canvas, Vec4 clip) {
    // w で割ると、画面に映る範囲が、x も y も -1〜1 になる。#06 の「z で割る」にあたる
    // Dividing by w maps the visible range to -1 to 1 on both x and y. This is the matrix version of dividing by z in #06.
    const float nx = clip.x / clip.w;
    const float ny = clip.y / clip.w;

    // -1〜1 に 1 を足して半分にすると 0〜1。そこに画面の幅と高さを掛けると、ピクセルの位置になる
    // 画面の y は下向きなので、y は 1 から引いて上下をひっくり返す
    // Add 1 to the range -1 to 1 and divide by 2 to get 0 to 1. Multiply by the screen width and height to get pixel coordinates.
    // Screen y points down, so subtract y from 1 to flip it vertically.
    const float sx = (nx + 1.0f) * 0.5f * static_cast<float>(canvas.width);
    const float sy = (1.0f - ny) * 0.5f * static_cast<float>(canvas.height);
    return Vec2{sx, sy};
}

// 立方体の8つの角。中心が原点で、1辺の長さは 2。回す中心を原点にしておくと、その場で回る
// The cube's eight corners. Its center is at the origin and each side is 2 units long. Rotating around the origin makes it spin in place.
const Vec3 cube[8] = {
    // 手前の4つ（z が -1）
    // The four front vertices (z = -1).
    {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},  
    // 奥の4つ（z が 1）
    // The four back vertices (z = 1).
    {-1.0f, -1.0f, 1.0f}, {1.0f, -1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}, {-1.0f, 1.0f, 1.0f},  
};

// 辺。角の番号を2つ組にして、12本を並べる
// Edges. Store the two endpoint indices as pairs, listing all 12 edges.
const int edges[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},  // 手前の面 / Front face.
    {4, 5}, {5, 6}, {6, 7}, {7, 4},  // 奥の面 / Back face.
    {0, 4}, {1, 5}, {2, 6}, {3, 7},  // 手前と奥をつなぐ / Connect the front and back faces.
};

// 立方体の12本の辺を、色 color で描く
// mvp は、右から順に「立方体自身の動き」「世界の傾きと押し出し」「透視投影」を掛け合わせた行列
// （GPU の世界では、それぞれの頭文字を取って、この行列を mvp と呼ぶ）
// 8つの角はどれも、この行列1つを掛けるだけで、画面の位置になる
// Draw the cube's 12 edges in color color.
// mvp is the product of these matrices, from right to left: the cube's own motion, the world rotation and translation, and perspective projection.
// (In GPU terms, mvp is named from the first letters of these transformations.)
// Multiplying each of the eight corners by this one matrix gives its screen position.
void draw_cube(Canvas& canvas, const Mat4& mvp, Color color) {
    Vec2 screen[8] = {};
    for (int i = 0; i < 8; i++) {
        const Vec4 clip = mvp * Vec4{cube[i].x, cube[i].y, cube[i].z, 1.0f};
        screen[i] = to_screen(canvas, clip);
    }

    // 12本の辺を、両端を画面に写してから、#02 の draw_line で引く
    // Project both ends of each of the 12 edges to the screen, then draw the edge with draw_line from #02.
    for (int i = 0; i < 12; i++) {
        draw_line(canvas, screen[edges[i][0]], screen[edges[i][1]], color);
    }
}

// ここから下の translate_row、perspective_rh、perspective_minus_one は、ネットのサンプルから写したもの
// #07 の translate と perspective は、このチャンネルの決まりで作ってある
// The translate_row, perspective_rh, and perspective_minus_one functions below are copied from online examples.
// The translate and perspective functions from #07 use this channel’s conventions.

// サンプルから写した、点を (x, y, z) だけ動かす行列。行ベクトル（v * M）用
// このチャンネルの translate との違いは、動かす量の入れ場所。一番右の列ではなく、一番下の行に入っている
// 行ベクトルは行列の左から掛けるので、点の w（= 1）に掛かるのが、一番下の行だから
// A sample matrix that moves a point by (x, y, z), for row vectors (v * M).
// The difference from this channel’s translate is where the offset goes: in the bottom row instead of the rightmost column.
// A row vector multiplies from the left, so the point’s w (= 1) uses the bottom row.
Mat4 translate_row(float x, float y, float z) {
    Mat4 r = identity();
    r.m[3][0] = x;
    r.m[3][1] = y;
    r.m[3][2] = z;
    return r;
}

// 行と列を入れ替えた行列。行ベクトル用の行列は、これで M * v 用の形になる
// （translate_row を転置すると、このチャンネルの translate と同じになる）
// Swap the rows and columns of a matrix. This turns a row-vector matrix into a form for M * v.
// (Transposing translate_row gives the same matrix as this channel’s translate.)
Mat4 transpose(const Mat4& a) {
    Mat4 r = {};
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 4; col++) {
            r.m[row][col] = a.m[col][row];
        }
    }
    return r;
}

// サンプルから写した、右手系の透視投影の行列。カメラは原点にいて、-Z を向いている
// このチャンネルの perspective との違いは、手の向きだけ。深度は 0〜1 のまま
// ・カメラの前にあるものの z はマイナスになる。だから、w に入れるのは z ではなく -z（一番下の行がマイナス）
// ・z が -near_z で 0、-far_z で 1 になるよう、z の行の符号を変えてある
// A sample right-handed perspective matrix. The camera is at the origin and looks along -Z.
// Only the handedness differs from this channel’s perspective matrix; depth stays 0 to 1.
// - Points in front of the camera have negative z, so w uses -z (the bottom row is negative).
// - The z row’s signs make z map to 0 at -near_z and 1 at -far_z.
Mat4 perspective_rh(float fov_y, float aspect, float near_z, float far_z) {
    const float f = 1.0f / std::tan(fov_y * 0.5f);

    Mat4 r = {};
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = far_z / (near_z - far_z);
    r.m[2][3] = near_z * far_z / (near_z - far_z);
    r.m[3][2] = -1.0f;
    return r;
}

// サンプルから写した、深度が -1〜1 の透視投影の行列。手の向きは、このチャンネルと同じ左手系
// このチャンネルの perspective との違いは、z の行（m[2][2] と m[2][3]）だけ。w で割ると、z が near_z で -1、far_z で 1 になる
// 0〜1 の深度を 2 倍して 1 を引いたもの、と同じ
// A sample perspective matrix with depth from -1 to 1, using this channel’s left-handed coordinates.
// Only the z row (m[2][2] and m[2][3]) differs from this channel’s perspective matrix. After dividing by w, z is -1 at near_z and 1 at far_z.
// This is the same as doubling depth from 0 to 1 and subtracting 1.
Mat4 perspective_minus_one(float fov_y, float aspect, float near_z, float far_z) {
    const float f = 1.0f / std::tan(fov_y * 0.5f);

    Mat4 r = {};
    r.m[0][0] = f / aspect;
    r.m[1][1] = f;
    r.m[2][2] = (far_z + near_z) / (far_z - near_z);
    r.m[2][3] = -2.0f * far_z * near_z / (far_z - near_z);
    r.m[3][2] = 1.0f;
    return r;
}

// 実験用の決まり
// Settings for the experiments.
const int mode_count = 7;
const float fov_y_degrees = 50.0f;

// 実験1・2のニアとファー。#07 と同じ
// Near and far for experiments 1 and 2. Same as #07.
const float space_near = 0.1f;
const float space_far = 100.0f;

// 実験3のニアとファー。ニアを #07 の 10 倍にしてある
// ニアが 0.1 だと、-1〜1 の行列で赤くなる範囲は 0.1〜0.2 の間だけで、立方体がカメラにくっつくまで見えない
// ニアを 1 にすると、1〜2 の間になって、立方体が画面に収まったまま、赤い部分を見られる
// Near and far for experiment 3. Near is ten times the value in #07.
// With near at 0.1, the red range for the -1 to 1 matrix is only from 0.1 to 0.2, so it is not visible until the cube is very close.
// With near at 1, the range is 1 to 2, so the red area is visible while the cube still fits on screen.
const float approach_near = 1.0f;
const float approach_far = 100.0f;

// 実験1・2で、モードごとに差し替える行列。地球と月は、それぞれ1つの行列にまとめてある
// Matrices swapped by mode for experiments 1 and 2. The Earth and Moon each use one combined matrix.
struct Setup {
    Mat4 projection;
    Mat4 view;
    Mat4 earth;
    Mat4 moon;
    bool has_moon;
};

// mode に合わせて、行列を決める
// 1〜3 は、まず #07 と同じ行列を作り、モードごとに差し替える
// 0、4、5 は実験2の絵。太陽と地球だけで、月は描かない。0 は、サンプルが作りたかった絵（右の位置で、その場で回る地球）
// Choose matrices for the mode.
// For modes 1–3, first build the same matrices as #07, then swap them by mode.
// Modes 0, 4, and 5 show experiment 2: only the Sun and Earth, without the Moon. Mode 0 is the image the sample intended: Earth at the right, rotating in place.
Setup make_setup(int mode, float aspect, float time) {
    const float fov_y = to_radians(fov_y_degrees);

    // 地球の公転の行列は、地球と月が共有する
    // The Earth and Moon share the Earth’s orbit matrix.
    const Mat4 earth_orbit = rotate_y(time * 1.0f) * translate(3.5f, 0.0f, 0.0f);

    Setup setup = {
        perspective(fov_y, aspect, space_near, space_far),
        translate(0.0f, 0.0f, 9.5f) * rotate_x(to_radians(-20.0f)),
        earth_orbit * rotate_y(time * 2.0f) * scale(0.5f),
        earth_orbit * rotate_y(time * 3.0f) * translate(1.4f, 0.0f, 0.0f) * scale(0.2f),
        true,
    };

    if (mode == 2) {
        // 実験1-a：透視投影だけを、右手系のものに差し替える。宇宙は +Z のまま（右手系のカメラから見ると、後ろ）
        // Experiment 1-a: replace only the perspective matrix with a right-handed one. Keep the solar system at +Z, behind the right-handed camera.
        setup.projection = perspective_rh(fov_y, aspect, space_near, space_far);
    } else if (mode == 3) {
        // 実験1-b：宇宙を -Z（右手系のカメラの前）に置き直す
        // 右手系では、手前が +Z になる。だから、見下ろすための傾きも、符号を逆にする
        // Experiment 1-b: move the solar system to -Z, in front of the right-handed camera.
        // In right-handed coordinates, +Z is closer to the camera, so reverse the sign of the tilt used to look down.
        setup.projection = perspective_rh(fov_y, aspect, space_near, space_far);
        setup.view = translate(0.0f, 0.0f, -9.5f) * rotate_x(to_radians(20.0f));
    } else if (mode == 0) {
        // サンプルが作りたかった地球。縮めて、その場で回して、右へ 3.5 置く。このチャンネルの書き方では、右から順に効く
        // The Earth image intended by the sample: scale it down, rotate it in place, and move it 3.5 units right. In this channel’s order, transforms take effect from right to left.
        setup.has_moon = false;
        setup.earth = translate(3.5f, 0.0f, 0.0f) * rotate_y(time * 2.0f) * scale(0.5f);
    } else if (mode == 4) {
        // 実験2-a：サンプルの translate_row を、そのまま M * v で使う。ほかは mode 0 と同じ
        // Experiment 2-a: use the sample’s translate_row as-is with M * v. Everything else matches mode 0.
        setup.has_moon = false;
        setup.earth = translate_row(3.5f, 0.0f, 0.0f) * rotate_y(time * 2.0f) * scale(0.5f);
    } else if (mode == 5) {
        // 実験2-b：サンプルは v * S * R * T（縮めて、その場で回して、右へ置く）
        // translate_row は M * v 用に転置して直す。掛ける順番は、R * T をサンプルのとおりに左から写す
        // 縮める scale だけは、地球の大きさが変わらないよう、このチャンネルの書き方（一番右）に置く
        // このチャンネルでは右から効くので、「動かしてから回す」になる
        // Experiment 2-b: the sample uses v * S * R * T (scale, rotate in place, then move right).
        // Transpose translate_row to fix it for M * v. Keep the sample’s R * T order.
        // Place only the scale matrix at the far right, as this channel does, so Earth keeps the same size.
        // Here transforms take effect from right to left, so this order moves the Earth before rotating it.
        setup.has_moon = false;
        setup.earth = rotate_y(time * 2.0f) * transpose(translate_row(3.5f, 0.0f, 0.0f)) * scale(0.5f);
    }
    return setup;
}

// 世界の x・y・z の軸を、原点から長さ 2.4 の線で描く（x が橙、y が緑、z が紫）
// 絵が上下左右さかさまになっていないか、奥と手前が入れ替わっていないかを、見分けるため
// Draw the world x, y, and z axes as lines of length 2.4 from the origin (x orange, y green, z purple).
// Use them to check whether the image is upside down or reversed front to back.
void draw_axes(Canvas& canvas, const Mat4& mvp) {
    const Vec3 tips[3] = {{2.4f, 0.0f, 0.0f}, {0.0f, 2.4f, 0.0f}, {0.0f, 0.0f, 2.4f}};
    const Color colors[3] = {from_hex(0xFF8030), from_hex(0x40E060), from_hex(0xE060FF)};

    const Vec2 origin = to_screen(canvas, mvp * Vec4{0.0f, 0.0f, 0.0f, 1.0f});
    for (int i = 0; i < 3; i++) {
        const Vec2 tip = to_screen(canvas, mvp * Vec4{tips[i].x, tips[i].y, tips[i].z, 1.0f});
        draw_line(canvas, origin, tip, colors[i]);
    }
}

// 実験1・2：太陽・地球・月を描く。#07 の main のループの中身と同じ
// viewport は、画面のどこに映すか（全体なら identity）。show_axes が true なら、世界の軸も描く
// Experiments 1 and 2: draw the Sun, Earth, and Moon. This matches the main loop in #07.
// viewport sets where to draw on screen (identity means the full screen). If show_axes is true, also draw the world axes.
void draw_space(Canvas& canvas, const Mat4& viewport, const Setup& setup, float time, bool show_axes) {
    const Color yellow = from_hex(0xFFD93B);
    const Color blue = from_hex(0x4D9BFF);
    const Color gray = from_hex(0xB4B4B4);

    const Mat4 camera = viewport * setup.projection * setup.view;
    const Mat4 sun = rotate_y(time * 0.5f);

    if (show_axes) {
        draw_axes(canvas, camera);
    }
    draw_cube(canvas, camera * sun, yellow);
    draw_cube(canvas, camera * setup.earth, blue);
    if (setup.has_moon) {
        draw_cube(canvas, camera * setup.moon, gray);
    }
}

// a から b へ線を引く。ただし、w で割ったあとの深度が 0 より小さいピクセルは、warning の色にする
// depth_a と depth_b は、両端の深度（w で割ったあとの z）
// w で割ったあとの深度は、画面の上で一次式になる（A + B / w の形で、1 / w が画面の上で一次式のため）
// だから、線に沿って両端の深度を混ぜるだけで、途中のピクセルの深度が求まる
// Draw a line from a to b, coloring pixels red when depth after division by w is below 0.
// depth_a and depth_b are the endpoint depths (z after division by w).
// Depth after division by w is linear on screen (A + B / w, because 1 / w is linear on screen).
// So blend the endpoint depths along the line to find the depth of each pixel.
void draw_line_depth(Canvas& canvas, Vec2 a, float depth_a, Vec2 b, float depth_b, Color color, Color warning) {
    // 画面から遠く離れた点を含む線は、引かずに捨てる。理由は draw_line と同じ
    // Skip lines that include points far outside the screen. The reason is the same as for draw_line.
    const float limit = 10000.0f;
    if (!(std::abs(a.x) < limit && std::abs(a.y) < limit && std::abs(b.x) < limit && std::abs(b.y) < limit)) {
        return;
    }

    const int x0 = static_cast<int>(std::floor(a.x));
    const int y0 = static_cast<int>(std::floor(a.y));
    const int x1 = static_cast<int>(std::floor(b.x));
    const int y1 = static_cast<int>(std::floor(b.y));
    const int dx = x1 - x0;
    const int dy = y1 - y0;
    const int steps = std::max(std::abs(dx), std::abs(dy));

    if (steps == 0) {
        set_pixel(canvas, x0, y0, (depth_a < 0.0f) ? warning : color);
        return;
    }

    for (int i = 0; i <= steps; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = x0 + static_cast<int>(std::round(static_cast<float>(dx) * t));
        const int y = y0 + static_cast<int>(std::round(static_cast<float>(dy) * t));
        const float depth = depth_a + (depth_b - depth_a) * t;
        set_pixel(canvas, x, y, (depth < 0.0f) ? warning : color);
    }
}

// 実験3：カメラに向かって近づいてくる立方体を、projection で映して描く
// w で割ったあとの深度が 0 より小さいところは、赤にする（0 より小さい深度は、近すぎるとして切る予定の範囲）
// Experiment 3: project and draw a cube moving toward the camera.
// Color areas red when depth after division by w is below 0 (depths below 0 are meant to be clipped as too close).
void draw_approaching_cube(Canvas& canvas, const Mat4& projection, float time) {
    // 立方体は、カメラの正面を、遠く（z = 8）から近く（z = 1.2）へ、また遠くへと、20 秒で往復する
    // 一定の速さでゆっくり動かす。ゆっくり回すのは、手前の角の距離が、少しずつ変わるようにするため
    // Move the cube back and forth in front of the camera, from far (z = 8) to near (z = 1.2) and back over 20 seconds.
    // Move slowly at a steady speed. Rotate slowly so the distance to the near corner changes gradually.
    const float period = 20.0f;
    const float phase = std::fmod(time, period) / period;
    const float closeness = 1.0f - std::abs(2.0f * phase - 1.0f);  // 0 → 1 → 0
    const float z = 8.0f + (1.2f - 8.0f) * closeness;
    const Mat4 model = translate(0.0f, 0.0f, z) * rotate_y(time * 0.5f) * rotate_x(to_radians(30.0f)) * scale(0.3f);
    const Mat4 mvp = projection * model;

    Vec2 screen[8] = {};
    float depth[8] = {};
    for (int i = 0; i < 8; i++) {
        const Vec4 clip = mvp * Vec4{cube[i].x, cube[i].y, cube[i].z, 1.0f};

        // カメラの後ろ（w が 0 以下）は扱えない。この立方体は、そこまで近づかない
        // Points behind the camera (w at or below 0) cannot be handled. The cube never gets that close.
        if (clip.w <= 0.0f) {
            return;
        }
        screen[i] = to_screen(canvas, clip);
        depth[i] = clip.z / clip.w;
    }

    const Color color = from_hex(0xA0E0FF);
    const Color red = from_hex(0xFF3030);
    for (int i = 0; i < 12; i++) {
        const int a = edges[i][0];
        const int b = edges[i][1];
        draw_line_depth(canvas, screen[a], depth[a], screen[b], depth[b], color, red);
    }
}

// 画面の半分に縮めて映すための行列。透視投影のあと、w で割る前に掛ける
// x と y を半分にして、x を半分ずつ左右にずらす。ずらす量 shift は、w に掛けてある（translate と同じ形）
// w で割ると、shift がそのまま「画面の幅の 1/4」のずれになる。左半分なら -0.5、右半分なら 0.5
// Matrix to draw into half the screen. Apply it after perspective projection but before dividing by w.
// Halve x and y, then shift x left or right. The shift is multiplied by w, like translate.
// After division by w, shift moves the image by a quarter of the screen width: -0.5 for the left and 0.5 for the right.
Mat4 half_screen(float shift) {
    Mat4 r = identity();
    r.m[0][0] = 0.5f;
    r.m[1][1] = 0.5f;
    r.m[0][3] = shift;
    return r;
}

// モードに合わせて、場面を描く。viewport は、画面のどこに映すか（全体なら identity）
// mode 0 は、実験2でサンプルが作りたかった絵。キーでは選べず、左右に並べたときの左側にだけ使う
// Draw a scene for the selected mode. viewport sets where it appears on screen (identity means the full screen).
// Mode 0 is the image intended by the sample for experiment 2. It cannot be selected with a key; it is used only on the left side of split view.
void draw_mode(Canvas& canvas, int mode, float aspect, float time, const Mat4& viewport, bool show_axes) {
    if (mode <= 5) {
        const Setup setup = make_setup(mode, aspect, time);
        draw_space(canvas, viewport, setup, time, show_axes);
    } else if (mode == 6) {
        const Mat4 projection = perspective(to_radians(fov_y_degrees), aspect, approach_near, approach_far);
        draw_approaching_cube(canvas, viewport * projection, time);
    } else {
        const Mat4 projection = perspective_minus_one(to_radians(fov_y_degrees), aspect, approach_near, approach_far);
        draw_approaching_cube(canvas, viewport * projection, time);
    }
}

// 距離 z の位置にある点の、w で割ったあとの深度
// Depth after division by w for a point at distance z.
float depth_at(const Mat4& projection, float z) {
    const Vec4 clip = projection * Vec4{0.0f, 0.0f, z, 1.0f};
    return clip.z / clip.w;
}

// カメラからの距離ごとの深度を、2つの行列で並べて表示する
// Show depth by distance from the camera for two matrices.
void print_depth_table(float aspect, float near_z, float far_z, const float distances[], int count) {
    const Mat4 zero_to_one = perspective(to_radians(fov_y_degrees), aspect, near_z, far_z);
    const Mat4 minus_one_to_one = perspective_minus_one(to_radians(fov_y_degrees), aspect, near_z, far_z);

    std::printf("depth after dividing by w (near = %g, far = %g)\n", static_cast<double>(near_z), static_cast<double>(far_z));
    std::printf("  distance   perspective (0..1)   perspective_minus_one (-1..1)\n");
    for (int i = 0; i < count; i++) {
        std::printf("  %8.3f   %18.5f   %29.5f\n",
                    static_cast<double>(distances[i]),
                    static_cast<double>(depth_at(zero_to_one, distances[i])),
                    static_cast<double>(depth_at(minus_one_to_one, distances[i])));
    }
}

// 深度が -1〜1 の行列で、深度がちょうど 0 になる距離。ニアの約 2 倍
// -1〜1 の深度は、0〜1 の深度を 2 倍して 1 を引いたもの。だから 0 になるのは、0〜1 の深度が 0.5 のとき
// Distance where depth is exactly 0 with the -1 to 1 matrix: about twice the near distance.
// Depth from -1 to 1 is depth from 0 to 1 doubled and reduced by 1. So it is 0 where depth from 0 to 1 is 0.5.
float minus_one_zero_distance(float near_z, float far_z) {
    return 2.0f * near_z / (1.0f + near_z / far_z);
}

// 左右に並べたときの、左側のモード。比べる相手の絵
// 1〜3 は、このチャンネルの決まり（1）。4 と 5 は、サンプルが作りたかった絵（0）。6 と 7 は、このチャンネルの行列（6）
// Mode shown on the left in split view, for comparison.
// Modes 1–3 use this channel’s convention (1); modes 4 and 5 show the image intended by the sample (0); modes 6 and 7 use this channel’s matrix (6).
int reference_mode(int mode) {
    if (mode >= 6) {
        return 6;
    }
    if (mode >= 4) {
        return 0;
    }
    return 1;
}

// モードを切り替えたときに、名前と、差し替えている行列を、コンソールに出す
// Print the mode name and swapped matrices to the console when the mode changes.
void print_mode(int mode) {
    switch (mode) {
        case 1:
            std::printf("mode 1: this channel's convention (left-handed, M * v, depth 0..1). The same picture as ep07_done\n");
            std::printf("  projection  = perspective\n");
            std::printf("  view        = translate(0, 0, 9.5) * rotate_x(-20 degrees)\n");
            std::printf("  earth       = rotate_y(time) * translate(3.5, 0, 0) * rotate_y(time * 2) * scale(0.5)   (the moon follows the earth)\n");
            break;
        case 2:
            std::printf("mode 2 (experiment 1-a): a right-handed projection, used as it is\n");
            std::printf("  projection  = perspective_rh   <- swapped (the camera looks toward -Z, w = -z)\n");
            std::printf("  view        = translate(0, 0, 9.5) * rotate_x(-20 degrees)   (unchanged: the world is behind this camera)\n");
            break;
        case 3:
            std::printf("mode 3 (experiment 1-b): a right-handed projection, and the world moved to z = -9.5\n");
            std::printf("  projection  = perspective_rh   <- swapped\n");
            std::printf("  view        = translate(0, 0, -9.5) * rotate_x(+20 degrees)   <- swapped (the world is in front of the camera, the tilt flips)\n");
            break;
        case 4:
            std::printf("mode 4 (experiment 2-a): the row-vector translate, used with M * v (the sun and the earth only)\n");
            std::printf("  earth = translate_row(3.5, 0, 0) * rotate_y(time * 2) * scale(0.5)   <- swapped (the amount sits in the bottom row)\n");
            std::printf("  the picture the sample meant: the earth at x = 3.5, spinning in place (V: left side)\n");
            break;
        case 5:
            std::printf("mode 5 (experiment 2-b): the sample's order (v * S * R * T) copied from the left (the sun and the earth only)\n");
            std::printf("  earth = rotate_y(time * 2) * transpose(translate_row(3.5, 0, 0)) * scale(0.5)   <- translate fixed, but R * T copied as it is\n");
            std::printf("  the picture the sample meant: the earth at x = 3.5, spinning in place (V: left side)\n");
            break;
        case 6:
            std::printf("mode 6 (experiment 3): approaching cube, perspective (depth 0..1). near = %g, far = %g\n",
                        static_cast<double>(approach_near), static_cast<double>(approach_far));
            std::printf("  red = depth after dividing by w is less than 0. That happens only when the distance is less than %g\n",
                        static_cast<double>(approach_near));
            break;
        default:
            std::printf("mode 7 (experiment 3): approaching cube, perspective_minus_one (depth -1..1). near = %g, far = %g\n",
                        static_cast<double>(approach_near), static_cast<double>(approach_far));
            std::printf("  projection  = perspective_minus_one   <- swapped\n");
            std::printf("  red = depth after dividing by w is less than 0. That happens when the distance is less than %.2f\n",
                        static_cast<double>(minus_one_zero_distance(approach_near, approach_far)));
            break;
    }
}

// キーが押された瞬間だけ true を返す。押しっぱなしでは1回しか反応しない
// Return true only on the frame when the key is pressed. Holding it down does not trigger it again.
bool key_pressed(Canvas& canvas, int key, bool was_down[]) {
    const bool is_down = canvas.is_key_down(key);
    const bool pressed = is_down && !was_down[key];
    was_down[key] = is_down;
    return pressed;
}

// canvas 全体を color で塗りつぶす。前のフレームの絵を消すため
// Fill the whole canvas with color to clear the previous frame.
void clear_canvas(Canvas& canvas, Color color) {
    std::fill(canvas.pixels.begin(), canvas.pixels.end(), color);
}

// canvas の四角形（x, y から幅 w、高さ h）の中のピクセルを、取り出す
// Get pixels from a rectangle on the canvas, starting at (x, y) with width w and height h.
std::vector<Color> copy_rect(const Canvas& canvas, int x, int y, int w, int h) {
    std::vector<Color> pixels;
    for (int v = 0; v < h; v++) {
        for (int u = 0; u < w; u++) {
            pixels.push_back(canvas.pixels[(y + v) * canvas.width + (x + u)]);
        }
    }
    return pixels;
}

// copy_rect で取り出したピクセルを、canvas の四角形の中に戻す
// Put pixels from copy_rect back into a rectangle on the canvas.
void paste_rect(Canvas& canvas, const std::vector<Color>& pixels, int x, int y, int w, int h) {
    for (int v = 0; v < h; v++) {
        for (int u = 0; u < w; u++) {
            set_pixel(canvas, x + u, y + v, pixels[v * w + u]);
        }
    }
}

// 四角形のふちを描く。左右に並べたときの、それぞれの画面の範囲を示す
// Draw a rectangle outline showing each view's screen bounds when shown side by side.
void draw_frame(Canvas& canvas, int x, int y, int w, int h, Color color) {
    for (int u = 0; u < w; u++) {
        set_pixel(canvas, x + u, y, color);
        set_pixel(canvas, x + u, y + h - 1, color);
    }
    for (int v = 0; v < h; v++) {
        set_pixel(canvas, x, y + v, color);
        set_pixel(canvas, x + w - 1, y + v, color);
    }
}

// 今のモードを、色の印で示す。選ばれているモードだけ明るい。左から順に、モード 1〜7
// Show the current mode with color markers. Only the selected mode is bright. Modes 1–7 appear from left to right.
void draw_mode_indicator(Canvas& canvas, int x, int y, int mode) {
    const uint32_t hexes[mode_count] = {0x40E040, 0xFF9020, 0xFF60C0, 0xC060FF, 0x40D0FF, 0xFFE040, 0xFF5050};
    for (int k = 0; k < mode_count; k++) {
        const Color color = (k + 1 == mode) ? from_hex(hexes[k]) : shade(from_hex(hexes[k]), -170);
        for (int v = 0; v < 4; v++) {
            for (int u = 0; u < 8; u++) {
                set_pixel(canvas, x + k * 10 + u, y + v, color);
            }
        }
    }
}

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する / Display a 320x180 image enlarged 4 times.

    const Color background = from_hex(0x101030);
    const float aspect = static_cast<float>(canvas.width) / static_cast<float>(canvas.height);

    int mode = 1;
    bool side_by_side = false;
    bool show_axes = false;
    bool paused = false;
    // 止められるように、canvas.time とは別に数える
    // Count separately from canvas.time so the scene can be paused.
    float scene_time = 0.0f;  
    bool was_down[256] = {};

    // 深度の表に出す距離。1つ目はニア 0.1 の表、2つ目は実験3のニア（1）の表
    // Distances shown in the depth tables. The first table uses near 0.1; the second uses the experiment 3 near value (1).
    const float table_distances[11] = {0.1f, 0.15f, 0.2f, 0.3f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 50.0f, 100.0f};
    const float approach_distances[10] = {0.5f, 1.0f, 1.5f, 1.98f, 2.0f, 3.0f, 5.0f, 10.0f, 50.0f, 100.0f};

    std::printf("1-7: mode  V: side by side  A: axes  T: depth table  Space: pause  R: restart  Esc: quit\n");
    print_mode(mode);

    while (canvas.update()) {
        if (canvas.is_key_down(key_escape)) {
            break;
        }

        // モードの切り替え。変えたときだけ、コンソールに知らせる
        // Switch modes and print a message to the console only when the mode changes.
        for (int k = 0; k < mode_count; k++) {
            if (key_pressed(canvas, '1' + k, was_down) && k + 1 != mode) {
                mode = k + 1;
                print_mode(mode);
            }
        }

        if (key_pressed(canvas, 'V', was_down)) {
            side_by_side = !side_by_side;
            std::printf("side by side: %s (right: the current mode, left: this channel's convention, or the sample's intended picture for modes 4 and 5)\n", side_by_side ? "on" : "off");
        }
        if (key_pressed(canvas, 'A', was_down)) {
            show_axes = !show_axes;
            std::printf("axes: %s (x: orange, y: green, z: purple)\n", show_axes ? "on" : "off");
        }
        if (key_pressed(canvas, 'T', was_down)) {
            print_depth_table(aspect, space_near, space_far, table_distances, 11);
            print_depth_table(aspect, approach_near, approach_far, approach_distances, 10);
        }
        if (key_pressed(canvas, key_space, was_down)) {
            paused = !paused;
            std::printf("paused: %s\n", paused ? "yes" : "no");
        }
        if (key_pressed(canvas, 'R', was_down)) {
            scene_time = 0.0f;
        }
        if (!paused) {
            scene_time += canvas.delta_time;
        }

        if (!side_by_side) {
            clear_canvas(canvas, background);
            draw_mode(canvas, mode, aspect, scene_time, identity(), show_axes);
            draw_mode_indicator(canvas, 2, canvas.height - 6, mode);
        } else {
            // 画面を左右に分け、それぞれ縦横を半分に縮めて映す。画面の真ん中の高さに、横長の絵が2つ並ぶ
            // 線がもう一方の範囲にはみ出さないよう、1つずつ描いて、範囲の中だけ取り出す
            // Split the screen in half and draw each view at half width and height. Two wide images appear side by side at screen center.
            // Draw each view separately and copy only its area so lines do not spill into the other view.
            const int panel_w = canvas.width / 2;
            const int panel_h = canvas.height / 2;
            const int panel_y = canvas.height / 4;
            const int left_mode = reference_mode(mode);

            clear_canvas(canvas, background);
            draw_mode(canvas, left_mode, aspect, scene_time, half_screen(-0.5f), show_axes);
            const std::vector<Color> left = copy_rect(canvas, 0, panel_y, panel_w, panel_h);

            clear_canvas(canvas, background);
            draw_mode(canvas, mode, aspect, scene_time, half_screen(0.5f), show_axes);
            const std::vector<Color> right = copy_rect(canvas, panel_w, panel_y, panel_w, panel_h);

            clear_canvas(canvas, background);
            paste_rect(canvas, left, 0, panel_y, panel_w, panel_h);
            paste_rect(canvas, right, panel_w, panel_y, panel_w, panel_h);
            draw_frame(canvas, 0, panel_y, panel_w, panel_h, from_hex(0x505070));
            draw_frame(canvas, panel_w, panel_y, panel_w, panel_h, from_hex(0x505070));
            draw_mode_indicator(canvas, 2, panel_y + panel_h - 7, left_mode);
            draw_mode_indicator(canvas, panel_w + 2, panel_y + panel_h - 7, mode);
        }

        canvas.present();
    }
}
