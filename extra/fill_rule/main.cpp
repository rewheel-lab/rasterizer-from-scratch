// 番外編「塗りのルール」の実験用プログラム
// ラスタライザー編 #04 の三角形の塗り方に、ルールの切り替えと、拡大表示を足したもの
//
// 操作
//   1〜5 … 絵を選ぶ（1: 顔、2: 幅 15 の板、3: メッシュ、4: 扇、5: 整数の位置の四角形）
//   T    … 左上ルール（規格どおり）
//   B    … 右下ルール（逆向き）
//   G    … 「> 0」だけを塗る（辺の上ぴったりは、どの三角形も塗らない）
//   E    … 「>= 0」を塗る（辺の上ぴったりは、どの三角形も塗る）
//   V    … 表示の切り替え（ひとつ → 左右に並べる → 違いを赤で表示）
//   Z, X … 拡大、縮小
//   矢印 … 表示する場所を動かす
//   C    … ピクセルの境目と、ピクセルの中心の印を出す（拡大が 8 倍以上のとき）
//   W    … 三角形のふちの線を重ねる
//   A    … 塗った回数を足し算で表示する（二重に塗られたピクセルが明るくなる）
//   Esc  … 終わる

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <numbers>
#include <utility>
#include <vector>

#include "framework/canvas.h"

// 画面上の位置。ピクセルの間の位置も表せるように、小数で持つ
struct Vec2 {
    float x;
    float y;
};

struct Triangle {
    Vec2 a;
    Vec2 b;
    Vec2 c;
    Color color;
};

// 絵を描く先。Canvas と同じ並びのピクセルに加えて、各ピクセルを何回塗ったかを覚える
// 回数は、ルールの違いや、二重塗り・すき間を調べるために使う
struct Image {
    int width;
    int height;
    std::vector<Color> pixels;
    std::vector<int> counts;
};

// 辺の上ぴったりのピクセルを、どう扱うか
enum class FillRule {
    top_left,          // 上の辺と左の辺の上だけ塗る（Direct3D の規格）
    bottom_right,      // 下の辺と右の辺の上だけ塗る（逆向き）
    greater,           // 辺の上は、どの三角形も塗らない（> 0）
    greater_or_equal,  // 辺の上は、どの三角形も塗る（>= 0）
};

// 絵の大きさ。拡大して見るので、小さくしてある
const int scene_width = 160;
const int scene_height = 90;
const int scene_count = 5;

// 表示する範囲。canvas のどこに、絵のどこを、何倍で出すか
struct View {
    int dest_x;  // canvas の上の位置と大きさ
    int dest_y;
    int dest_w;
    int dest_h;
    int view_x;  // dest の左上に出す、絵のピクセル
    int view_y;
    int zoom;
};

