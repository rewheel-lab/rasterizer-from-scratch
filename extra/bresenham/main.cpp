// 番外編「ブレゼンハム：整数だけで線を引く」の実験用プログラム
// ラスタライザー編 #02 の線の引き方（DDA）と、小数を使わずに整数だけで引くブレゼンハムを並べて、違うピクセルを赤く光らせる
// ブレゼンハムは、ずれ（誤差）を整数でためていく。マス目の上にその値を数字で出して、ためて戻す様子を見る
//
// 操作
//   1    … #02 の花火（画面の真ん中から、ふちへ 20 ピクセルおきに線を引く）を、左に DDA、右にブレゼンハムで並べる
//          画面の幅の半分に入るのは、花火の真ん中の部分（左右 79 ピクセルずつ）。Z, X と矢印で、2枚いっしょに拡大や移動ができる
//   2    … 同じ花火を、#02 と同じ大きさで1枚出して、DDA とブレゼンハムで違うピクセルを赤で表示する
//   3    … 線を1本だけ拡大して、誤差の数字を出す。始点から右へ 10、上へ 4 の線
//   4    … 3 と同じで、始点から右へ 10、上へ 5 の線（傾きがちょうど 1/2）。もう一度押すと、最初の線に戻る
//   V    … 表示の切り替え（1: そのまま → 相手にないピクセルを赤で光らせる。2, 3, 4: DDA → ブレゼンハム → 違いを赤で表示）
//   Z, X … 拡大、縮小
//   矢印 … 表示する場所を動かす
//   C    … ピクセルの境目の線を出す（拡大が 8 倍以上のとき）
//   0    … 表示の位置と拡大を、最初に戻す
//   F    … 始点を (0, 0) に置き、終点を ±60 の中の全部の位置にして、DDA とブレゼンハムが違う線を数える
//   M    … 速さを測る（DDA とブレゼンハムで、同じ線を大量に引く）
//   Esc  … 終わる
// 場面 1, 2
//   L    … 線の本数を変える（20 ピクセルおき → 10 → 5）
// 場面 3, 4
//   W, A, S, D … 終点を1マス動かす（Shift を押しながらだと始点）
//   Q    … 始点と終点を入れ替える（同じ線を、反対の端から引く）
//   R    … 終点を、始点のまわりでゆっくり回す（もう一度押すと止まる）
//   E    … 誤差の数字を出す（拡大が 16 倍以上のとき。ブレゼンハムと、違いの表示で出る）
//   T    … 本当の線（始点のピクセルの中心から、終点のピクセルの中心まで）を重ねる
//   P    … 今の線の誤差の表を、コンソールに出す
// 場面を切り替えたり、線が変わったりしたときは、違うピクセルの数をコンソールに出す
// 画面の左下の小さな四角の印は、今の表示（明るいのが選ばれている）。左から順に、DDA、ブレゼンハム、違い
// 違いの表示では、DDA だけが打ったピクセルは赤いベタ塗り、ブレゼンハムだけが打ったピクセルは赤い枠で表す
// 誤差の数字は、マスの中に出す。マスごとに、誤差に足したあと、超えたかを調べる前の値を出す
//   緑のマス … 超えた。次のマスは1段進む。下の黄色い数字は、1段分を引いたあとの誤差
//   紫のマス … 超えていないが、基準とちょうど同じ値（本当の線が、次のマスの2つのピクセルのちょうど真ん中を通る）
// 画面の y は下向きなので、右上がりの線は、終点の y が始点より小さい

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <utility>
#include <vector>

#include "framework/canvas.h"

// 画面上の位置（ピクセル）
struct Point {
    int x;
    int y;
};

// 画面の上の位置。ピクセルの間の位置も表せるように、小数で持つ
struct Vec2 {
    float x;
    float y;
};

// 絵を描く先。Canvas と同じ並びのピクセルに加えて、各ピクセルに線が通ったかを覚える
// 線が通ったかは、DDA とブレゼンハムの違いを調べるために使う
struct Image {
    int width;
    int height;
    std::vector<Color> pixels;
    std::vector<uint8_t> lit;  // 1: 線が通った
};

// 線を引くときの、1マスごとの記録。デバッグ表示と、違いを数えるために使う
struct LineStep {
    int x;          // 打ったピクセル
    int y;
    int checked;    // ブレゼンハムの誤差に足したあと、超えたかを調べる前の値（DDA では使わない）
    bool stepped;   // 超えて、次のマスを1段進めたか（DDA では使わない）
    int error;      // 1段分を引いたあとの誤差。超えなかったときは checked と同じ（DDA では使わない）
};

// 表示するもの
enum class Display {
    dda,
    bresenham,
    difference,  // 違うピクセルを赤で出す
};

// canvas の中の dest の四角形に、絵の left, top から始まる範囲を、zoom 倍にして出す
struct View {
    int dest_x;
    int dest_y;
    int dest_w;
    int dest_h;
    int left;
    int top;
    int zoom;
};

// 場面の番号（キーの 1〜4 に対応する）
const int scene_side_by_side = 0;
const int scene_fan = 1;
const int scene_line_4 = 2;
const int scene_line_5 = 3;
const int scene_count = 4;

// 絵の大きさ。canvas と同じにして、花火を #02 と同じ大きさで引く
const int scene_width = 320;
const int scene_height = 180;

const int zoom_levels[7] = {1, 2, 4, 8, 16, 24, 32};
const int zoom_level_count = 7;

// 場面 3, 4 の線の始点。画面の真ん中あたりに置く
const Point line_origin = {155, 92};

const Color background = {16, 16, 48};
const Color line_color = {176, 104, 32};

