#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "framework/canvas.h"

// 画面上の位置（ピクセル）
struct Point {
    int x;
    int y;
};

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

// 時計の針の先の位置を求める。degrees は 12時を 0 度として、時計回りに増える角度
// この回では中身を気にしなくてよい。回す話は #05 でくわしく見る
Point clock_hand_tip(int center_x, int center_y, float length, float degrees) {
    // sin と cos はラジアンで受け取るので、度から変換する
    const float radians = degrees * std::numbers::pi_v<float> / 180.0f;

    // 画面の y は下向きなので、12時（上）へ進むには y を引く
    const float tip_x = static_cast<float>(center_x) + length * std::sin(radians);
    const float tip_y = static_cast<float>(center_y) - length * std::cos(radians);

    return Point{static_cast<int>(std::round(tip_x)), static_cast<int>(std::round(tip_y))};
}

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    const int center_x = 160;
    const int center_y = 90;

    while (canvas.update()) {
        // 毎フレーム、背景を暗い青で塗り直す。前のフレームの絵を消すため
        const Color background = {16, 16, 48};
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // 花火。画面の真ん中から、画面のふちの点へ、20ピクセルおきに線を引く
        // sin と cos を使わないので、線の引き方だけを確かめられる
        const Color orange = {176, 104, 32};
        const int last_x = canvas.width - 1;
        const int last_y = canvas.height - 1;
        for (int x = 0; x <= last_x; x += 20) {
            draw_line(canvas, center_x, center_y, x, 0, orange);       // 上のふち
            draw_line(canvas, center_x, center_y, x, last_y, orange);  // 下のふち
        }
        for (int y = 0; y <= last_y; y += 20) {
            draw_line(canvas, center_x, center_y, 0, y, orange);       // 左のふち
            draw_line(canvas, center_x, center_y, last_x, y, orange);  // 右のふち
        }

        // 時計の針。canvas.time（秒）に 90 を掛けて、1秒で90度、4秒で1周にする
        // 本物の時計は1秒で6度だが、それでは遅くて見ていられない
        const float degrees = canvas.time * 90.0f;
        const Point tip = clock_hand_tip(center_x, center_y, 80.0f, degrees);
        const Color white = {255, 255, 255};
        draw_line(canvas, center_x, center_y, tip.x, tip.y, white);

        canvas.present();
    }
}