// 0xRRGGBB の形の数を Color にする
Color from_hex(uint32_t hex) {
    const uint8_t r = static_cast<uint8_t>((hex >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((hex >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(hex & 0xFF);
    return Color{r, g, b};
}

// 色の明るさをずらす。三角形を見分けやすいように、隣どうしで少し色を変えるために使う
Color shade(Color color, int delta) {
    return Color{
        static_cast<uint8_t>(std::clamp(color.r + delta, 0, 255)),
        static_cast<uint8_t>(std::clamp(color.g + delta, 0, 255)),
        static_cast<uint8_t>(std::clamp(color.b + delta, 0, 255)),
    };
}

Image make_image(int width, int height) {
    const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height);
    return Image{width, height, std::vector<Color>(size, Color{0, 0, 0}), std::vector<int>(size, 0)};
}

void clear_image(Image& image, Color color) {
    std::fill(image.pixels.begin(), image.pixels.end(), color);
    std::fill(image.counts.begin(), image.counts.end(), 0);
}

// (x, y) のピクセルを color にする。塗った回数も 1 増やす
void set_pixel(Image& image, int x, int y, Color color) {
    if (x < 0 || x >= image.width || y < 0 || y >= image.height) {
        return;
    }
    image.pixels[y * image.width + x] = color;
    image.counts[y * image.width + x] += 1;
}

// (x, y) のピクセルに color を足す。255 を超える分は 255 で止める
// 二重に塗られたピクセルだけが、足し算で明るくなる
void add_pixel(Image& image, int x, int y, Color color) {
    if (x < 0 || x >= image.width || y < 0 || y >= image.height) {
        return;
    }
    Color& pixel = image.pixels[y * image.width + x];
    pixel.r = static_cast<uint8_t>(std::min(pixel.r + color.r, 255));
    pixel.g = static_cast<uint8_t>(std::min(pixel.g + color.g, 255));
    pixel.b = static_cast<uint8_t>(std::min(pixel.b + color.b, 255));
    image.counts[y * image.width + x] += 1;
}

// a から b へ向かう線から見て、p が右にあればプラス、左にあればマイナス、線の上なら 0 を返す
// 右と左は、画面に映った向きのまま。画面の y は下向きなので、数学の教科書の図とは逆に見える
float edge_function(Vec2 a, Vec2 b, Vec2 p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}

// 三角形の辺 a → b が、上の辺か左の辺かを返す
// 頂点が画面の上で時計回りなら、三角形の中は、進む向きから見て右側にある
// 右へ進む水平な辺は、中が下にあるので上の辺。上へ進む辺は、中が右にあるので左の辺
bool is_top_left(Vec2 a, Vec2 b) {
    const bool is_top = (a.y == b.y) && (b.x > a.x);
    const bool is_left = b.y < a.y;
    return is_top || is_left;
}

// 辺 a → b が、下の辺か右の辺かを返す。is_top_left の逆向き
// 水平な辺は、左へ進むと下の辺。下へ進む辺は、中が左にあるので右の辺
bool is_bottom_right(Vec2 a, Vec2 b) {
    const bool is_bottom = (a.y == b.y) && (b.x < a.x);
    const bool is_right = b.y > a.y;
    return is_bottom || is_right;
}

// 辺 a → b の上ぴったり（辺の値が 0）のピクセルを、この三角形のものにするか
bool owns_edge(FillRule rule, Vec2 a, Vec2 b) {
    switch (rule) {
        case FillRule::top_left:
            return is_top_left(a, b);
        case FillRule::bottom_right:
            return is_bottom_right(a, b);
        case FillRule::greater:
            return false;
        case FillRule::greater_or_equal:
            return true;
    }
    return false;
}

// 辺の値 e が、三角形の内側かを返す
// 0 より大きければ中。ちょうど 0（線の上）は、ルールで決まった辺の上だけ自分のものにする
bool covers(float e, bool owns) {
    if (e > 0.0f) {
        return true;
    }
    return e == 0.0f && owns;
}

// triangle の中を、rule に従って塗る。色は三角形ごとに1色
// additive が true のときは、塗る代わりに足す。二重に塗られた場所が明るくなる
void fill_triangle(Image& image, const Triangle& triangle, FillRule rule, bool additive) {
    const Vec2 a = triangle.a;
    const Vec2 b = triangle.b;
    const Vec2 c = triangle.c;

    // 頂点は、画面の上で時計回りに並べる。反時計回りだと面積がマイナスになり、何も塗られない
    const float area = edge_function(a, b, c);
    if (area <= 0.0f) {
        return;
    }

    // 三角形をぎりぎり囲む四角形を、画面の中に切り詰めて求める
    const float last_x = static_cast<float>(image.width - 1);
    const float last_y = static_cast<float>(image.height - 1);
    const float box_min_x = std::clamp(std::min(a.x, std::min(b.x, c.x)), 0.0f, last_x);
    const float box_max_x = std::clamp(std::max(a.x, std::max(b.x, c.x)), 0.0f, last_x);
    const float box_min_y = std::clamp(std::min(a.y, std::min(b.y, c.y)), 0.0f, last_y);
    const float box_max_y = std::clamp(std::max(a.y, std::max(b.y, c.y)), 0.0f, last_y);

    const int min_x = static_cast<int>(std::floor(box_min_x));
    const int max_x = static_cast<int>(std::ceil(box_max_x));
    const int min_y = static_cast<int>(std::floor(box_min_y));
    const int max_y = static_cast<int>(std::ceil(box_max_y));

    // 3本の辺の上ぴったりを、この三角形が塗るか。三角形ごとに1回決めれば足りる
    const bool owns0 = owns_edge(rule, b, c);
    const bool owns1 = owns_edge(rule, c, a);
    const bool owns2 = owns_edge(rule, a, b);

    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            // ピクセルを代表する点は、マスの真ん中（+0.5）にする
            const Vec2 p = {static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f};

            const float e0 = edge_function(b, c, p);
            const float e1 = edge_function(c, a, p);
            const float e2 = edge_function(a, b, p);

            if (covers(e0, owns0) && covers(e1, owns1) && covers(e2, owns2)) {
                if (additive) {
                    add_pixel(image, x, y, triangle.color);
                } else {
                    set_pixel(image, x, y, triangle.color);
                }
            }
        }
    }
}

// 絵を描き直す。背景を塗ってから、三角形を順に塗る
// additive のときは、どの三角形も同じ暗い灰色にして、足し算で塗る
// 1回塗られたところは暗い灰色、2回塗られたところは明るい灰色、抜けたところは背景のまま
void render_scene(Image& image, const std::vector<Triangle>& triangles, FillRule rule, bool additive) {
    clear_image(image, from_hex(0x101030));
    for (const Triangle& triangle : triangles) {
        Triangle t = triangle;
        if (additive) {
            t.color = from_hex(0x505050);
        }
        fill_triangle(image, t, rule, additive);
    }
}

// 三角形を追加する。画面の上で反時計回りだったら、b と c を入れ替えて時計回りにする
void add_triangle(std::vector<Triangle>& triangles, Vec2 a, Vec2 b, Vec2 c, Color color) {
    if (edge_function(a, b, c) < 0.0f) {
        std::swap(b, c);
    }
    triangles.push_back(Triangle{a, b, c, color});
}

// 四角形を、対角線で三角形2つに分けて追加する。頂点は左上、右上、右下、左下の順
void add_quad(std::vector<Triangle>& triangles, Vec2 left_top, Vec2 right_top, Vec2 right_bottom, Vec2 left_bottom, Color color_a, Color color_b) {
    add_triangle(triangles, left_top, right_top, right_bottom, color_a);
    add_triangle(triangles, left_top, right_bottom, left_bottom, color_b);
}

// 0.5 刻みの位置にそろえる
// 位置が 0.5 の倍数なら、辺の値の計算が小数のままでも誤差なしで済む
float snap_half(float value) {
    return std::round(value * 2.0f) / 2.0f;
}

// 絵 1：三角形でできた、ネコのような顔
// 中心から外へ、3つの輪を作って三角形で埋める。どの三角形も重ならず、すき間もない
void build_face(std::vector<Triangle>& triangles) {
    const int sectors = 16;
    const Vec2 center = {80.0f, 50.0f};
    const float radii[3] = {14.0f, 28.0f, 42.0f};
    const float squash = 0.85f;  // 縦を少しつぶして、横長の顔にする
    const float pi = std::numbers::pi_v<float>;

    // 輪の上の点。隣の三角形と共有する点は、ここで1回だけ計算して使い回す
    Vec2 ring[3][sectors];
    for (int k = 0; k < 3; k++) {
        for (int s = 0; s < sectors; s++) {
            const float angle = 2.0f * pi * static_cast<float>(s) / static_cast<float>(sectors);
            ring[k][s].x = snap_half(center.x + radii[k] * std::cos(angle));
            ring[k][s].y = snap_half(center.y + radii[k] * squash * std::sin(angle));
        }
    }

    const Color fur_light = from_hex(0xF2B35E);
    const Color fur_dark = from_hex(0xE0913A);
    const Color nose = from_hex(0xF06080);
    const Color eye = from_hex(0x203040);
    const Color mouth = from_hex(0x903030);
    const Color ear = from_hex(0xD07828);

    int n = 0;  // 何番目の三角形か。偶数と奇数で明るさを変える

    // 中心の扇。下の2枚を鼻にする
    for (int s = 0; s < sectors; s++) {
        const int next = (s + 1) % sectors;
        const Color color = (s == 3 || s == 4) ? nose : fur_light;
        add_triangle(triangles, center, ring[0][s], ring[0][next], shade(color, (n % 2 == 0) ? 0 : -14));
        n++;
    }

    // 輪と輪のあいだの帯。四角形を三角形2つに分ける
    for (int k = 0; k < 2; k++) {
        for (int s = 0; s < sectors; s++) {
            const int next = (s + 1) % sectors;
            Color color = (k == 0) ? fur_light : fur_dark;
            if (k == 0 && (s == 10 || s == 13)) {
                color = eye;  // 左右の上のほうの2マスを目にする
            }
            if (k == 1 && (s == 3 || s == 4)) {
                color = mouth;  // 鼻の下の2マスを口にする
            }
            add_triangle(triangles, ring[k][s], ring[k][next], ring[k + 1][next], shade(color, (n % 2 == 0) ? 0 : -14));
            n++;
            add_triangle(triangles, ring[k][s], ring[k + 1][next], ring[k + 1][s], shade(color, (n % 2 == 0) ? 0 : -14));
            n++;
        }
    }

    // 耳。外側の輪の辺を1本ずつ底にして、外へとがらせる
    const float ear_angle_left = 2.0f * pi * 10.5f / static_cast<float>(sectors);
    const float ear_angle_right = 2.0f * pi * 13.5f / static_cast<float>(sectors);
    const float ear_length = radii[2] * 1.45f;
    const Vec2 tip_left = {snap_half(center.x + ear_length * std::cos(ear_angle_left)), snap_half(center.y + ear_length * squash * std::sin(ear_angle_left))};
    const Vec2 tip_right = {snap_half(center.x + ear_length * std::cos(ear_angle_right)), snap_half(center.y + ear_length * squash * std::sin(ear_angle_right))};
    add_triangle(triangles, ring[2][10], tip_left, ring[2][11], ear);
    add_triangle(triangles, ring[2][13], tip_right, ring[2][14], ear);
}

// 絵 2：幅 15 の板。真ん中ぞろえで置くと、ふちが x.5 の位置に来る
// 絵の幅 160 の真ん中は 80。80 ± 7.5 で、72.5 と 87.5 になる
// 高さも 15 にして、縦のふちも .5 に置く（真ん中は 45）
void build_banner(std::vector<Triangle>& triangles) {
    const float left = static_cast<float>(scene_width) / 2.0f - 7.5f;
    const float right = static_cast<float>(scene_width) / 2.0f + 7.5f;
    const float top = static_cast<float>(scene_height) / 2.0f - 7.5f;
    const float bottom = static_cast<float>(scene_height) / 2.0f + 7.5f;
    add_quad(triangles, Vec2{left, top}, Vec2{right, top}, Vec2{right, bottom}, Vec2{left, bottom}, from_hex(0x40A0E0), from_hex(0xE0A040));
}

// 絵 3：頂点がすべてピクセルの中心（x.5, y.5）に乗っているメッシュ
// 8角形の宝石のような形。上下は水平、左右は垂直、角は 45 度で切り落としてある
// 中の頂点は、整数だけずらして、辺の向きをばらばらにする（ずらしても中心の位置のまま）
void build_lattice(std::vector<Triangle>& triangles) {
    const int columns = 9;
    const int rows = 6;
    const float spacing = 14.0f;
    const float bulge[rows] = {0.0f, 14.0f, 14.0f, 14.0f, 14.0f, 0.0f};  // 左右のふちを外へ出す量

    Vec2 grid[rows][columns];
    for (int j = 0; j < rows; j++) {
        for (int i = 0; i < columns; i++) {
            float x = 24.5f + spacing * static_cast<float>(i);
            float y = 8.5f + spacing * static_cast<float>(j);
            const bool inside = (i > 0 && i < columns - 1) && (j > 0 && j < rows - 1);
            if (inside) {
                x += static_cast<float>((i * 7 + j * 3) % 5 - 2);
                y += static_cast<float>((i * 5 + j * 11) % 5 - 2);
            }
            if (i == 0) {
                x -= bulge[j];
            }
            if (i == columns - 1) {
                x += bulge[j];
            }
            grid[j][i] = Vec2{x, y};
        }
    }

    for (int j = 0; j < rows - 1; j++) {
        for (int i = 0; i < columns - 1; i++) {
            const Vec2 p00 = grid[j][i];
            const Vec2 p10 = grid[j][i + 1];
            const Vec2 p11 = grid[j + 1][i + 1];
            const Vec2 p01 = grid[j + 1][i];

            // 場所によって色を変える。三角形2つのあいだでも明るさを変える
            const Color color = from_hex(0x3050A0 + static_cast<uint32_t>(i * 0x100000 + j * 0x001400));
            const Color color_b = shade(color, 24);

            // 対角線の向きを、マスごとに入れ替える
            if ((i + j) % 2 == 0) {
                add_triangle(triangles, p00, p10, p11, color);
                add_triangle(triangles, p00, p11, p01, color_b);
            } else {
                add_triangle(triangles, p00, p10, p01, color);
                add_triangle(triangles, p10, p11, p01, color_b);
            }
        }
    }
}

// 絵 4：扇。12 枚の三角形が、ピクセルの中心に乗った1つの頂点を共有する
// 共有する頂点のピクセルを、何枚が塗るかを見る
void build_fan(std::vector<Triangle>& triangles) {
    const Vec2 center = {80.5f, 45.5f};
    const Vec2 offsets[12] = {
        {30.0f, 0.0f}, {26.0f, 15.0f}, {15.0f, 26.0f}, {0.0f, 30.0f},
        {-15.0f, 26.0f}, {-26.0f, 15.0f}, {-30.0f, 0.0f}, {-26.0f, -15.0f},
        {-15.0f, -26.0f}, {0.0f, -30.0f}, {15.0f, -26.0f}, {26.0f, -15.0f},
    };
    for (int k = 0; k < 12; k++) {
        const Vec2 offset0 = offsets[k];
        const Vec2 offset1 = offsets[(k + 1) % 12];
        const Vec2 p0 = {center.x + offset0.x, center.y + offset0.y};
        const Vec2 p1 = {center.x + offset1.x, center.y + offset1.y};
        const Color color = (k % 2 == 0) ? from_hex(0xE06060) : from_hex(0x60C0A0);
        add_triangle(triangles, center, p0, p1, color);
    }
}

// 絵 5：頂点が整数の位置にある正方形（40, 5）〜（120, 85）
// ふちはピクセルの中心を通らない（対角線だけは、45 度なので通る）
void build_square(std::vector<Triangle>& triangles) {
    add_quad(triangles, Vec2{40.0f, 5.0f}, Vec2{120.0f, 5.0f}, Vec2{120.0f, 85.0f}, Vec2{40.0f, 85.0f}, from_hex(0xE06060), from_hex(0x6080E0));
}

std::vector<Triangle> build_scene(int scene) {
    std::vector<Triangle> triangles;
    switch (scene) {
        case 0:
            build_face(triangles);
            break;
        case 1:
            build_banner(triangles);
            break;
        case 2:
            build_lattice(triangles);
            break;
        case 3:
            build_fan(triangles);
            break;
        default:
            build_square(triangles);
            break;
    }
    return triangles;
}

const char* scene_name(int scene) {
    switch (scene) {
        case 0:
            return "1: face (character)";
        case 1:
            return "2: banner (width 15, centered)";
        case 2:
            return "3: mesh (vertices on pixel centers)";
        case 3:
            return "4: fan (shared vertex on a pixel center)";
        default:
            return "5: square (vertices on integers)";
    }
}

const char* rule_name(FillRule rule) {
    switch (rule) {
        case FillRule::top_left:
            return "top-left rule (the standard)";
        case FillRule::bottom_right:
            return "bottom-right rule (reversed)";
        case FillRule::greater:
            return "> 0 only (edge pixels are never drawn)";
        case FillRule::greater_or_equal:
            return ">= 0 (edge pixels are always drawn)";
    }
    return "";
}

const char* view_name(int view_mode) {
    switch (view_mode) {
        case 0:
            return "single";
        case 1:
            return "side by side (left: top-left, right: bottom-right)";
        default:
            return "difference (red: drawn by only one of the two rules)";
    }
}

// キーが押された瞬間だけ true を返す。押しっぱなしでは1回しか反応しない
bool key_pressed(Canvas& canvas, int key, bool was_down[]) {
    const bool is_down = canvas.is_key_down(key);
    const bool pressed = is_down && !was_down[key];
    was_down[key] = is_down;
    return pressed;
}

// canvas の (x, y) を color にする。範囲の外は何もしない
void put_pixel(Canvas& canvas, int x, int y, Color color) {
    if (x < 0 || x >= canvas.width || y < 0 || y >= canvas.height) {
        return;
    }
    canvas.pixels[y * canvas.width + x] = color;
}

// image の view で決めた範囲を、canvas に拡大して出す
// show_diff が true のときは、image と other で塗られ方が違うピクセルを赤にする（ほかは暗くする）
void draw_view(Canvas& canvas, const Image& image, const Image& other, bool show_diff, const View& view, bool show_guides) {
    // 拡大が小さいときは、ピクセルの境目の線が絵をつぶすので、出さない
    const bool guides = show_guides && view.zoom >= 8;

    for (int v = 0; v < view.dest_h; v++) {
        for (int u = 0; u < view.dest_w; u++) {
            // 絵のどのピクセルか。bx, by は、そのピクセルを拡大したマスの中の位置
            const int sx = view.view_x + u / view.zoom;
            const int sy = view.view_y + v / view.zoom;
            const int bx = u % view.zoom;
            const int by = v % view.zoom;

            // 絵の外は灰色にする
            Color color = from_hex(0x282828);
            if (sx >= 0 && sx < image.width && sy >= 0 && sy < image.height) {
                const int i = sy * image.width + sx;
                color = image.pixels[i];

                if (show_diff) {
                    if ((image.counts[i] > 0) != (other.counts[i] > 0)) {
                        color = from_hex(0xFF2020);
                    } else {
                        color = Color{static_cast<uint8_t>(color.r / 2), static_cast<uint8_t>(color.g / 2), static_cast<uint8_t>(color.b / 2)};
                    }
                }

                if (guides) {
                    if (bx == 0 || by == 0) {
                        color = from_hex(0x505070);
                    } else if (std::abs(2 * bx + 1 - view.zoom) <= 1 && std::abs(2 * by + 1 - view.zoom) <= 1) {
                        // マスの真ん中（ピクセルの中心）。どんな色の上でも見えるように、色を反転する
                        color = Color{static_cast<uint8_t>(255 - color.r), static_cast<uint8_t>(255 - color.g), static_cast<uint8_t>(255 - color.b)};
                    }
                }
            }
            put_pixel(canvas, view.dest_x + u, view.dest_y + v, color);
        }
    }
}

// 絵の上の位置 p が、canvas のどこに当たるか。ピクセルの中心（x + 0.5）は、拡大したマスの真ん中に来る
Vec2 to_canvas(const View& view, Vec2 p) {
    return Vec2{
        static_cast<float>(view.dest_x) + (p.x - static_cast<float>(view.view_x)) * static_cast<float>(view.zoom),
        static_cast<float>(view.dest_y) + (p.y - static_cast<float>(view.view_y)) * static_cast<float>(view.zoom),
    };
}

// canvas の上に、a から b まで線を引く。view の範囲の外には引かない
void draw_wire_line(Canvas& canvas, const View& view, Vec2 a, Vec2 b, Color color) {
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length = std::max(std::abs(dx), std::abs(dy));
    // とても大きい値だと繰り返しが長くなるので、画面の大きさで打ち切る
    if (length > 20000.0f) {
        return;
    }
    const int steps = static_cast<int>(std::ceil(length)) + 1;
    for (int i = 0; i <= steps; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = static_cast<int>(std::floor(a.x + dx * t));
        const int y = static_cast<int>(std::floor(a.y + dy * t));
        if (x >= view.dest_x && x < view.dest_x + view.dest_w && y >= view.dest_y && y < view.dest_y + view.dest_h) {
            put_pixel(canvas, x, y, color);
        }
    }
}

// 三角形のふちを、本当の位置（小数のまま）で重ねて描く
void draw_wireframe(Canvas& canvas, const std::vector<Triangle>& triangles, const View& view) {
    const Color color = from_hex(0xFFFF00);
    for (const Triangle& triangle : triangles) {
        const Vec2 a = to_canvas(view, triangle.a);
        const Vec2 b = to_canvas(view, triangle.b);
        const Vec2 c = to_canvas(view, triangle.c);
        draw_wire_line(canvas, view, a, b, color);
        draw_wire_line(canvas, view, b, c, color);
        draw_wire_line(canvas, view, c, a, color);
    }
}

// dest の真ん中に center が来るように、view_x, view_y を決める
View make_view(int dest_x, int dest_y, int dest_w, int dest_h, Vec2 center, int zoom) {
    const float half_w = static_cast<float>(dest_w) / (2.0f * static_cast<float>(zoom));
    const float half_h = static_cast<float>(dest_h) / (2.0f * static_cast<float>(zoom));
    return View{
        dest_x,
        dest_y,
        dest_w,
        dest_h,
        static_cast<int>(std::floor(center.x - half_w)),
        static_cast<int>(std::floor(center.y - half_h)),
        zoom,
    };
}

// 画面の左下に、今のルールを色で示す。選ばれているルールだけ明るい
void draw_rule_indicator(Canvas& canvas, FillRule rule) {
    const Color colors[4] = {from_hex(0x40E040), from_hex(0xFF9020), from_hex(0xC060FF), from_hex(0x40D0FF)};
    const FillRule rules[4] = {FillRule::top_left, FillRule::bottom_right, FillRule::greater, FillRule::greater_or_equal};
    for (int k = 0; k < 4; k++) {
        const Color color = (rules[k] == rule) ? colors[k] : shade(colors[k], -170);
        for (int y = 0; y < 4; y++) {
            for (int x = 0; x < 8; x++) {
                put_pixel(canvas, 2 + k * 10 + x, canvas.height - 6 + y, color);
            }
        }
    }
}

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    Image shown = make_image(scene_width, scene_height);
    Image image_top_left = make_image(scene_width, scene_height);
    Image image_bottom_right = make_image(scene_width, scene_height);

    int scene = 0;
    FillRule rule = FillRule::top_left;
    int view_mode = 0;
    bool additive = false;
    bool show_guides = true;
    bool show_wire = false;

    const int zoom_levels[5] = {1, 2, 4, 8, 16};
    int zoom_index = 1;
    Vec2 center = {static_cast<float>(scene_width) / 2.0f, static_cast<float>(scene_height) / 2.0f};

    std::vector<Triangle> triangles = build_scene(scene);
    bool was_down[256] = {};

    std::printf("T: top-left  B: bottom-right  G: > 0  E: >= 0  V: view  Z/X: zoom  arrows: move\n");
    std::printf("C: pixel centers  W: wireframe  A: overlap brightening  1-5: scene  Esc: quit\n");
    std::printf("scene: %s\n", scene_name(scene));
    std::printf("rule: %s\n", rule_name(rule));

    while (canvas.update()) {
        if (canvas.is_key_down(key_escape)) {
            break;
        }

        // 絵の切り替え
        for (int k = 0; k < scene_count; k++) {
            if (key_pressed(canvas, '1' + k, was_down) && k != scene) {
                scene = k;
                triangles = build_scene(scene);
                std::printf("scene: %s\n", scene_name(scene));
            }
        }

        // ルールの切り替え。変えたときだけ、コンソールに知らせる
        FillRule new_rule = rule;
        if (key_pressed(canvas, 'T', was_down)) {
            new_rule = FillRule::top_left;
        }
        if (key_pressed(canvas, 'B', was_down)) {
            new_rule = FillRule::bottom_right;
        }
        if (key_pressed(canvas, 'G', was_down)) {
            new_rule = FillRule::greater;
        }
        if (key_pressed(canvas, 'E', was_down)) {
            new_rule = FillRule::greater_or_equal;
        }
        if (new_rule != rule) {
            rule = new_rule;
            std::printf("rule: %s\n", rule_name(rule));
        }

        if (key_pressed(canvas, 'V', was_down)) {
            view_mode = (view_mode + 1) % 3;
            std::printf("view: %s\n", view_name(view_mode));
        }
        if (key_pressed(canvas, 'A', was_down)) {
            additive = !additive;
            std::printf("overlap brightening: %s\n", additive ? "on" : "off");
        }
        if (key_pressed(canvas, 'C', was_down)) {
            show_guides = !show_guides;
            std::printf("pixel grid and centers: %s (shown at zoom 8 or more)\n", show_guides ? "on" : "off");
        }
        if (key_pressed(canvas, 'W', was_down)) {
            show_wire = !show_wire;
            std::printf("wireframe: %s\n", show_wire ? "on" : "off");
        }
        if (key_pressed(canvas, 'Z', was_down) && zoom_index < 4) {
            zoom_index++;
            std::printf("zoom: x%d\n", zoom_levels[zoom_index]);
        }
        if (key_pressed(canvas, 'X', was_down) && zoom_index > 0) {
            zoom_index--;
            std::printf("zoom: x%d\n", zoom_levels[zoom_index]);
        }
        const int zoom = zoom_levels[zoom_index];

        // 矢印キーで表示の中心を動かす。拡大しているほど、ゆっくり動かす
        const float speed = 160.0f / static_cast<float>(zoom) * canvas.delta_time;
        if (canvas.is_key_down(key_left)) {
            center.x -= speed;
        }
        if (canvas.is_key_down(key_right)) {
            center.x += speed;
        }
        if (canvas.is_key_down(key_up)) {
            center.y -= speed;
        }
        if (canvas.is_key_down(key_down)) {
            center.y += speed;
        }
        center.x = std::clamp(center.x, 0.0f, static_cast<float>(scene_width));
        center.y = std::clamp(center.y, 0.0f, static_cast<float>(scene_height));

        // 絵を描く。比べる表示のために、2つのルールの絵も毎フレーム描く
        render_scene(shown, triangles, rule, additive);
        render_scene(image_top_left, triangles, FillRule::top_left, additive);
        render_scene(image_bottom_right, triangles, FillRule::bottom_right, additive);

        // 表示。間の線を残すため、まず灰色で全体を塗る
        for (Color& pixel : canvas.pixels) {
            pixel = from_hex(0x606060);
        }
        if (view_mode == 0) {
            const View view = make_view(0, 0, canvas.width, canvas.height, center, zoom);
            draw_view(canvas, shown, shown, false, view, show_guides);
            if (show_wire) {
                draw_wireframe(canvas, triangles, view);
            }
            draw_rule_indicator(canvas, rule);
        } else if (view_mode == 1) {
            const int half = (canvas.width - 2) / 2;
            const View left_view = make_view(0, 0, half, canvas.height, center, zoom);
            const View right_view = make_view(half + 2, 0, half, canvas.height, center, zoom);
            draw_view(canvas, image_top_left, image_bottom_right, false, left_view, show_guides);
            draw_view(canvas, image_bottom_right, image_top_left, false, right_view, show_guides);
            if (show_wire) {
                draw_wireframe(canvas, triangles, left_view);
                draw_wireframe(canvas, triangles, right_view);
            }
        } else {
            const View view = make_view(0, 0, canvas.width, canvas.height, center, zoom);
            draw_view(canvas, image_top_left, image_bottom_right, true, view, show_guides);
            if (show_wire) {
                draw_wireframe(canvas, triangles, view);
            }
        }

        canvas.present();
    }
}