// 0xRRGGBB の形の数を Color にする
Color from_hex(uint32_t hex) {
    const uint8_t r = static_cast<uint8_t>((hex >> 16) & 0xFF);
    const uint8_t g = static_cast<uint8_t>((hex >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>(hex & 0xFF);
    return Color{r, g, b};
}

// 色を暗くする。1 で変わらず、0 で黒
Color darken(Color color, float amount) {
    return Color{
        static_cast<uint8_t>(static_cast<float>(color.r) * amount),
        static_cast<uint8_t>(static_cast<float>(color.g) * amount),
        static_cast<uint8_t>(static_cast<float>(color.b) * amount),
    };
}

Image make_image(int width, int height) {
    const size_t size = static_cast<size_t>(width) * static_cast<size_t>(height);
    return Image{width, height, std::vector<Color>(size, background), std::vector<uint8_t>(size, 0)};
}

void clear_image(Image& image) {
    std::fill(image.pixels.begin(), image.pixels.end(), background);
    std::fill(image.lit.begin(), image.lit.end(), static_cast<uint8_t>(0));
}

// (x, y) のピクセルを color にする。線が通ったことも覚える
void set_pixel(Image& image, int x, int y, Color color) {
    // 画面の外は何もしない。はみ出して pixels の外を書くと、プログラムが壊れるため
    if (x < 0 || x >= image.width || y < 0 || y >= image.height) {
        return;
    }
    image.pixels[y * image.width + x] = color;
    image.lit[y * image.width + x] = 1;
}

// (x0, y0) から (x1, y1) まで、1ピクセルずつ進みながら線を引く。#02 の draw_line と同じ中身
// trace が nullptr でなければ、打ったピクセルを順に記録する
void draw_line_dda(Image& image, int x0, int y0, int x1, int y1, Color color, std::vector<LineStep>* trace = nullptr) {
    const int dx = x1 - x0;
    const int dy = y1 - y0;

    // x と y のうち長いほうの軸で数える。短いほうは1歩で1マス以下しか動かないので、間が空かない
    const int steps = std::max(std::abs(dx), std::abs(dy));

    // 始点と終点が同じ場所だと steps が 0 になり、下の割り算ができない。点を1つ打って終わる
    if (steps == 0) {
        set_pixel(image, x0, y0, color);
        if (trace != nullptr) {
            trace->push_back(LineStep{x0, y0, 0, false, 0});
        }
        return;
    }

    for (int i = 0; i <= steps; i++) {
        // t は 0（始点）から 1（終点）まで進む割合
        const float t = static_cast<float>(i) / static_cast<float>(steps);

        // 最も近いピクセルに丸める。ちょうど半分のときは、0 から遠いほう（始点から遠いほう）に丸まる
        const int x = x0 + static_cast<int>(std::round(static_cast<float>(dx) * t));
        const int y = y0 + static_cast<int>(std::round(static_cast<float>(dy) * t));
        set_pixel(image, x, y, color);
        if (trace != nullptr) {
            trace->push_back(LineStep{x, y, 0, false, 0});
        }
    }
}

// (x0, y0) から (x1, y1) まで、整数だけで線を引く（ブレゼンハム）
// trace が nullptr でなければ、打ったピクセルと、そのときの誤差を順に記録する
void draw_line_bresenham(Image& image, int x0, int y0, int x1, int y1, Color color, std::vector<LineStep>* trace = nullptr) {
    // 8つの向きを、1つの書き方にまとめる
    // 縦のほうが長い線は、x と y を入れ替えて、ゆるやかな線として扱う（打つときに元へ戻す）
    const bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    // 右から左へ進む線は、始点と終点を入れ替える。いつも左から右へ進めば、向きの場合分けが減る
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    const int dx = x1 - x0;
    const int dy = std::abs(y1 - y0);
    // 下がる線（y が減る線）は、縦に進む向きをマイナスにするだけ
    const int y_step = (y0 < y1) ? 1 : -1;

    // 誤差は、本当の線と今の高さのずれ。小数をなくすために、全部を 2 * dx 倍して整数にしてある
    //   1 マス分のずれ = 2 * dx
    //   半マス分のずれ = dx（超えたかどうかの基準）
    //   横に1マス進んだときの、本当の線の上がり = dy / dx マス = 2 * dy
    int error = 0;
    int y = y0;
    for (int x = x0; x <= x1; x++) {
        if (steep) {
            set_pixel(image, y, x, color);
        } else {
            set_pixel(image, x, y, color);
        }

        const int plotted_y = y;

        error += 2 * dy;
        const int checked = error;
        if (error > dx) {
            y += y_step;
            error -= 2 * dx;
        }

        if (trace != nullptr) {
            // 表示のための記録。線の引き方には関係しない。入れ替えて計算していたときは、元の x と y に戻して記録する
            trace->push_back(LineStep{steep ? plotted_y : x, steep ? x : plotted_y, checked, checked > dx, error});
        }
    }
}

// 誤差が基準とちょうど同じ値になったマスの数。本当の線が、次のマスの2つのピクセルのちょうど真ん中を通る場所
// 最後のマスの次はないので、数えない
int count_half_points(const std::vector<LineStep>& trace, int threshold) {
    int count = 0;
    for (size_t i = 0; i + 1 < trace.size(); i++) {
        if (trace[i].checked == threshold) {
            count++;
        }
    }
    return count;
}

// 線の長いほうの軸の長さ（歩数）。ブレゼンハムの dx と同じ値
int long_length(Point start, Point end) {
    return std::max(std::abs(end.x - start.x), std::abs(end.y - start.y));
}

// 花火の1回分の記録
struct FanStats {
    int lines;
    int half_lines;   // ちょうど半分を通る線の数
    int half_points;  // ちょうど半分のマスの数
};

// 同じ線を、DDA とブレゼンハムで別々の絵に引く
void draw_both(Image& dda_image, Image& bres_image, int x0, int y0, int x1, int y1, FanStats& stats) {
    std::vector<LineStep> trace;
    draw_line_dda(dda_image, x0, y0, x1, y1, line_color);
    draw_line_bresenham(bres_image, x0, y0, x1, y1, line_color, &trace);

    stats.lines++;
    const int half_points = count_half_points(trace, long_length(Point{x0, y0}, Point{x1, y1}));
    if (half_points > 0) {
        stats.half_lines++;
        stats.half_points += half_points;
    }
}

// 場面 1, 2：#02 の花火。絵の真ん中から、絵のふちの点へ、spacing ピクセルおきに線を引く
FanStats render_fan(Image& dda_image, Image& bres_image, int spacing) {
    clear_image(dda_image);
    clear_image(bres_image);

    FanStats stats = {0, 0, 0};
    const int center_x = dda_image.width / 2;
    const int center_y = dda_image.height / 2;
    const int last_x = dda_image.width - 1;
    const int last_y = dda_image.height - 1;
    for (int x = 0; x <= last_x; x += spacing) {
        draw_both(dda_image, bres_image, center_x, center_y, x, 0, stats);        // 上のふち
        draw_both(dda_image, bres_image, center_x, center_y, x, last_y, stats);   // 下のふち
    }
    for (int y = 0; y <= last_y; y += spacing) {
        draw_both(dda_image, bres_image, center_x, center_y, 0, y, stats);        // 左のふち
        draw_both(dda_image, bres_image, center_x, center_y, last_x, y, stats);   // 右のふち
    }
    return stats;
}

// 場面 3, 4：線を1本だけ引く。表示のために、1マスごとの記録も取る
void render_line(Image& dda_image, Image& bres_image, Point start, Point end, std::vector<LineStep>& dda_trace, std::vector<LineStep>& bres_trace) {
    clear_image(dda_image);
    clear_image(bres_image);
    dda_trace.clear();
    bres_trace.clear();
    draw_line_dda(dda_image, start.x, start.y, end.x, end.y, line_color, &dda_trace);
    draw_line_bresenham(bres_image, start.x, start.y, end.x, end.y, line_color, &bres_trace);
}

// 2つの絵で、線が通ったかが違うピクセルの数と、線が通ったピクセルの数
struct Difference {
    int only_dda;
    int only_bresenham;
    int dda_pixels;
    int bresenham_pixels;
};

Difference compare_images(const Image& dda_image, const Image& bres_image) {
    Difference difference = {0, 0, 0, 0};
    for (size_t i = 0; i < dda_image.lit.size(); i++) {
        const bool lit_dda = dda_image.lit[i] != 0;
        const bool lit_bres = bres_image.lit[i] != 0;
        difference.dda_pixels += lit_dda ? 1 : 0;
        difference.bresenham_pixels += lit_bres ? 1 : 0;
        if (lit_dda && !lit_bres) {
            difference.only_dda++;
        }
        if (!lit_dda && lit_bres) {
            difference.only_bresenham++;
        }
    }
    return difference;
}

// 記録のピクセルを、並べ替えて比べられる数にする。始点の向きが違う線どうしを比べるため、順番ではなく集まりで比べる
std::vector<int> pixel_keys(const std::vector<LineStep>& trace) {
    std::vector<int> keys;
    for (const LineStep& step : trace) {
        keys.push_back((step.x + 1000) * 4096 + (step.y + 1000));
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

// a にあって b にないピクセルの数
int count_only_in_first(const std::vector<LineStep>& a, const std::vector<LineStep>& b) {
    const std::vector<int> keys_a = pixel_keys(a);
    const std::vector<int> keys_b = pixel_keys(b);
    std::vector<int> only_a;
    std::set_difference(keys_a.begin(), keys_a.end(), keys_b.begin(), keys_b.end(), std::back_inserter(only_a));
    return static_cast<int>(only_a.size());
}

// 違う線の数と、違うピクセルの数（片方にだけあるピクセルの数）の集計
struct Tally {
    int lines;
    int different_lines;
    int different_pixels;
};

void add_to_tally(Tally& tally, int different_pixels) {
    tally.lines++;
    if (different_pixels > 0) {
        tally.different_lines++;
        tally.different_pixels += different_pixels;
    }
}

void print_tally(const char* name, const Tally& tally, int total_pixels) {
    const double line_percent = 100.0 * static_cast<double>(tally.different_lines) / static_cast<double>(std::max(1, tally.lines));
    const double pixel_percent = 100.0 * static_cast<double>(tally.different_pixels) / static_cast<double>(std::max(1, total_pixels));
    std::printf("  %-52s lines %5d of %5d (%5.2f%%)   pixels %6d (%5.2f%%)\n", name, tally.different_lines, tally.lines, line_percent, tally.different_pixels, pixel_percent);
}

// 始点を (0, 0) に置き、終点を ±max_length の中の全部の位置にして、線を引き比べる
void count_all_lines(int max_length) {
    // 絵には描かない。1x1 の絵を渡すと、画面の外として全部捨てられて、記録だけが残る
    Image nowhere = make_image(1, 1);

    Tally dda_vs_bresenham = {};
    Tally dda_vs_bresenham_forward = {};    // 長いほうの軸が増える向きに引く線
    Tally dda_vs_bresenham_backward = {};   // 長いほうの軸が減る向きに引く線
    Tally swap_dda = {};
    Tally swap_bresenham = {};
    int half_lines = 0;
    int half_points = 0;
    int total_lines = 0;
    int total_pixels = 0;
    int forward_pixels = 0;
    int backward_pixels = 0;

    std::vector<LineStep> dda;
    std::vector<LineStep> dda_back;
    std::vector<LineStep> bresenham;
    std::vector<LineStep> bresenham_back;

    for (int dy = -max_length; dy <= max_length; dy++) {
        for (int dx = -max_length; dx <= max_length; dx++) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            dda.clear();
            dda_back.clear();
            bresenham.clear();
            bresenham_back.clear();

            // 始点 → 終点 と、終点 → 始点 の2通りで引く
            draw_line_dda(nowhere, 0, 0, dx, dy, line_color, &dda);
            draw_line_dda(nowhere, dx, dy, 0, 0, line_color, &dda_back);
            draw_line_bresenham(nowhere, 0, 0, dx, dy, line_color, &bresenham);
            draw_line_bresenham(nowhere, dx, dy, 0, 0, line_color, &bresenham_back);

            const int long_len = long_length(Point{0, 0}, Point{dx, dy});
            const int points = count_half_points(bresenham, long_len);
            if (points > 0) {
                half_lines++;
                half_points += points;
            }

            // 長いほうの軸が増える向き（右へ、または下へ）か
            const bool forward = (std::abs(dy) > std::abs(dx)) ? (dy > 0) : (dx > 0);
            const int different = count_only_in_first(dda, bresenham);
            add_to_tally(dda_vs_bresenham, different);
            add_to_tally(forward ? dda_vs_bresenham_forward : dda_vs_bresenham_backward, different);
            add_to_tally(swap_dda, count_only_in_first(dda, dda_back));
            add_to_tally(swap_bresenham, count_only_in_first(bresenham, bresenham_back));
            total_lines++;
            total_pixels += static_cast<int>(dda.size());
            (forward ? forward_pixels : backward_pixels) += static_cast<int>(dda.size());
        }
    }

    std::printf("lines from (0, 0) to every end point within +-%d: %d lines, %d pixels\n", max_length, total_lines, total_pixels);
    std::printf("  lines that pass exactly through a half point: %d (%.2f%%), half points: %d\n", half_lines, 100.0 * static_cast<double>(half_lines) / static_cast<double>(total_lines), half_points);
    std::printf("pixels drawn by DDA but not by Bresenham (the same number the other way around):\n");
    print_tally("all lines", dda_vs_bresenham, total_pixels);
    print_tally("lines drawn toward right or down (long axis +)", dda_vs_bresenham_forward, forward_pixels);
    print_tally("lines drawn toward left or up (long axis -)", dda_vs_bresenham_backward, backward_pixels);
    std::printf("the same line drawn from the other end (start and end swapped):\n");
    print_tally("DDA", swap_dda, total_pixels);
    print_tally("Bresenham", swap_bresenham, total_pixels);
}

// 次の乱数を返す。同じ数列を2つの方法に使うための、簡単な計算
uint32_t next_random(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state >> 8;
}

// DDA とブレゼンハムで、同じ線を大量に引いて、かかった時間を測る
void measure_speed() {
    const int line_count = 100000;
    const int rounds = 3;

    // 画面の中の、でたらめな2点を結ぶ線。どちらの方法にも同じ線を使う
    std::vector<Point> starts;
    std::vector<Point> ends;
    uint32_t state = 12345u;
    for (int i = 0; i < line_count; i++) {
        starts.push_back(Point{static_cast<int>(next_random(state) % scene_width), static_cast<int>(next_random(state) % scene_height)});
        ends.push_back(Point{static_cast<int>(next_random(state) % scene_width), static_cast<int>(next_random(state) % scene_height)});
    }

    Image image = make_image(scene_width, scene_height);
    double best_dda = 1.0e30;
    double best_bresenham = 1.0e30;
    int lit_dda = 0;
    int lit_bresenham = 0;

    std::printf("drawing %d random lines, best of %d rounds (the lines are drawn into a %dx%d picture)\n", line_count, rounds, scene_width, scene_height);
    for (int round = 0; round < rounds; round++) {
        clear_image(image);
        const auto dda_begin = std::chrono::steady_clock::now();
        for (int i = 0; i < line_count; i++) {
            draw_line_dda(image, starts[i].x, starts[i].y, ends[i].x, ends[i].y, line_color);
        }
        const double dda_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - dda_begin).count();
        lit_dda = static_cast<int>(std::count(image.lit.begin(), image.lit.end(), static_cast<uint8_t>(1)));

        clear_image(image);
        const auto bres_begin = std::chrono::steady_clock::now();
        for (int i = 0; i < line_count; i++) {
            draw_line_bresenham(image, starts[i].x, starts[i].y, ends[i].x, ends[i].y, line_color);
        }
        const double bres_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - bres_begin).count();
        lit_bresenham = static_cast<int>(std::count(image.lit.begin(), image.lit.end(), static_cast<uint8_t>(1)));

        best_dda = std::min(best_dda, dda_ms);
        best_bresenham = std::min(best_bresenham, bres_ms);
    }
    std::printf("  DDA       : %8.2f ms   (lit pixels in the picture: %d)\n", best_dda, lit_dda);
    std::printf("  Bresenham : %8.2f ms   (lit pixels in the picture: %d)\n", best_bresenham, lit_bresenham);
    std::printf("  Bresenham / DDA = %.2f\n", best_bresenham / best_dda);
}

// 3×5 ピクセルの数字。# のところが点く。0〜9 と、10 番がマイナス
// 外のフォントや Windows の文字の描き方は使わず、ここに持っておく
const char* const glyphs[11][5] = {
    {"###", "# #", "# #", "# #", "###"},  // 0
    {" # ", "## ", " # ", " # ", "###"},  // 1
    {"###", "  #", "###", "#  ", "###"},  // 2
    {"###", "  #", "###", "  #", "###"},  // 3
    {"# #", "# #", "###", "  #", "  #"},  // 4
    {"###", "#  ", "###", "  #", "###"},  // 5
    {"###", "#  ", "###", "# #", "###"},  // 6
    {"###", "  #", "  #", "  #", "  #"},  // 7
    {"###", "# #", "###", "# #", "###"},  // 8
    {"###", "# #", "###", "  #", "###"},  // 9
    {"   ", "   ", "###", "   ", "   "},  // -
};

const int glyph_width = 3;
const int glyph_height = 5;
const int minus_glyph = 10;

// canvas の (x, y) を color にする。範囲の外は何もしない
void put_pixel(Canvas& canvas, int x, int y, Color color) {
    if (x < 0 || x >= canvas.width || y < 0 || y >= canvas.height) {
        return;
    }
    canvas.pixels[y * canvas.width + x] = color;
}

// canvas の四角形を color で塗る。範囲の外は何もしない
void fill_rect(Canvas& canvas, int left, int top, int width, int height, Color color) {
    for (int y = top; y < top + height; y++) {
        for (int x = left; x < left + width; x++) {
            put_pixel(canvas, x, y, color);
        }
    }
}

// value を、中心が (center_x, center_y) になるように、数字で書く。scale は1ドットを何ピクセルにするか
void draw_number(Canvas& canvas, int value, int center_x, int center_y, int scale, Color color) {
    // 書く文字を、左から順に並べる。マイナス、そのあとに上の桁から
    int ids[12];
    int count = 0;
    if (value < 0) {
        ids[count] = minus_glyph;
        count++;
    }
    int digits[11];
    int digit_count = 0;
    int rest = std::abs(value);
    do {
        digits[digit_count] = rest % 10;
        digit_count++;
        rest /= 10;
    } while (rest > 0);
    for (int k = digit_count - 1; k >= 0; k--) {
        ids[count] = digits[k];
        count++;
    }

    // 文字と文字のあいだは 1 ドット空ける
    const int total_width = (count * glyph_width + (count - 1)) * scale;
    const int left = center_x - total_width / 2;
    const int top = center_y - glyph_height * scale / 2;
    for (int k = 0; k < count; k++) {
        for (int row = 0; row < glyph_height; row++) {
            for (int column = 0; column < glyph_width; column++) {
                if (glyphs[ids[k]][row][column] == '#') {
                    const int x = left + (k * (glyph_width + 1) + column) * scale;
                    fill_rect(canvas, x, top + row * scale, scale, scale, color);
                }
            }
        }
    }
}

// 誤差の数字の1ドットを、何ピクセルにするか。マスに、数字を2段に重ねても収まる大きさにする
int digit_scale(int zoom) {
    return std::max(1, (zoom - 2) / (2 * glyph_height + 1));
}

// 違いの表示で、ブレゼンハムだけが打ったピクセルを囲む赤い枠の太さ。数字を出す大きさのときは、数字が枠にかからないように細くする
int frame_width(int zoom) {
    if (zoom < 16) {
        return std::max(1, zoom / 5);
    }
    return std::clamp((zoom - (2 * glyph_height + 1) * digit_scale(zoom)) / 2, 1, zoom / 5);
}

// 絵を、view で決めたとおり canvas に拡大して出す
// mark が true のときは、もう片方の絵にはない（線が通っていない）ピクセルを、赤で光らせる
void draw_cells(Canvas& canvas, const Image& dda_image, const Image& bres_image, Display display, bool mark, const View& view, bool show_grid) {
    // 拡大が小さいときは、ピクセルの境目の線が絵をつぶすので、出さない
    const bool grid = show_grid && view.zoom >= 8;
    const Color grid_color = from_hex(0x505070);
    const Color red = from_hex(0xFF2020);
    const Color outside = from_hex(0x282828);

    const int frame = frame_width(view.zoom);

    for (int v = 0; v < view.dest_h; v++) {
        for (int u = 0; u < view.dest_w; u++) {
            // 絵のどのピクセルか。bx, by は、そのピクセルを拡大したマスの中の位置
            const int sx = view.left + u / view.zoom;
            const int sy = view.top + v / view.zoom;
            const int bx = u % view.zoom;
            const int by = v % view.zoom;

            Color color = outside;
            if (sx >= 0 && sx < dda_image.width && sy >= 0 && sy < dda_image.height) {
                const int i = sy * dda_image.width + sx;
                const bool lit_dda = dda_image.lit[i] != 0;
                const bool lit_bres = bres_image.lit[i] != 0;
                color = background;
                bool is_red = false;
                if (display == Display::dda) {
                    if (lit_dda) {
                        color = dda_image.pixels[i];
                    }
                    if (mark && lit_dda && !lit_bres) {
                        color = red;
                        is_red = true;
                    }
                } else if (display == Display::bresenham) {
                    if (lit_bres) {
                        color = bres_image.pixels[i];
                    }
                    if (mark && lit_bres && !lit_dda) {
                        color = red;
                        is_red = true;
                    }
                } else {
                    if (lit_dda && lit_bres) {
                        // どちらも打ったピクセルは、暗くして目立たなくする
                        color = darken(line_color, 0.6f);
                    } else if (lit_dda) {
                        color = red;
                        is_red = true;
                    } else if (lit_bres) {
                        // 拡大しているときは枠だけにして、DDA だけの印と見分ける。小さいときは赤いベタ塗り
                        const bool on_frame = bx < frame || bx >= view.zoom - frame || by < frame || by >= view.zoom - frame;
                        if (view.zoom < 8 || on_frame) {
                            color = red;
                            is_red = true;
                        }
                    }
                }

                if (grid && !is_red && (bx == 0 || by == 0)) {
                    color = grid_color;
                }
            }
            put_pixel(canvas, view.dest_x + u, view.dest_y + v, color);
        }
    }
}

// 絵の上の位置 p が、canvas のどこに当たるか。ピクセルの中心（x + 0.5）は、拡大したマスの真ん中に来る
Vec2 to_canvas(const View& view, Vec2 p) {
    return Vec2{
        static_cast<float>(view.dest_x) + (p.x - static_cast<float>(view.left)) * static_cast<float>(view.zoom),
        static_cast<float>(view.dest_y) + (p.y - static_cast<float>(view.top)) * static_cast<float>(view.zoom),
    };
}

// 本当の線を重ねて描く。始点のピクセルの中心から、終点のピクセルの中心まで
void draw_true_line(Canvas& canvas, const View& view, Point start, Point end) {
    const Vec2 a = to_canvas(view, Vec2{static_cast<float>(start.x) + 0.5f, static_cast<float>(start.y) + 0.5f});
    const Vec2 b = to_canvas(view, Vec2{static_cast<float>(end.x) + 0.5f, static_cast<float>(end.y) + 0.5f});
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const int steps = static_cast<int>(std::ceil(std::max(std::abs(dx), std::abs(dy)))) + 1;
    const Color color = from_hex(0xFFFF00);
    for (int i = 0; i <= steps; i++) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const int x = static_cast<int>(std::floor(a.x + dx * t));
        const int y = static_cast<int>(std::floor(a.y + dy * t));
        if (x >= view.dest_x && x < view.dest_x + view.dest_w && y >= view.dest_y && y < view.dest_y + view.dest_h) {
            put_pixel(canvas, x, y, color);
        }
    }
}

