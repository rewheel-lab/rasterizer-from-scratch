#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "framework/canvas.h"

// 画面上の位置（ピクセル） / Position on the screen, in pixels.
struct Point {
    int x;
    int y;
};

// (x, y) のピクセルを color にする / Set the pixel at (x, y) to color.
void set_pixel(Canvas& canvas, int x, int y, Color color) {
    // 画面の外は何もしない。はみ出して pixels の外を書くと、プログラムが壊れるため / Skip off-screen points. Writing outside pixels breaks the program.
    if (x < 0 || x >= canvas.width || y < 0 || y >= canvas.height) {
        return;
    }
    canvas.pixels[y * canvas.width + x] = color;
}

// (x0, y0) から (x1, y1) まで、1ピクセルずつ進みながら線を引く / Draw a line from (x0, y0) to (x1, y1), one pixel at a time.
void draw_line(Canvas& canvas, int x0, int y0, int x1, int y1, Color color) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;

    // x と y のうち長いほうの軸で数える。短いほうは1歩で1マス以下しか動かないので、間が空かない / Count along the longer axis. The shorter one moves at most 1 pixel per step: no gaps.
    const int steps = std::max(std::abs(dx), std::abs(dy));

    // 始点と終点が同じ場所だと steps が 0 になり、下の割り算ができない。点を1つ打って終わる / If start equals end, steps is 0 and division fails. Plot one pixel and stop.
    if (steps == 0) {
        set_pixel(canvas, x0, y0, color);
        return;
    }

    for (int i = 0; i <= steps; i++) {
        // t は 0（始点）から 1（終点）まで進む割合 / t is the ratio that goes from 0 (start) to 1 (end).
        const float t = static_cast<float>(i) / static_cast<float>(steps);

        // 切り捨て（static_cast<int> だけ）だと、いつも 0 の向きに削れて線が片側にずれる。最も近いピクセルに丸める / Truncating cuts toward 0 and skews the line. Round to the nearest pixel.
        const int x = x0 + static_cast<int>(std::round(static_cast<float>(dx) * t));
        const int y = y0 + static_cast<int>(std::round(static_cast<float>(dy) * t));
        set_pixel(canvas, x, y, color);
    }
}

// 時計の針の先の位置を求める。degrees は 12時を 0 度として、時計回りに増える角度 / Find the hand tip. degrees is 0 at 12 o'clock and grows clockwise.
// この回では中身を気にしなくてよい。回す話は #05 でくわしく見る / You do not need to read the inside now. We cover rotation in #05.
Point clock_hand_tip(int center_x, int center_y, float length, float degrees) {
    // sin と cos はラジアンで受け取るので、度から変換する / sin and cos take radians, so convert from degrees.
    const float radians = degrees * std::numbers::pi_v<float> / 180.0f;

    // 画面の y は下向きなので、12時（上）へ進むには y を引く / Screen y points down, so subtract y to go toward 12 o'clock (up).
    const float tip_x = static_cast<float>(center_x) + length * std::sin(radians);
    const float tip_y = static_cast<float>(center_y) - length * std::cos(radians);

    return Point{static_cast<int>(std::round(tip_x)), static_cast<int>(std::round(tip_y))};
}

// ここに、この回で書くものを足していく / Add what we write in this episode here.
//   Vec2           … x と y の2つの小数をまとめた構造体 / Vec2: a struct that groups two floats, x and y.
//   edge_function  … 点が線の右にあるか左にあるかを、数の符号で教える関数 / edge_function: the sign says if a point is right or left of a line.
//   fill_triangle  … 三角形の中のピクセルを塗る関数 / fill_triangle: fills the pixels inside a triangle.

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する / Draw at 320x180 and show it 4x larger.

    const int center_x = 160;
    const int center_y = 90;

    while (canvas.update()) {
        // 毎フレーム、背景を暗い青で塗り直す。前のフレームの絵を消すため / Clear the background to dark blue every frame to erase the last frame.
        const Color background = {16, 16, 48};
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // 花火。画面の真ん中から、画面のふちの点へ、20ピクセルおきに線を引く / Fireworks: lines from the center to the screen edge, every 20 pixels.
        // sin と cos を使わないので、線の引き方だけを確かめられる / It does not use sin or cos, so we can check only how lines are drawn.
        const Color orange = {176, 104, 32};
        const int last_x = canvas.width - 1;
        const int last_y = canvas.height - 1;
        for (int x = 0; x <= last_x; x += 20) {
            draw_line(canvas, center_x, center_y, x, 0, orange);       // 上のふち / Top edge.
            draw_line(canvas, center_x, center_y, x, last_y, orange);  // 下のふち / Bottom edge.
        }
        for (int y = 0; y <= last_y; y += 20) {
            draw_line(canvas, center_x, center_y, 0, y, orange);       // 左のふち / Left edge.
            draw_line(canvas, center_x, center_y, last_x, y, orange);  // 右のふち / Right edge.
        }

        // 時計の針。canvas.time（秒）に 90 を掛けて、1秒で90度、4秒で1周にする / Clock hand: canvas.time * 90 = 90 degrees/s, one turn in 4 s.
        // 本物の時計は1秒で6度だが、それでは遅くて見ていられない / A real clock turns 6 degrees per second, but that is too slow to watch.
        const float degrees = canvas.time * 90.0f;
        const Point tip = clock_hand_tip(center_x, center_y, 80.0f, degrees);
        const Color white = {255, 255, 255};
        draw_line(canvas, center_x, center_y, tip.x, tip.y, white);

        // ここで fill_triangle を呼んで、三角形を描く / Call fill_triangle here to draw triangles.
        // 前回の花火と時計は、三角形を描くときは消してよい / You can remove the last episode's fireworks and clock when drawing triangles.

        canvas.present();
    }
}
