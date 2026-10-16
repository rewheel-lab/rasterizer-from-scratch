#include "framework/canvas.h"

// ここに set_pixel を書く

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    while (canvas.update()) {
        // ここに描く処理を書く
        canvas.present();
    }
}
