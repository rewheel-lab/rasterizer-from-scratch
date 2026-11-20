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

// 色を s 倍にする。1 より小さければ暗く、大きければ明るくなる
// 0〜255 を超えると uint8_t に入りきらず、壊れた色になる。だから 255 で止める
// Scale a color by s. Values below 1 make it darker; values above 1 make it brighter.
// Values above 255 do not fit in uint8_t and make a broken color, so clamp to 255.
Color scale_color(Color color, float s) {
    const float r = std::clamp(static_cast<float>(color.r) * s, 0.0f, 255.0f);
    const float g = std::clamp(static_cast<float>(color.g) * s, 0.0f, 255.0f);
    const float b = std::clamp(static_cast<float>(color.b) * s, 0.0f, 255.0f);

    // 0.5 を足してから切り捨てると、四捨五入になる
    // Adding 0.5 before truncating rounds the value.
    return Color{
        static_cast<uint8_t>(r + 0.5f),
        static_cast<uint8_t>(g + 0.5f),
        static_cast<uint8_t>(b + 0.5f),
    };
}

// Vec3 の x, y だけを取り出して、Vec2 にする
// 画面の上の位置は x, y で決まる。z は深度なので、辺の判定には使わない
// Take only x and y from Vec3 to make a Vec2.
// The screen position depends on x and y. z is depth, so it is not used to test the edges.
Vec2 to_vec2(Vec3 v) {
    return Vec2{v.x, v.y};
}