// ブレゼンハムの誤差を、マスの中に数字で書く。マスが全部見えているものだけ書く
// 超えたマス（次を1段進めるマス）は緑にして、1段分を引いたあとの誤差を下に書く。基準とちょうど同じ値のマスは紫にする
void draw_errors(Canvas& canvas, const Image& dda_image, const std::vector<LineStep>& bres_trace, int threshold, Display display, const View& view, bool show_grid) {
    if (display == Display::dda || view.zoom < 16) {
        return;
    }
    const int scale = digit_scale(view.zoom);
    const int frame = frame_width(view.zoom);
    const int grid_inset = (show_grid && view.zoom >= 8) ? 1 : 0;
    const Color white = from_hex(0xFFFFFF);
    const Color yellow = from_hex(0xFFE064);
    const Color green = from_hex(0x208858);
    const Color purple = from_hex(0x903C90);

    for (const LineStep& step : bres_trace) {
        const int left = view.dest_x + (step.x - view.left) * view.zoom;
        const int top = view.dest_y + (step.y - view.top) * view.zoom;
        if (left < view.dest_x || top < view.dest_y || left + view.zoom > view.dest_x + view.dest_w || top + view.zoom > view.dest_y + view.dest_h) {
            continue;
        }

        // 違いの表示で、ブレゼンハムだけが打ったマスは、赤い枠を残して中を塗る。ほかは、境目の線を残してマス全体を塗る
        const bool framed = (display == Display::difference) && dda_image.lit[step.y * dda_image.width + step.x] == 0;
        const int inset = framed ? frame : grid_inset;
        const int size = framed ? view.zoom - 2 * frame : view.zoom - grid_inset;
        if (step.stepped) {
            fill_rect(canvas, left + inset, top + inset, size, size, green);
        } else if (step.checked == threshold) {
            fill_rect(canvas, left + inset, top + inset, size, size, purple);
        }

        const int center_x = left + view.zoom / 2;
        const int center_y = top + view.zoom / 2;
        if (step.stepped) {
            // 上に、足したあとの誤差。下に、1段分を引いたあとの誤差
            const int offset = (glyph_height + 1) * scale / 2;
            draw_number(canvas, step.checked, center_x, center_y - offset, scale, white);
            draw_number(canvas, step.error, center_x, center_y + offset, scale, yellow);
        } else {
            draw_number(canvas, step.checked, center_x, center_y, scale, white);
        }
    }
}

