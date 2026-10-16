#include <cstdint>

#include "framework/canvas.h"

// (x, y) のピクセルを color にする
void set_pixel(Canvas& canvas, int x, int y, Color color) {
    // 画面の外は何もしない。はみ出して pixels の外を書くと、プログラムが壊れるため
    if (x < 0 || x >= canvas.width || y < 0 || y >= canvas.height) {
        return;
    }
    canvas.pixels[y * canvas.width + x] = color;
}

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    while (canvas.update()) {
        // 画面全体を塗る。赤は x に、緑は y に合わせて 0〜255 に変える
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                const uint8_t r = static_cast<uint8_t>(x * 255 / (canvas.width - 1));
                const uint8_t g = static_cast<uint8_t>(y * 255 / (canvas.height - 1));
                set_pixel(canvas, x, y, Color{r, g, 0});
            }
        }

        // x = 100 に白い縦線を引く。y は 0 〜 height - 1 なので、< で止める
        const Color white = {255, 255, 255};
        for (int y = 0; y < canvas.height; y++) {
            set_pixel(canvas, 100, y, white);
        }

        canvas.present();
    }
}
