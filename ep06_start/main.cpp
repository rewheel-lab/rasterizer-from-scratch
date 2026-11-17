#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

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

// 原点 (0, 0) のまわりに、点 p を angle だけ回す。angle はラジアン
// 画面の y は下向きなので、angle を増やすと時計回りに回る
// Rotate point p around the origin (0, 0) by angle. angle is in radians.
// Screen y points down, so increasing angle turns clockwise.
Vec2 rotate(Vec2 p, float angle) {
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    return Vec2{p.x * c - p.y * s, p.x * s + p.y * c};
}

// 回す中心を (0, 0) とした点 p を回してから、画面の真ん中に置く
// 回す中心を (0, 0) にしておくと、式がかんたんになる
// Rotate point p around (0, 0), then move it to the center of the screen.
// Keeping the rotation center at (0, 0) makes the formula simple.
Vec2 place(Vec2 p, float angle) {
    const Vec2 r = rotate(p, angle);
    return Vec2{r.x + 160.0f, r.y + 90.0f};
}

// ここに、この回で書くものを足していく
//   rotate_y, rotate_x … 3D の点を、y 軸と x 軸のまわりに回す関数
//   project            … 3D の点を、画面の位置に写す関数
// 上の rotate と place は、この回の 3D の回転に置き換わるので、使わなくなる
// Add what we write in this episode here.
// rotate_y, rotate_x ... Functions that rotate a 3D point around the y and x axes.
// project ... A function that projects a 3D point to a screen position.
// The rotate and place functions above are replaced by 3D rotation in this episode, so we no longer need them.

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する / Display a 320x180 image enlarged 4 times.

    // カラーコードで書いた、頂点の色
    // Vertex color written as a color code.
    const Color red = from_hex(0xFF0000);
    const Color green = from_hex(0x00FF00);
    const Color blue = from_hex(0x0000FF);

    // この回で、ここに立方体の8つの角と、12本の辺を足す。三角形の頂点は使わなくなる
    // 三角形の頂点。回す中心を (0, 0) として持つ。上 → 右下 → 左下の順で、画面の上で時計回り
    // In this episode, add the cube's eight corners and 12 edges here. Triangle vertices are no longer used.
    // Triangle vertices. Keep their rotation center at (0, 0). Order them top, bottom right, bottom left, clockwise on screen.
    const Vec2 top = {0.0f, -60.0f};
    const Vec2 right_bottom = {52.0f, 30.0f};
    const Vec2 left_bottom = {-52.0f, 30.0f};

    // 1秒あたりに回す角度（度）。マイナスにすると逆回り
    // Rotation speed in degrees per second. Use a negative value to turn the other way.
    const float speed = 90.0f;  
    float angle = 0.0f;         // 今の角度（度） / Current angle in degrees.

    while (canvas.update()) {
        // 1フレームごとに回す量は、1秒あたりの量に、前のフレームからの秒数を掛けて決める
        // こうすると、描く回数が多い PC でも少ない PC でも、1秒で回る量は同じになる
        // Multiply the per-second rotation amount by the seconds since the previous frame to get the rotation for this frame.
        // This keeps the rotation per second the same on fast and slow PCs, even when they draw a different number of frames.
        angle += speed * canvas.delta_time;

        // 毎フレーム、背景を暗い青で塗り直す。前のフレームの絵を消すため
        // Clear the background to dark blue each frame to erase the previous image.
        const Color background = from_hex(0x101030);
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // 角度は度で持っているので、sin と cos に渡す前にラジアンへ直す
        // The angle is stored in degrees, so convert it to radians before passing it to sin and cos.
        const float radians = to_radians(angle);

        // この回で、ここを「回る立方体」に差し替える
        // 回転では頂点の並び順が変わらないので、時計回りのまま。fill_triangle に捨てられない
        // Replace this with the rotating cube in this episode.
        // Rotation does not change the vertex order, so the triangle stays clockwise and fill_triangle does not discard it.
        fill_triangle(canvas, place(top, radians), place(right_bottom, radians), place(left_bottom, radians), red, green, blue);

        canvas.present();
    }
}
