#include <algorithm>
#include <cmath>
#include <cstdint>

#include "framework/canvas.h"

// 画面上の位置。ピクセルの間の位置も表せるように、小数で持つ
struct Vec2 {
    float x;
    float y;
};

// ここに、この回で書くものを足していく
//   from_hex       … 0xRRGGBB の形の数（カラーコード）を Color にする関数
//   is_top_left, covers … 辺の上ぴったりのピクセルを、どちらの三角形のものにするかを決める関数
//   mix_color      … 3つの色を、割合を決めて混ぜる関数

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
float edge_function(Vec2 a, Vec2 b, Vec2 p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// a, b, c を頂点とする三角形の中を color で塗る
// この回で、頂点ごとに色を持たせて、間を混ぜる形に作り変える
// 頂点は、画面の上で時計回りに並べる。反時計回りだと、中が3本の線の左側になり、何も塗られない
void fill_triangle(Canvas& canvas, Vec2 a, Vec2 b, Vec2 c, Color color) {
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

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            // 判定する点は、ピクセルの番号そのもの。この回で、ピクセルの真ん中（+0.5）に直す
            const Vec2 p = {static_cast<float>(x), static_cast<float>(y)};

            // 3本の辺それぞれについて、p が右側にいるか調べる
            const float w0 = edge_function(b, c, p);
            const float w1 = edge_function(c, a, p);
            const float w2 = edge_function(a, b, p);

            // 3本とも右側なら、三角形の中。ちょうど 0（線の上）は、中に入れない
            // この回で、辺の値を面積で割って頂点の色を混ぜ、線の上の扱いも直す
            if (w0 > 0.0f && w1 > 0.0f && w2 > 0.0f) {
                set_pixel(canvas, x, y, color);
            }
        }
    }
}

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    while (canvas.update()) {
        // 毎フレーム、背景を暗い青で塗り直す。前のフレームの絵を消すため
        const Color background = {16, 16, 48};
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // この回で、ここを虹色の三角形と、三角形2つの四角形に差し替える

        // どの三角形も、頂点を画面の上で時計回りに並べる

        // 真ん中の大きな三角形。上 → 右下 → 左下の順
        fill_triangle(canvas, Vec2{160.0f, 20.0f}, Vec2{260.0f, 160.0f}, Vec2{60.0f, 160.0f}, Color{96, 176, 64});

        // まわりの小さな三角形
        fill_triangle(canvas, Vec2{30.0f, 30.0f}, Vec2{90.0f, 50.0f}, Vec2{20.0f, 90.0f}, Color{224, 128, 32});
        fill_triangle(canvas, Vec2{230.0f, 15.0f}, Vec2{300.0f, 40.0f}, Vec2{250.0f, 80.0f}, Color{224, 80, 144});
        fill_triangle(canvas, Vec2{10.0f, 120.0f}, Vec2{70.0f, 150.0f}, Vec2{15.0f, 170.0f}, Color{64, 128, 224});
        fill_triangle(canvas, Vec2{270.0f, 110.0f}, Vec2{310.0f, 170.0f}, Vec2{230.0f, 175.0f}, Color{240, 208, 64});

        // 大きな三角形の上に重ねる三角形。あとから描いたものが上になる
        fill_triangle(canvas, Vec2{150.0f, 80.0f}, Vec2{190.0f, 130.0f}, Vec2{120.0f, 130.0f}, Color{240, 240, 240});

        canvas.present();
    }
}
