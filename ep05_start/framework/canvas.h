#pragma once
#include <cstdint>
#include <vector>

// 画面に出す1ピクセルの色。0〜255
struct Color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct Canvas {
    int width;                  // 描く絵の幅（ピクセル）
    int height;                 // 描く絵の高さ（ピクセル）
    std::vector<Color> pixels;  // 左上から右へ、1行ずつ並ぶ。(x, y) は pixels[y * width + x]

    float time = 0.0f;          // 起動してからの秒数
    float delta_time = 0.0f;    // 前のフレームからの秒数

    Canvas(int width, int height, int scale);  // scale 倍に拡大して表示する
    ~Canvas();

    // 窓のハンドルを二重に閉じないよう、コピーは禁止する
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    bool update();              // 窓のメッセージを処理する。窓が閉じられたら false
    void present();             // pixels を画面に映す

    bool is_key_down(int key);  // キーが押されているか。英字は 'W' のように大文字で、数字は '1' のように渡す
    bool is_mouse_down(int button);// 0 が左、1 が右
    float mouse_dx = 0.0f;      // 前のフレームからのマウスの移動量（ピクセル）
    float mouse_dy = 0.0f;

    // 内部で使う（触らない）
    void* window = nullptr;
    int scale;
    bool is_first_update = true;
    long long start_ticks = 0;
    long long last_ticks = 0;
    int last_cursor_x = 0;
    int last_cursor_y = 0;
    std::vector<uint32_t> screen;  // 画面に送るときの色。Windows が求める B, G, R の順に並べ替えたもの
};

// 英字・数字以外のキー。値は Windows の仮想キーコードと同じ
const int key_space = 0x20;
const int key_shift = 0x10;
const int key_escape = 0x1B;
const int key_left = 0x25;
const int key_right = 0x27;
const int key_up = 0x26;
const int key_down = 0x28;
