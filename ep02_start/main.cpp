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

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する / Draw at 320x180 and show it 4x larger.

    while (canvas.update()) {
        // 毎フレーム、背景を暗い青で塗り直す。前のフレームの絵を消すため / Clear the background to dark blue every frame to erase the last frame.
        const Color background = {16, 16, 48};
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // ここに線を引くコードを書く（draw_line は、この回でこれから作る） / Write the line-drawing code here (we make draw_line in this episode).

        canvas.present();
    }
}
