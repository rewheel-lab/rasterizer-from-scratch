#include <algorithm>
#include <cmath>
#include <cstdint>
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

Vec3 operator+(Vec3 a, Vec3 b) {
    return Vec3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 operator-(Vec3 a, Vec3 b) {
    return Vec3{a.x - b.x, a.y - b.y, a.z - b.z};
}

// 3つの成分を、まとめて s 倍する
// Multiply all three components by s.
Vec3 operator*(Vec3 v, float s) {
    return Vec3{v.x * s, v.y * s, v.z * s};
}

// 内積。長さ1の向きどうしなら、向きのそろい具合（同じ向きで 1、直角で 0、反対向きで -1）になる
// Dot product. For unit directions, it measures how well they align: 1 for the same direction, 0 for perpendicular, and -1 for opposite directions.
float dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float length(Vec3 v) {
    return std::sqrt(dot(v, v));
}

Vec3 normalize(Vec3 v) {
    return v * (1.0f / length(v));  // 長さで割って、長さを1にする / Divide by the length to make the length 1.
}

// 0xRRGGBB の形の数を Color にする。お絵かきソフトのカラーコードと同じ並びで、左の2桁が赤、真ん中が緑、右の2桁が青
// Convert a number in 0xRRGGBB format to Color. It uses the same order as color codes in drawing software: the left two digits are red, the middle two green, and the right two blue.
Color from_hex(uint32_t hex) {
    const uint8_t r = static_cast<uint8_t>((hex >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((hex >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(hex & 0xFF);
    return Color{r, g, b};
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

// a から b へ向かう線から見て、p が右にあればプラス、左にあればマイナス、線の上なら 0 を返す
// 右と左は、画面に映った向きのまま。画面の y は下向きなので、数学の教科書の図とは逆に見える
// 値の大きさは、a, b, p を頂点とする三角形の面積の2倍になっている
// Return a positive value if p is to the right of the directed line from a to b, a negative value if it is to the left, and 0 if it is on the line.
// Right and left are based on the screen as shown. Since screen y points down, they look reversed from diagrams in math books.
// The absolute value is twice the area of the triangle with vertices a, b, and p.
float edge_function(Vec2 a, Vec2 b, Vec2 p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// 三角形の辺 a → b が、上の辺か左の辺かを返す
// 頂点が画面の上で時計回りなら、三角形の中は、進む向きから見て右側にある
// 右へ進む水平な辺は、中が下にあるので上の辺。上へ進む辺は、中が右にあるので左の辺
// Return whether triangle edge a -> b is a top edge or a left edge.
// If the vertices are clockwise on screen, the inside of the triangle is on the right as we move along an edge.
// For a horizontal edge going right, the inside is below it, so it is a top edge. For an edge going up, the inside is to the right, so it is a left edge.
bool is_top_left(Vec2 a, Vec2 b) {
    const bool is_top = (a.y == b.y) && (b.x > a.x);
    const bool is_left = b.y < a.y;
    return is_top || is_left;
}

// 辺の値 e が、三角形の内側かを返す
// 0 より大きければ中。ちょうど 0（線の上）は、上の辺と左の辺の上だけ自分のものにする
// 隣の三角形と共有する辺は、片方では左（か上）、もう片方では右（か下）になるので、ちょうど1回だけ塗られる
// Return whether edge value e is inside the triangle.
// A value greater than 0 is inside. For exactly 0 (on the edge), include only top and left edges.
// A shared edge is left (or top) for one triangle and right (or bottom) for the other, so it is drawn exactly once.
bool covers(float e, bool top_left) {
    if (e > 0.0f) {
        return true;
    }
    return e == 0.0f && top_left;
}

// 3つの色を、割合 w0, w1, w2 で混ぜる。w0 が a の割合
// Blend the three colors using weights w0, w1, and w2. w0 is the weight for a.
Color mix_color(Color color_a, Color color_b, Color color_c, float w0, float w1, float w2) {
    const float r = static_cast<float>(color_a.r) * w0 + static_cast<float>(color_b.r) * w1 + static_cast<float>(color_c.r) * w2;
    const float g = static_cast<float>(color_a.g) * w0 + static_cast<float>(color_b.g) * w1 + static_cast<float>(color_c.g) * w2;
    const float b = static_cast<float>(color_a.b) * w0 + static_cast<float>(color_b.b) * w1 + static_cast<float>(color_c.b) * w2;

    // 0.5 を足してから切り捨てると、四捨五入になる。切り捨てだけでは、全体が少し暗くなる
    // Adding 0.5 before truncating rounds to the nearest integer. Truncating alone makes the whole image a little darker.
    return Color{
        static_cast<uint8_t>(r + 0.5f),
        static_cast<uint8_t>(g + 0.5f),
        static_cast<uint8_t>(b + 0.5f),
    };
}

// a, b, c を頂点とする三角形の中を塗る。色は頂点ごとに決め、間をなめらかに混ぜる
// 頂点は、画面の上で時計回りに並べる。反時計回りだと面積がマイナスになり、何も塗られない
// Fill the triangle with vertices a, b, and c. Set a color at each vertex and blend smoothly between them.
// Order the vertices clockwise on screen. If they are counterclockwise, the area is negative and nothing is filled.
void fill_triangle(Canvas& canvas, Vec2 a, Vec2 b, Vec2 c, Color color_a, Color color_b, Color color_c) {
    // 三角形全体の面積の2倍。各辺の値をこれで割ると、頂点ごとの割合になる
    // 0 のときは割れないので、塗らずに終わる
    // Twice the triangle's area. Divide each edge value by this to get the weight for each vertex.
    // If it is 0, division is not possible, so do not fill the triangle.
    const float area = edge_function(a, b, c);
    if (area <= 0.0f) {
        return;
    }

    // 三角形をぎりぎり囲む四角形を求める。この四角形の中のピクセルだけ調べれば足りる
    // 画面の外まで調べても塗れない（set_pixel が捨てる）ので、範囲を画面の中に切り詰める
    // 大きく外へはみ出した三角形で、無駄な繰り返しをしないため
    // 小数のまま切り詰めてから整数にするのは、とても大きな値を int にすると壊れるため
    // Find the tight bounding box around the triangle. We only need to check the pixels inside it.
    // Checking outside the screen cannot draw anything (set_pixel discards those pixels), so clamp the range to the screen.
    // This avoids unnecessary loops for triangles that extend far off-screen.
    // Clamp the values while they are still floats, before converting them to integers, because converting a very large value to int can break the program.
    const float last_x = static_cast<float>(canvas.width - 1);
    const float last_y = static_cast<float>(canvas.height - 1);
    const float box_min_x = std::clamp(std::min(a.x, std::min(b.x, c.x)), 0.0f, last_x);
    const float box_max_x = std::clamp(std::max(a.x, std::max(b.x, c.x)), 0.0f, last_x);
    const float box_min_y = std::clamp(std::min(a.y, std::min(b.y, c.y)), 0.0f, last_y);
    const float box_max_y = std::clamp(std::max(a.y, std::max(b.y, c.y)), 0.0f, last_y);

    // 小数の範囲を、外へ広げて整数にする。ぎりぎり内側のピクセルを取りこぼさないため
    // Round the fractional range outward to integers so we do not miss pixels at the edge.
    const int min_x = static_cast<int>(std::floor(box_min_x));
    const int max_x = static_cast<int>(std::ceil(box_max_x));
    const int min_y = static_cast<int>(std::floor(box_min_y));
    const int max_y = static_cast<int>(std::ceil(box_max_y));

    // 3本の辺が、上の辺か左の辺か。三角形ごとに1回決めれば足りる
    // Whether each of the three edges is a top or left edge. Decide once per triangle.
    const bool top_left0 = is_top_left(b, c);
    const bool top_left1 = is_top_left(c, a);
    const bool top_left2 = is_top_left(a, b);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            // ピクセルは点ではなく小さなマス。マスを代表する点は、真ん中（+0.5）にする
            // 整数の位置のままだと、整数の位置に置いた辺の上に、判定の点がいつも乗ってしまう
            // A pixel is a small square, not a point. Use its center (+0.5) as the representative point.
            // If we use integer positions, the test point always lies on edges placed at integer positions.
            const Vec2 p = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};

            // 3本の辺それぞれについて、p が右側にいるか調べる
            // For each of the three edges, check whether p is on the right side.
            const float e0 = edge_function(b, c, p);
            const float e1 = edge_function(c, a, p);
            const float e2 = edge_function(a, b, p);

            if (covers(e0, top_left0) && covers(e1, top_left1) && covers(e2, top_left2)) {
                // 重心座標。辺の値を面積で割ると、合計がちょうど 1 になる3つの数になる
                // w0 は a の割合。a に近いほど、a の向かい側の辺 b → c が作る三角形が大きくなるため
                // Barycentric coordinates. Dividing each edge value by the area gives three weights that add up to 1.
                // w0 is the weight for a. The closer a point is to a, the larger the triangle formed by the point and the opposite edge b -> c.
                const float w0 = e0 / area;
                const float w1 = e1 / area;
                const float w2 = e2 / area;
                set_pixel(canvas, x, y, mix_color(color_a, color_b, color_c, w0, w1, w2));
            }
        }
    }
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

// ここに、この回で書くものを足していく
//   scale_color        … 色を s 倍にして、明るさを変える関数
//   to_vec2            … Vec3 の x, y だけを取り出す関数
//   triangles          … 立方体の12個の三角形（角の番号の3つ組）の表
//   face_brightness    … 面ごとの明るさの表
//   draw_depth_buffer  … 深度バッファを白黒の絵にして、画面に出す関数
// 上の to_screen は深度も返す形に、fill_triangle は深度つきに、draw_cube は線ではなく面を塗る形に、作り直す
// edges（辺の表）と mix_color は、この回の作り直しで使わなくなる
// Add what we write in this episode here.
// scale_color ... A function that scales a color by s to change its brightness.
// to_vec2 ... A function that takes only x and y from Vec3.
// triangles ... A table of the cube's 12 triangles (triples of corner indices).
// face_brightness ... A table of brightness values for each face.
// draw_depth_buffer ... A function that shows the depth buffer as a grayscale image.
// In this episode, change to_screen so it also returns depth, make fill_triangle use depth, and make draw_cube fill faces instead of drawing lines.
// The edges table and mix_color are no longer used after these changes.

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する / Display a 320x180 image enlarged 4 times.

    const Color background = from_hex(0x101030);
    const Color yellow = from_hex(0xFFD93B);
    const Color blue = from_hex(0x4D9BFF);
    const Color gray = from_hex(0xB4B4B4);

    // 透視投影の行列。画面に映る範囲を決める。毎フレーム変わらないので、ループの外で1回だけ作る
    // 縦の視野角は 50 度。#06 の 90 度だと広すぎて、宇宙を収めようとすると小さくなり、近づくと手前が大きく歪む
    // ニアは 0.1、ファーは 100。この先の回でも、この値を使う
    // Perspective projection matrix. It sets the visible range. Since it does not change each frame, create it once outside the loop.
    // The vertical field of view is 50 degrees. The 90 degrees in #06 is too wide: fitting the universe makes it look small, and moving close makes the front look very distorted.
    // The near plane is 0.1 and the far plane is 100. We use these values in later episodes too.
    const float aspect = static_cast<float>(canvas.width) / static_cast<float>(canvas.height);
    const Mat4 projection = perspective(to_radians(50.0f), aspect, 0.1f, 100.0f);

    // 世界全体を傾けて、奥へ押し出す行列。宇宙を、少し上から見下ろすため（右側の、傾けるほうが先に効く）
    // rotate_x は、プラスの角度だと手前が上がって奥が下がる。見下ろすには、手前を下げて奥を上げたいので、マイナスの角度にする
    // 9.5 だけ押し出すと、地球と月が公転の途中でカメラの後ろに回らず、画面にも収まる
    // A matrix that tilts the whole world and moves it away, so we can look down at the universe a little (the rightmost matrix, which tilts, takes effect first).
    // With rotate_x, a positive angle raises the front and lowers the back. To look down, we want the front lower and the back higher, so use a negative angle.
    // Moving it away by 9.5 keeps Earth and Moon in view as they orbit without going behind the camera.
    const Mat4 view = translate(0.0f, 0.0f, 9.5f) * rotate_x(to_radians(-20.0f));

    // この回で、ここに深度バッファ（std::vector<float>）と、白黒表示の切り替え（show_depth）を足す
    // Add the depth buffer (std::vector<float>) and the grayscale display toggle (show_depth) here in this episode.

    while (canvas.update()) {
        // 起動してからの秒数。そのまま角度（ラジアン）として使う。1秒でおよそ 57 度回る
        // Seconds since startup. Use this directly as the angle in radians. It rotates about 57 degrees per second.
        const float time = canvas.time;

        // 毎フレーム、背景を塗り直す。前のフレームの絵を消すため
        // Redraw the background every frame to erase the previous frame.
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // この回の最後で、ここで深度バッファも消す
        // At the end of this episode, clear the depth buffer here too.

        // 太陽は、真ん中（原点）でゆっくり回る
        // The Sun slowly rotates at the center (the origin).
        const Mat4 sun = rotate_y(time * 0.5f);

        // 地球の公転。右へ 3.5 動かしてから、太陽（原点）のまわりに回す。動かしてから回すので、太陽のまわりを回る
        // 地球と月が共有する
        // Earth's orbit. Move 3.5 units to the right, then rotate around the Sun (the origin). Rotating after moving makes Earth orbit the Sun.
        // Used by both Earth and Moon.
        const Mat4 earth_orbit = rotate_y(time * 1.0f) * translate(3.5f, 0.0f, 0.0f);

        // 地球。半分の大きさにして、その場で回してから、公転の行列でつなぐ
        // Earth. Scale it to half size, rotate it in place, then combine it with the orbit matrix.
        const Mat4 earth = earth_orbit * rotate_y(time * 2.0f) * scale(0.5f);

        // 月。地球の公転の行列のうしろに、自分の動きを足すだけ。地球が動けば、月もついてくる
        // 地球から 1.4 離す。近すぎると地球の立方体に、遠すぎると太陽の立方体に、めり込んで見える
        // The Moon. Add its own motion after Earth's orbit matrix. When Earth moves, the Moon moves with it.
        // Place it 1.4 units from Earth. If it is too close, it clips into Earth's cube. If it is too far, it clips into the Sun's cube.
        const Mat4 moon = earth_orbit * rotate_y(time * 3.0f) * translate(1.4f, 0.0f, 0.0f) * scale(0.2f);

        // この回で、ここ（3つの draw_cube）を、面を塗る書き方に差し替える。最初は太陽だけ塗る（地球と月の行は、いったんコメントにする）
        // 右から順に、立方体自身の動き、世界の傾きと押し出し、透視投影
        // Replace the three draw_cube calls with face filling here. Start by filling only the Sun (temporarily comment out the Earth and Moon lines).
        // From right to left: the cube's own motion, the world rotation and translation, and perspective projection.
        draw_cube(canvas, projection * view * sun, yellow);
        draw_cube(canvas, projection * view * earth, blue);
        draw_cube(canvas, projection * view * moon, gray);

        // この回で、ここ（present の前）に、深度バッファの白黒表示を足す
        // Add the grayscale depth display here, before present, in this episode.

        canvas.present();
    }
}