// 表示の中心を決めると、view の左上に出す絵のピクセルが決まる。絵の外が見えないように、中心を内側に寄せる
Vec2 clamp_center(Vec2 center, int zoom, int dest_w, int dest_h, const Image& image) {
    const float half_w = static_cast<float>(dest_w) / (2.0f * static_cast<float>(zoom));
    const float half_h = static_cast<float>(dest_h) / (2.0f * static_cast<float>(zoom));
    return Vec2{
        std::clamp(center.x, half_w, static_cast<float>(image.width) - half_w),
        std::clamp(center.y, half_h, static_cast<float>(image.height) - half_h),
    };
}

// dest の真ん中に center が来るように、view の left, top を決める
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

// 画面の左下に、今の表示を色で示す。選ばれているものだけ明るい
void draw_indicators(Canvas& canvas, Display display) {
    const Color colors[3] = {from_hex(0xFFA030), from_hex(0x40D0FF), from_hex(0xFF3030)};
    const Display displays[3] = {Display::dda, Display::bresenham, Display::difference};
    for (int k = 0; k < 3; k++) {
        const Color color = (displays[k] == display) ? colors[k] : darken(colors[k], 0.25f);
        fill_rect(canvas, 2 + k * 10, canvas.height - 6, 8, 4, color);
    }
}