// a, b, c を頂点とする三角形の中を、color で塗る
// 頂点の x, y は画面のピクセルの位置で、z は w で割ったあとの深度（0 が手前、1 が奥）
// 頂点は、画面の上で時計回りに並べる。反時計回りだと面積がマイナスになり、何も塗られない
// depth_buffer は、ピクセルごとに、今までに塗ったものの中で一番手前の深度を覚えておく表。canvas.pixels と同じ並びで、(x, y) は depth_buffer[y * canvas.width + x]
// Fill the triangle with vertices a, b, and c in color color.
// The vertex x and y values are screen pixel positions, and z is depth after dividing by w (0 is near, 1 is far).
// Order vertices clockwise on screen. A counterclockwise order gives a negative area, so nothing is filled.
// depth_buffer stores the nearest depth drawn so far for each pixel. It uses the same order as canvas.pixels, where (x, y) is depth_buffer[y * canvas.width + x].
void fill_triangle(Canvas& canvas, std::vector<float>& depth_buffer, Vec3 a, Vec3 b, Vec3 c, Color color) {
    // 辺の判定は、画面の上の位置（x, y）だけで行う
    // Test edges using only the screen position (x, y).
    const Vec2 a_xy = to_vec2(a);
    const Vec2 b_xy = to_vec2(b);
    const Vec2 c_xy = to_vec2(c);

    // 三角形全体の面積の2倍。各辺の値をこれで割ると、頂点ごとの割合になる
    // 0 のときは割れないので、塗らずに終わる
    // Twice the triangle's area. Divide each edge value by this to get the weight for each vertex.
    // If it is 0, division is not possible, so do not fill the triangle.
    const float area = edge_function(a_xy, b_xy, c_xy);
    if (area <= 0.0f) {
        return;
    }

    // 三角形をぎりぎり囲む四角形を求める。この四角形の中のピクセルだけ調べれば足りる
    // 画面の外は塗れない（set_pixel が捨てる）。深度バッファにも、画面の外の場所はないので、はみ出して読み書きすると壊れる
    // だから、範囲を画面の中に切り詰める。大きく外へはみ出した三角形で、無駄な繰り返しをしないためでもある
    // 小数のまま切り詰めてから整数にするのは、とても大きな値を int にすると壊れるため
    // Find the tight bounding rectangle around the triangle. We only need to check the pixels inside it.
    // We cannot draw outside the screen (set_pixel skips those pixels). The depth buffer has no entries outside the screen either, so reading or writing past it would break the program.
    // Clamp the range to the screen. This also avoids wasted loops for triangles that extend far outside it.
    // Clamp while the values are still floats, before converting to int, because converting a very large value to int would break the program.
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
    const bool top_left0 = is_top_left(b_xy, c_xy);
    const bool top_left1 = is_top_left(c_xy, a_xy);
    const bool top_left2 = is_top_left(a_xy, b_xy);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            // ピクセルは点ではなく小さなマス。マスを代表する点は、真ん中（+0.5）にする
            // 整数の位置のままだと、整数の位置に置いた辺の上に、判定の点がいつも乗ってしまう
            // A pixel is a small square, not a point. Use its center (+0.5) as the representative point.
            // If we use integer positions, the test point always lies on edges placed at integer positions.
            const Vec2 p = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};

            // 3本の辺それぞれについて、p が右側にいるか調べる
            // For each of the three edges, check whether p is on the right side.
            const float e0 = edge_function(b_xy, c_xy, p);
            const float e1 = edge_function(c_xy, a_xy, p);
            const float e2 = edge_function(a_xy, b_xy, p);

            if (covers(e0, top_left0) && covers(e1, top_left1) && covers(e2, top_left2)) {
                // 重心座標。辺の値を面積で割ると、合計がちょうど 1 になる3つの数になる
                // w0 は a の割合。a に近いほど、a の向かい側の辺 b → c が作る三角形が大きくなるため
                // Barycentric coordinates. Dividing each edge value by the area gives three weights that add up to 1.
                // w0 is the weight for a. The closer a point is to a, the larger the triangle formed by the point and the opposite edge b -> c.
                const float w0 = e0 / area;
                const float w1 = e1 / area;
                const float w2 = e2 / area;

                // 深度も、同じ割合で混ぜる。w で割ったあとの深度は、画面の上でまっすぐ（一次式で）変わるので、そのまま混ぜてよい
                // Blend depth with the same weights. Depth after dividing by w changes linearly on screen, so it can be blended directly.
                const float depth = w0 * a.z + w1 * b.z + w2 * c.z;

                // 今までに塗ったものより手前のときだけ、色を塗って、記録も書き換える（深度テスト）
                // Only draw the color and update the depth record if this pixel is closer than what has been drawn so far (depth test).
                const int index = y * canvas.width + x;
                if (depth < depth_buffer[index]) {
                    depth_buffer[index] = depth;
                    set_pixel(canvas, x, y, color);
                }
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

// w で割ってから、画面のピクセルの位置と深度にする。clip は、透視投影まで掛け終わった点
// 返す Vec3 の x, y は画面のピクセルの位置で、z は w で割ったあとの深度（ニアで 0、ファーで 1）
// カメラの後ろ（w が 0 以下）の点は、ここでは正しく扱えない。#12 で片付ける
// Divide by w to get screen pixel positions and depth. clip is the point after perspective projection.
// In the returned Vec3, x and y are screen pixel positions, and z is depth after dividing by w (0 at the near plane, 1 at the far plane).
// Points behind the camera (w <= 0) are not handled correctly here. We will fix this in #12.
Vec3 to_screen(const Canvas& canvas, Vec4 clip) {
    // w で割ると、画面に映る範囲が、x も y も -1〜1 になる。#06 の「z で割る」にあたる
    // Dividing by w maps the visible range to -1 to 1 on both x and y. This is the matrix version of dividing by z in #06.
    const float nx = clip.x / clip.w;
    const float ny = clip.y / clip.w;

    // z も w で割る。perspective の行列が、割ったあとに、ニアで 0、ファーで 1 になるように作ってある
    // Divide z by w too. The perspective matrix is built so the result is 0 at the near plane and 1 at the far plane.
    const float depth = clip.z / clip.w;

    // -1〜1 に 1 を足して半分にすると 0〜1。そこに画面の幅と高さを掛けると、ピクセルの位置になる
    // 画面の y は下向きなので、y は 1 から引いて上下をひっくり返す
    // Add 1 to the range -1 to 1 and divide by 2 to get 0 to 1. Multiply by the screen width and height to get pixel coordinates.
    // Screen y points down, so subtract y from 1 to flip it vertically.
    const float sx = (nx + 1.0f) * 0.5f * static_cast<float>(canvas.width);
    const float sy = (1.0f - ny) * 0.5f * static_cast<float>(canvas.height);
    return Vec3{sx, sy, depth};
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

// 面。角の番号を3つ組にして、12個の三角形を並べる。1つの面（四角形）を、対角線で三角形2つに分ける
// どの三角形も、外から見て時計回りに並べる。1つでも逆だと、その三角形だけ、外から見ても裏と扱われて抜ける
// Faces. Store the three corner indices for each of the 12 triangles. Split each square face into two triangles along a diagonal.
// Order every triangle clockwise as seen from outside. If one is reversed, it is treated as a back face and disappears.
const int triangles[12][3] = {
    {3, 2, 1}, {3, 1, 0},  // 手前の面（z が -1） / Front face (z = -1).
    {6, 7, 4}, {6, 4, 5},  // 奥の面（z が 1） / Back face (z = 1).
    {7, 3, 0}, {7, 0, 4},  // 左の面（x が -1） / Left face (x = -1).
    {2, 6, 5}, {2, 5, 1},  // 右の面（x が 1） / Right face (x = 1).
    {5, 4, 0}, {5, 0, 1},  // 下の面（y が -1） / Bottom face (y = -1).
    {7, 6, 2}, {7, 2, 3},  // 上の面（y が 1） / Top face (y = 1).
};

// 面ごとの明るさ。光はまだないので、面の見分けがつくように、面ごとに少しずつ変えておく
// 1 が元の色で、小さいほど暗い。並びは triangles と同じ（手前、奥、左、右、下、上）。1つの面は三角形2つなので、三角形の番号を 2 で割ると面の番号になる
// Brightness for each face. There is no light yet, so use a slightly different value for each face to make them easy to tell apart.
// 1 keeps the original color; smaller values are darker. The order matches triangles (front, back, left, right, bottom, top). Each face has two triangles, so divide the triangle index by 2 to get its face index.
const float face_brightness[6] = {0.85f, 0.75f, 0.5f, 0.6f, 0.4f, 1.0f};

// 立方体の12個の三角形を、色 color で塗る。裏を向いた三角形は、塗らずに捨てる
// mvp は、右から順に「立方体自身の動き」「世界の傾きと押し出し」「透視投影」を掛け合わせた行列
// （GPU の世界では、それぞれの頭文字を取って、この行列を mvp と呼ぶ）
// 8つの角はどれも、この行列1つを掛けるだけで、画面の位置と深度になる
// Fill the cube's 12 triangles in color color. Skip triangles facing backward.
// mvp is the product of these matrices, from right to left: the cube's own motion, the world rotation and translation, and perspective projection.
// (In GPU terms, mvp is named from the first letters of these transformations.)
// Multiplying each of the eight corners by this one matrix gives its screen position and depth.
void draw_cube(Canvas& canvas, std::vector<float>& depth_buffer, const Mat4& mvp, Color color) {
    Vec3 screen[8] = {};
    for (int i = 0; i < 8; i++) {
        const Vec4 clip = mvp * Vec4{cube[i].x, cube[i].y, cube[i].z, 1.0f};
        screen[i] = to_screen(canvas, clip);
    }

    for (int i = 0; i < 12; i++) {
        const Vec3 a = screen[triangles[i][0]];
        const Vec3 b = screen[triangles[i][1]];
        const Vec3 c = screen[triangles[i][2]];

        // 画面の上で反時計回りなら、裏を向いている。塗る前に、ここで捨てる（背面カリング）
        // #03 で頂点を逆に並べると何も出なかったのは、このため。fill_triangle の最初でも同じ判定で捨てられるが、
        // ここで捨てると、裏の三角形は「塗らない」と読める。この先で、塗る前の計算（明るさなど）が増えても、裏では全部省ける
        // If a triangle is counterclockwise on screen, it faces backward. Skip it here before filling (back-face culling).
        // This is why reversing the vertices in #03 made nothing appear. fill_triangle also skips back faces at the start, but skipping them here makes it clear that we do not fill them. Later, when we add calculations before filling (such as brightness), we can skip all of them for back faces.
        if (edge_function(to_vec2(a), to_vec2(b), to_vec2(c)) <= 0.0f) {
            continue;
        }

        fill_triangle(canvas, depth_buffer, a, b, c, scale_color(color, face_brightness[i / 2]));
    }
}

// 深度バッファを、白黒の絵にして画面に出す。手前ほど黒、奥ほど白。何も塗られていないピクセル（1.0）は真っ白
// 色と違って、深度は目に見えない。絵にして初めて、バッファの中身が確かめられる
// Show the depth buffer on screen as a grayscale image. Near pixels are black and far pixels are white. Pixels with nothing drawn (1.0) are pure white.
// Unlike color, depth is not visible. Turning it into an image lets us inspect the buffer.
void draw_depth_buffer(Canvas& canvas, const std::vector<float>& depth_buffer) {
    // 何かが塗られたピクセルの、一番手前と一番奥の深度を探す
    // Find the nearest and farthest depth among pixels that have something drawn.
    float nearest = 1.0f;
    float farthest = 0.0f;
    for (int i = 0; i < canvas.width * canvas.height; i++) {
        if (depth_buffer[i] < 1.0f) {
            nearest = std::min(nearest, depth_buffer[i]);
            farthest = std::max(farthest, depth_buffer[i]);
        }
    }

    // 深度は 1 の近くに詰まる。ニア 0.1・ファー 100 だと、この場面の立方体は、どれも 0.98〜0.99 ほどに収まる
    // そのまま 0〜255 の灰色にすると、ほぼ真っ白で見分けがつかない。見つけた範囲だけを、黒〜灰色に引き伸ばして見る
    // 深度がなぜ詰まるのかは、のちの番外編で（Zファイティングと深度の精度）
    // 一番奥の灰色は、背景の真っ白と見分けがつくように、255 ではなく 200 で止める
    // Depth values cluster near 1. With a near plane of 0.1 and a far plane of 100, every cube in this scene has a depth around 0.98 to 0.99.
    // Mapping this range directly to grayscale from 0 to 255 makes it almost all white and hard to read. Stretch the observed range from black to gray instead.
    // A later extra episode explains why depth values cluster (Z-Fighting and Depth Precision).
    // Stop the farthest gray at 200 instead of 255 so it is easy to tell from the pure white background.
    const float range = farthest - nearest;
    for (int y = 0; y < canvas.height; y++) {
        for (int x = 0; x < canvas.width; x++) {
            const float depth = depth_buffer[y * canvas.width + x];
            uint8_t gray = 255;
            if (depth < 1.0f) {
                const float t = (range > 0.0f) ? (depth - nearest) / range : 0.0f;
                gray = static_cast<uint8_t>(t * 200.0f);
            }
            set_pixel(canvas, x, y, Color{gray, gray, gray});
        }
    }
}

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

    // 深度バッファ。画面のピクセル1つにつき、深度を1つ覚えておく。canvas.pixels と同じ数・同じ並び
    // 1.0 は「一番奥」。どんなものを塗っても、これより手前になる
    // Depth buffer. Store one depth value per screen pixel, with the same count and order as canvas.pixels.
    // 1.0 means "farthest back." Every object we draw is closer than that.
    std::vector<float> depth_buffer(canvas.width * canvas.height, 1.0f);

    // true にすると、色の絵の代わりに、深度バッファの中身を白黒で映す
    // When true, show the depth buffer as a grayscale image instead of the color image.
    const bool show_depth = false;

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

        // 深度バッファも、毎フレーム「一番奥」で埋め直す。画面を消すなら、深度も一緒に消す
        // Fill the depth buffer with "farthest back" every frame too. When clearing the screen, clear depth as well.
        std::fill(depth_buffer.begin(), depth_buffer.end(), 1.0f);

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

        // 右から順に、立方体自身の動き、世界の傾きと押し出し、透視投影
        // 深度バッファがあるので、描く順番を入れ替えても、絵は変わらない
        // From right to left: the cube's own motion, the world rotation and translation, and perspective projection.
        // The depth buffer keeps the image the same even if we change the drawing order.
        draw_cube(canvas, depth_buffer, projection * view * sun, yellow);
        draw_cube(canvas, depth_buffer, projection * view * earth, blue);
        draw_cube(canvas, depth_buffer, projection * view * moon, gray);

        if (show_depth) {
            draw_depth_buffer(canvas, depth_buffer);
        }

        canvas.present();
    }
}
