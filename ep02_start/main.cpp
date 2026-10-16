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

    while (canvas.update()) {
        // 毎フレーム、背景を暗い青で塗り直す。前のフレームの絵を消すため
        const Color background = {16, 16, 48};
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                set_pixel(canvas, x, y, background);
            }
        }

        // ここに線を引くコードを書く（draw_line は、この回でこれから作る）

        canvas.present();
    }
}
