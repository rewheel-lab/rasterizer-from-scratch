#include "framework/canvas.h"

// ここに set_pixel を書く / Write set_pixel here.

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する / Draw at 320x180 and show it 4x larger.

    while (canvas.update()) {
        // ここに描く処理を書く / Write the drawing code here.
        canvas.present();
    }
}