const char* display_name(Display display) {
    switch (display) {
        case Display::dda:
            return "DDA";
        case Display::bresenham:
            return "Bresenham";
        default:
            return "difference (red solid: only DDA, red frame: only Bresenham)";
    }
}

// キーが押された瞬間だけ true を返す。押しっぱなしでは1回しか反応しない
bool key_pressed(Canvas& canvas, int key, bool was_down[]) {
    const bool is_down = canvas.is_key_down(key);
    const bool pressed = is_down && !was_down[key];
    was_down[key] = is_down;
    return pressed;
}

// 点を絵の中に収める
Point clamp_point(Point p) {
    return Point{std::clamp(p.x, 0, scene_width - 1), std::clamp(p.y, 0, scene_height - 1)};
}

// 場面 3, 4 の最初の線。始点から右へ 10、上へ rise
void set_preset_line(int rise, Point& start, Point& end) {
    start = line_origin;
    end = Point{line_origin.x + 10, line_origin.y - rise};
}

// 線の真ん中のピクセルの中心
Vec2 line_center(Point start, Point end) {
    return Vec2{static_cast<float>(start.x + end.x) / 2.0f + 0.5f, static_cast<float>(start.y + end.y) / 2.0f + 0.5f};
}

// 今の線の誤差の表を、コンソールに出す。DDA が打ったピクセルも並べて、違うところに印を付ける
void print_trace(Point start, Point end, const std::vector<LineStep>& dda_trace, const std::vector<LineStep>& bres_trace) {
    const bool steep = std::abs(end.y - start.y) > std::abs(end.x - start.x);
    const int threshold = long_length(start, end);
    const int add = 2 * std::min(std::abs(end.x - start.x), std::abs(end.y - start.y));
    std::printf("line (%d, %d) -> (%d, %d): long axis %s, dx %d, dy %d\n", start.x, start.y, end.x, end.y, steep ? "y" : "x", threshold, add / 2);
    std::printf("  Bresenham goes from the smaller %s to the larger. each pixel: error += %d; if error > %d then step and error -= %d\n",
                steep ? "y" : "x", add, threshold, 2 * threshold);
    std::printf("  %3s  %-12s %-12s %8s %5s %7s\n", "i", "Bresenham", "DDA", "checked", "step", "error");
    for (size_t i = 0; i < bres_trace.size(); i++) {
        const LineStep& step = bres_trace[i];

        // DDA の記録から、同じ位置（長いほうの軸の値が同じ）のピクセルを探す
        char dda_text[24] = "-";
        bool different = false;
        for (const LineStep& dda : dda_trace) {
            if ((steep ? dda.y : dda.x) == (steep ? step.y : step.x)) {
                std::snprintf(dda_text, sizeof(dda_text), "(%d, %d)", dda.x, dda.y);
                different = (dda.x != step.x || dda.y != step.y);
                break;
            }
        }
        char bres_text[24];
        std::snprintf(bres_text, sizeof(bres_text), "(%d, %d)", step.x, step.y);

        // 基準とちょうど同じ値のマスの次のマスで、DDA とブレゼンハムが分かれる
        const char* note = "";
        if (different) {
            note = "  <- different";
        } else if (i + 1 < bres_trace.size() && step.checked == threshold) {
            note = "  <- exactly half";
        }
        std::printf("  %3d  %-12s %-12s %8d %5s %7d%s\n", static_cast<int>(i), bres_text, dda_text, step.checked, step.stepped ? "yes" : "no", step.error, note);
    }
}

