#include <cmath>
#include <cstdint>

#include "framework/canvas.h"

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    // 改造チャレンジ：この数字を変えて F5 を押すと、色が変わる
    const float color_shift = 2.0f;

    // 色が流れる速さ
    const float speed = 1.0f;

    while (canvas.update()) {
        for (int y = 0; y < canvas.height; y++) {
            for (int x = 0; x < canvas.width; x++) {
                // 位置と時間から、波の位相を決める。時間が進むと、模様が流れる
                const float phase = static_cast<float>(x + y) * 0.02f + canvas.time * speed;

                // sin は -1〜1 なので、0〜1 にしてから 0〜255 に直す
                const float r = std::sin(phase) * 0.5f + 0.5f;
                const float g = std::sin(phase + color_shift) * 0.5f + 0.5f;
                const float b = std::sin(phase + color_shift * 2.0f) * 0.5f + 0.5f;

                canvas.pixels[y * canvas.width + x] = Color{
                    static_cast<uint8_t>(r * 255.0f),
                    static_cast<uint8_t>(g * 255.0f),
                    static_cast<uint8_t>(b * 255.0f),
                };
            }
        }
        canvas.present();
    }
}
