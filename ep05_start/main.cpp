#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "framework/canvas.h"

// 画面上の位置。ピクセルの間の位置も表せるように、小数で持つ
struct Vec2 {
    float x;
    float y;
};

// 0xRRGGBB の形の数を Color にする。お絵かきソフトのカラーコードと同じ並びで、左の2桁が赤、真ん中が緑、右の2桁が青
Color from_hex(uint32_t hex) {
    const uint8_t r = static_cast<uint8_t>((hex >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((hex >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(hex & 0xFF);
    return Color{r, g, b};
}

// (x, y) のピクセルを color にする
void set_pixel(Canvas& canvas, int x, int y, Color color) {
    // 画面の外は何もしない。はみ出して pixels の外を書くと、プログラムが壊れるため
    if (x < 0 || x >= canvas.width || y < 0 || y >= canvas.height) {
        return;
    }
    canvas.pixels[y * canvas.width + x] = color;
}

// (x0, y0) から (x1, y1) まで、1ピクセルずつ進みながら線を引く
void draw_line(Canvas& canvas, int x0, int y0, int x1, int y1, Color color) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;

    // x と y のうち長いほうの軸で数える。短いほうは1歩で1マス以下しか動かないので、間が空かない
    const int steps = std::max(std::abs(dx), std::abs(dy));

    // 始点と終点が同じ場所だと steps が 0 になり、下の割り算ができない。点を1つ打って終わる
    if (steps == 0) {
        set_pixel(canvas, x0, y0, color);
        return;
    }

    for (int i = 0; i <= steps; i++) {
        // t は 0（始点）から 1（終点）まで進む割合
        const float t = static_cast<float>(i) / static_cast<float>(steps);

        // 切り捨て（static_cast<int> だけ）だと、いつも 0 の向きに削れて線が片側にずれる。最も近いピクセルに丸める
        const int x = x0 + static_cast<int>(std::round(static_cast<float>(dx) * t));
        const int y = y0 + static_cast<int>(std::round(static_cast<float>(dy) * t));
        set_pixel(canvas, x, y, color);
    }
}

// a から b へ向かう線から見て、p が右にあればプラス、左にあればマイナス、線の上なら 0 を返す
// 右と左は、画面に映った向きのまま。画面の y は下向きなので、数学の教科書の図とは逆に見える
// 値の大きさは、a, b, p を頂点とする三角形の面積の2倍になっている
float edge_function(Vec2 a, Vec2 b, Vec2 p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// 三角形の辺 a → b が、上の辺か左の辺かを返す
// 頂点が画面の上で時計回りなら、三角形の中は、進む向きから見て右側にある
// 右へ進む水平な辺は、中が下にあるので上の辺。上へ進む辺は、中が右にあるので左の辺
bool is_top_left(Vec2 a, Vec2 b) {
    const bool is_top = (a.y == b.y) && (b.x > a.x);
    const bool is_left = b.y < a.y;
    return is_top || is_left;
}

// 辺の値 e が、三角形の内側かを返す
// 0 より大きければ中。ちょうど 0（線の上）は、上の辺と左の辺の上だけ自分のものにする
// 隣の三角形と共有する辺は、片方では左（か上）、もう片方では右（か下）になるので、ちょうど1回だけ塗られる
bool covers(float e, bool top_left) {
    if (e > 0.0f) {
        return true;
    }
    return e == 0.0f && top_left;
}

// 3つの色を、割合 w0, w1, w2 で混ぜる。w0 が a の割合
Color mix_color(Color color_a, Color color_b, Color color_c, float w0, float w1, float w2) {
    const float r = static_cast<float>(color_a.r) * w0 + static_cast<float>(color_b.r) * w1 + static_cast<float>(color_c.r) * w2;
    const float g = static_cast<float>(color_a.g) * w0 + static_cast<float>(color_b.g) * w1 + static_cast<float>(color_c.g) * w2;
    const float b = static_cast<float>(color_a.b) * w0 + static_cast<float>(color_b.b) * w1 + static_cast<float>(color_c.b) * w2;

    // 0.5 を足してから切り捨てると、四捨五入になる。切り捨てだけでは、全体が少し暗くなる
    return Color{
        static_cast<uint8_t>(r + 0.5f),
        static_cast<uint8_t>(g + 0.5f),
        static_cast<uint8_t>(b + 0.5f),
    };
}

// a, b, c を頂点とする三角形の中を塗る。色は頂点ごとに決め、間をなめらかに混ぜる
// 頂点は、画面の上で時計回りに並べる。反時計回りだと面積がマイナスになり、何も塗られない
void fill_triangle(Canvas& canvas, Vec2 a, Vec2 b, Vec2 c, Color color_a, Color color_b, Color color_c) {
    // 三角形全体の面積の2倍。各辺の値をこれで割ると、頂点ごとの割合になる
    // 0 のときは割れないので、塗らずに終わる
    const float area = edge_function(a, b, c);
    if (area <= 0.0f) {
        return;
    }

    // 三角形をぎりぎり囲む四角形を求める。この四角形の中のピクセルだけ調べれば足りる
    // 画面の外まで調べても塗れない（set_pixel が捨てる）ので、範囲を画面の中に切り詰める
    // 大きく外へはみ出した三角形で、無駄な繰り返しをしないため
    // 小数のまま切り詰めてから整数にするのは、とても大きな値を int にすると壊れるため
    const float last_x = static_cast<float>(canvas.width - 1);
    const float last_y = static_cast<float>(canvas.height - 1);
    const float box_min_x = std::clamp(std::min(a.x, std::min(b.x, c.x)), 0.0f, last_x);
    const float box_max_x = std::clamp(std::max(a.x, std::max(b.x, c.x)), 0.0f, last_x);
    const float box_min_y = std::clamp(std::min(a.y, std::min(b.y, c.y)), 0.0f, last_y);
    const float box_max_y = std::clamp(std::max(a.y, std::max(b.y, c.y)), 0.0f, last_y);

    // 小数の範囲を、外へ広げて整数にする。ぎりぎり内側のピクセルを取りこぼさないため
    const int min_x = static_cast<int>(std::floor(box_min_x));
    const int max_x = static_cast<int>(std::ceil(box_max_x));
    const int min_y = static_cast<int>(std::floor(box_min_y));
    const int max_y = static_cast<int>(std::ceil(box_max_y));

    // 3本の辺が、上の辺か左の辺か。三角形ごとに1回決めれば足りる
    const bool top_left0 = is_top_left(b, c);
    const bool top_left1 = is_top_left(c, a);
    const bool top_left2 = is_top_left(a, b);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            // ピクセルは点ではなく小さなマス。マスを代表する点は、真ん中（+0.5）にする
            // 整数の位置のままだと、整数の位置に置いた辺の上に、判定の点がいつも乗ってしまう
            const Vec2 p = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};

            // 3本の辺それぞれについて、p が右側にいるか調べる
            const float e0 = edge_function(b, c, p);
            const float e1 = edge_function(c, a, p);
            const float e2 = edge_function(a, b, p);

            if (covers(e0, top_left0) && covers(e1, top_left1) && covers(e2, top_left2)) {
                // 重心座標。辺の値を面積で割ると、合計がちょうど 1 になる3つの数になる
                // w0 は a の割合。a に近いほど、a の向かい側の辺 b → c が作る三角形が大きくなるため
                const float w0 = e0 / area;
                const float w1 = e1 / area;
                const float w2 = e2 / area;
                set_pixel(canvas, x, y, mix_color(color_a, color_b, color_c, w0, w1, w2));
            }
        }
    }
}

// ここに、この回で書くものを足していく
//   to_radians … 度をラジアンに直す関数
//   rotate     … 点を原点のまわりに回す関数（sin と cos を使う）
//   place      … 回してから、画面の真ん中に置く関数

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    // カラーコードで書いた、頂点の色
    const Color red = from_hex(0xFF0000);
    const Color green = from_hex(0x00FF00);
    const Color blue = from_hex(0x0000FF);
    const Color yellow = from_hex(0xFFFF00);

    // この回で、ここに「回す速さ」と「今の角度」の変数を足す

    while (canvas.update()) {
        // この回で、ここで角度を進める。1フレームの時間は canvas.delta_time
        // 毎フレーム、背景を暗い青で塗り直す。前のフレームの絵を消すため
        const Color background = from_hex(0x101030);
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // この回で、ここを「画面の真ん中で回る虹色の三角形」だけに差し替える
        // 頂点は、回す中心を (0, 0) とした位置で持ち、回してから画面の真ん中に置く

        // どの三角形も、頂点を画面の上で時計回りに並べる

        // 虹色の三角形。上 → 右下 → 左下の順
        fill_triangle(canvas, Vec2{220.0f, 30.0f}, Vec2{290.0f, 140.0f}, Vec2{150.0f, 140.0f}, red, green, blue);

        // 四角形は、三角形2つでできている。対角線は、左上と右下を結ぶ
        // 左上、右上、右下、左下の色は、赤、緑、青、黄色
        const Vec2 left_top = {40.0f, 40.0f};
        const Vec2 right_top = {120.0f, 40.0f};
        const Vec2 right_bottom = {120.0f, 120.0f};
        const Vec2 left_bottom = {40.0f, 120.0f};
        fill_triangle(canvas, left_top, right_top, right_bottom, red, green, blue);
        fill_triangle(canvas, left_top, right_bottom, left_bottom, red, blue, yellow);

        canvas.present();
    }
}