int main() {
    Canvas canvas(320, 180, 4);  // 320×180 の絵を、4倍に拡大して表示する

    Image dda_image = make_image(scene_width, scene_height);
    Image bres_image = make_image(scene_width, scene_height);
    std::vector<LineStep> dda_trace;
    std::vector<LineStep> bres_trace;

    int scene = scene_side_by_side;
    // 場面ごとの表示。場面 1 は、DDA（そのまま）と difference（違いを赤で光らせる）の2つだけを切り替える
    Display displays[scene_count] = {Display::dda, Display::difference, Display::bresenham, Display::bresenham};
    bool show_grid = true;
    bool show_errors = true;
    bool show_true_line = false;

    // 場面 1, 2 の線の間隔
    const int spacings[3] = {20, 10, 5};
    int spacing_index = 0;

    // 場面 3, 4 の線
    Point start = {0, 0};
    Point end = {0, 0};
    set_preset_line(4, start, end);

    // 終点を回すときの、始点からの距離と角度（ラジアン）
    bool rotating = false;
    float rotate_radius = 0.0f;
    float rotate_angle = 0.0f;
    const float rotate_speed = 0.25f;

    // 場面ごとの、拡大の段階と、表示の中心
    const int default_zoom_index[scene_count] = {0, 0, 5, 5};
    int zoom_index[scene_count] = {0, 0, 5, 5};
    Vec2 center[scene_count] = {
        Vec2{static_cast<float>(scene_width) / 2.0f, static_cast<float>(scene_height) / 2.0f},
        Vec2{static_cast<float>(scene_width) / 2.0f, static_cast<float>(scene_height) / 2.0f},
        line_center(start, end),
        line_center(start, end),
    };

    bool was_down[256] = {};
    bool dirty = true;  // 線を引き直して、違うピクセルの数をコンソールに出すか

    std::printf("1: fans side by side  2: fan, differences in red  3: one line (10 right, 4 up)  4: one line (10 right, 5 up)\n");
    std::printf("V: display  Z/X: zoom  arrows: move view  C: grid  0: reset view  F: count all lines  M: measure speed  Esc: quit\n");
    std::printf("scene 1, 2: L: number of lines\n");
    std::printf("scene 3, 4: W/A/S/D: move end (with Shift: start)  Q: swap ends  R: rotate end  E: errors  T: true line  P: print table\n");

    while (canvas.update()) {
        if (canvas.is_key_down(key_escape)) {
            break;
        }

        // 場面の切り替え。3 と 4 は、同じ場面でもう一度押すと最初の線に戻る
        for (int k = 0; k < scene_count; k++) {
            if (!key_pressed(canvas, '1' + k, was_down)) {
                continue;
            }
            scene = k;
            if (scene >= scene_line_4) {
                set_preset_line((scene == scene_line_4) ? 4 : 5, start, end);
                rotating = false;
                center[scene] = line_center(start, end);
            }
            dirty = true;
            std::printf("scene %d\n", scene + 1);
        }

        if (key_pressed(canvas, 'V', was_down)) {
            if (scene == scene_side_by_side) {
                displays[scene] = (displays[scene] == Display::dda) ? Display::difference : Display::dda;
                std::printf("red marks: %s\n", (displays[scene] == Display::difference) ? "on" : "off");
            } else {
                displays[scene] = static_cast<Display>((static_cast<int>(displays[scene]) + 1) % 3);
                std::printf("display: %s\n", display_name(displays[scene]));
            }
        }
        if (key_pressed(canvas, 'E', was_down)) {
            show_errors = !show_errors;
            std::printf("errors: %s (shown at zoom 16 or more)\n", show_errors ? "on" : "off");
        }
        if (key_pressed(canvas, 'C', was_down)) {
            show_grid = !show_grid;
            std::printf("grid: %s (shown at zoom 8 or more)\n", show_grid ? "on" : "off");
        }
        if (key_pressed(canvas, 'T', was_down)) {
            show_true_line = !show_true_line;
            std::printf("true line: %s\n", show_true_line ? "on" : "off");
        }
        if (key_pressed(canvas, 'Z', was_down) && zoom_index[scene] < zoom_level_count - 1) {
            zoom_index[scene]++;
            std::printf("zoom: x%d\n", zoom_levels[zoom_index[scene]]);
        }
        if (key_pressed(canvas, 'X', was_down) && zoom_index[scene] > 0) {
            zoom_index[scene]--;
            std::printf("zoom: x%d\n", zoom_levels[zoom_index[scene]]);
        }
        if (key_pressed(canvas, '0', was_down)) {
            zoom_index[scene] = default_zoom_index[scene];
            if (scene >= scene_line_4) {
                center[scene] = line_center(start, end);
            } else {
                center[scene] = Vec2{static_cast<float>(dda_image.width) / 2.0f, static_cast<float>(dda_image.height) / 2.0f};
            }
        }

        if (scene < scene_line_4) {
            if (key_pressed(canvas, 'L', was_down)) {
                spacing_index = (spacing_index + 1) % 3;
                dirty = true;
                std::printf("lines: every %d pixels\n", spacings[spacing_index]);
            }
        } else {
            // 始点と終点を1マスずつ動かす。Shift を押しているときは始点
            Point& moved = canvas.is_key_down(key_shift) ? start : end;
            Point move = {0, 0};
            if (key_pressed(canvas, 'A', was_down)) {
                move.x = -1;
            }
            if (key_pressed(canvas, 'D', was_down)) {
                move.x = 1;
            }
            if (key_pressed(canvas, 'W', was_down)) {
                move.y = -1;
            }
            if (key_pressed(canvas, 'S', was_down)) {
                move.y = 1;
            }
            if (move.x != 0 || move.y != 0) {
                moved = clamp_point(Point{moved.x + move.x, moved.y + move.y});
                rotating = false;
                dirty = true;
            }

            if (key_pressed(canvas, 'Q', was_down)) {
                std::swap(start, end);
                rotating = false;
                dirty = true;
                std::printf("swapped: the line is drawn from the other end\n");
            }

            if (key_pressed(canvas, 'R', was_down)) {
                rotating = !rotating;
                if (rotating) {
                    // 今の向きから回し始める。見失わないよう、始点を表示の真ん中にする
                    const float dx = static_cast<float>(end.x - start.x);
                    const float dy = static_cast<float>(end.y - start.y);
                    rotate_radius = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
                    rotate_angle = std::atan2(dy, dx);
                    center[scene] = Vec2{static_cast<float>(start.x) + 0.5f, static_cast<float>(start.y) + 0.5f};
                }
                std::printf("rotate end: %s\n", rotating ? "on" : "off");
            }
            if (rotating) {
                rotate_angle += rotate_speed * canvas.delta_time;
                const Point new_end = clamp_point(Point{
                    start.x + static_cast<int>(std::round(rotate_radius * std::cos(rotate_angle))),
                    start.y + static_cast<int>(std::round(rotate_radius * std::sin(rotate_angle))),
                });
                if (new_end.x != end.x || new_end.y != end.y) {
                    end = new_end;
                    dirty = true;
                }
            }
        }

        // 数える、測る。どちらも、終わるまで画面は止まる
        if (key_pressed(canvas, 'F', was_down)) {
            std::printf("counting...\n");
            count_all_lines(60);
        }
        if (key_pressed(canvas, 'M', was_down)) {
            std::printf("measuring...\n");
            measure_speed();
        }

        // 線を引き直して、違うピクセルの数を出す
        if (dirty) {
            FanStats stats = {0, 0, 0};
            if (scene < scene_line_4) {
                stats = render_fan(dda_image, bres_image, spacings[spacing_index]);
            } else {
                render_line(dda_image, bres_image, start, end, dda_trace, bres_trace);
            }
            const Difference difference = compare_images(dda_image, bres_image);
            if (scene < scene_line_4) {
                std::printf("fan, every %d pixels, %dx%d: %d lines, DDA %d pixels, Bresenham %d pixels, different %d pixels (only DDA %d, only Bresenham %d)\n",
                            spacings[spacing_index], dda_image.width, dda_image.height, stats.lines, difference.dda_pixels, difference.bresenham_pixels,
                            difference.only_dda + difference.only_bresenham, difference.only_dda, difference.only_bresenham);
                std::printf("  lines that pass exactly through a half point: %d, half points: %d\n", stats.half_lines, stats.half_points);
            } else {
                std::printf("line (%d, %d) -> (%d, %d): half points %d, different pixels %d (only DDA %d, only Bresenham %d)\n", start.x, start.y, end.x, end.y,
                            count_half_points(bres_trace, long_length(start, end)), difference.only_dda + difference.only_bresenham, difference.only_dda,
                            difference.only_bresenham);
            }
            dirty = false;
        }
        if (scene >= scene_line_4 && key_pressed(canvas, 'P', was_down)) {
            print_trace(start, end, dda_trace, bres_trace);
        }

        // 矢印キーで表示の中心を動かす。拡大しているほど、ゆっくり動かす
        const int zoom = zoom_levels[zoom_index[scene]];
        const float speed = 160.0f / static_cast<float>(zoom) * canvas.delta_time;
        if (canvas.is_key_down(key_left)) {
            center[scene].x -= speed;
        }
        if (canvas.is_key_down(key_right)) {
            center[scene].x += speed;
        }
        if (canvas.is_key_down(key_up)) {
            center[scene].y -= speed;
        }
        if (canvas.is_key_down(key_down)) {
            center[scene].y += speed;
        }

        // 表示。間の線を残すため、まず灰色で全体を塗る
        for (Color& pixel : canvas.pixels) {
            pixel = from_hex(0x606060);
        }
        if (scene == scene_side_by_side) {
            // 左に DDA、右にブレゼンハム。2枚の絵は、同じ位置と倍率で見る
            const int dest_w = (canvas.width - 2) / 2;
            center[scene] = clamp_center(center[scene], zoom, dest_w, canvas.height, dda_image);
            const View left_view = make_view(0, 0, dest_w, canvas.height, center[scene], zoom);
            const View right_view = make_view(canvas.width - dest_w, 0, dest_w, canvas.height, center[scene], zoom);
            const bool mark = displays[scene] == Display::difference;
            draw_cells(canvas, dda_image, bres_image, Display::dda, mark, left_view, show_grid);
            draw_cells(canvas, dda_image, bres_image, Display::bresenham, mark, right_view, show_grid);
        } else {
            center[scene] = clamp_center(center[scene], zoom, canvas.width, canvas.height, dda_image);
            const View view = make_view(0, 0, canvas.width, canvas.height, center[scene], zoom);
            draw_cells(canvas, dda_image, bres_image, displays[scene], false, view, show_grid);
            if (scene >= scene_line_4) {
                if (show_true_line) {
                    draw_true_line(canvas, view, start, end);
                }
                if (show_errors) {
                    draw_errors(canvas, dda_image, bres_trace, long_length(start, end), displays[scene], view, show_grid);
                }
            }
        }
        draw_indicators(canvas, displays[scene]);

        canvas.present();
    }
}
