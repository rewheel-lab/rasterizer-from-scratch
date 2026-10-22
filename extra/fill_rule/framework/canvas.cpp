#include "canvas.h"

#include <string>

// windows.h が余計な機能や min / max マクロを持ち込まないようにする
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

const wchar_t* window_class_name = L"CanvasWindow";

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_DESTROY) {
        // 窓が閉じられたら、update() が false を返せるように終了を知らせる
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

long long now_ticks() {
    LARGE_INTEGER counter;
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

long long ticks_per_second() {
    LARGE_INTEGER frequency;
    QueryPerformanceFrequency(&frequency);
    return frequency.QuadPart;
}

// 窓のタイトルを作る。exe の名前（ep02_done など）を入れて、
// 視聴者や録画から、どの回のフォルダを動かしているか分かるようにする
std::wstring make_window_title() {
    const std::wstring channel_name = L"車輪の再発明ラボ";

    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    // 取れなかったとき（長すぎて切れたときも含む）は、チャンネル名だけにする
    if (length == 0 || length >= MAX_PATH) {
        return channel_name;
    }

    // フォルダの部分と拡張子を取り除いて、ファイル名だけにする
    std::wstring name(path, length);
    const size_t slash = name.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        name = name.substr(slash + 1);
    }
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) {
        name = name.substr(0, dot);
    }
    if (name.empty()) {
        return channel_name;
    }
    return name + L" - " + channel_name;
}

}  // namespace

// 引数の名前がメンバーと同じだと警告が出るので、ここでは短い名前にする
Canvas::Canvas(int w, int h, int s)
    : width(w),
      height(h),
      pixels(static_cast<size_t>(w) * static_cast<size_t>(h), Color{0, 0, 0}),
      scale(s),
      screen(static_cast<size_t>(w) * static_cast<size_t>(h), 0) {
    // 表示スケール（125% など）で窓がぼやけないよう、DPI 対応にする
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = GetModuleHandleW(nullptr);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = window_class_name;
    // 2つ目以降の Canvas では登録に失敗するが、登録済みなので問題ない
    RegisterClassExW(&window_class);

    // 大きさを変えられない窓にする（絵の大きさと表示の大きさがずれないように）
    const DWORD style = static_cast<DWORD>(WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX);
    // 枠やタイトルバーの分を足して、クライアント領域をぴったり width * scale にする
    RECT rect = {0, 0, width * scale, height * scale};
    AdjustWindowRectExForDpi(&rect, style, FALSE, 0, GetDpiForSystem());

    const std::wstring title = make_window_title();
    HWND hwnd = CreateWindowExW(
        0, window_class_name, title.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, window_class.hInstance, nullptr);
    window = hwnd;
    ShowWindow(hwnd, SW_SHOW);

    start_ticks = now_ticks();
}

Canvas::~Canvas() {
    if (window != nullptr) {
        DestroyWindow(static_cast<HWND>(window));
    }
}

bool Canvas::update() {
    if (window == nullptr) {
        return false;
    }

    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            // 窓はもう壊れているので、デストラクタで壊さないようにする
            window = nullptr;
            return false;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    const long long ticks = now_ticks();
    const double frequency = static_cast<double>(ticks_per_second());
    time = static_cast<float>(static_cast<double>(ticks - start_ticks) / frequency);

    POINT cursor;
    GetCursorPos(&cursor);

    if (is_first_update) {
        // 比べる相手がいないので、最初のフレームは動いていないことにする
        delta_time = 0.0f;
        mouse_dx = 0.0f;
        mouse_dy = 0.0f;
        is_first_update = false;
    } else {
        delta_time = static_cast<float>(static_cast<double>(ticks - last_ticks) / frequency);
        // 画面全体での移動量。窓の拡大率では割らない
        mouse_dx = static_cast<float>(cursor.x - last_cursor_x);
        mouse_dy = static_cast<float>(cursor.y - last_cursor_y);
    }
    last_ticks = ticks;
    last_cursor_x = cursor.x;
    last_cursor_y = cursor.y;

    return true;
}

void Canvas::present() {
    if (window == nullptr) {
        return;
    }

    // Color は r, g, b の順。Windows は 0x00RRGGBB の数値（メモリ上では B, G, R の順）を求める
    for (size_t i = 0; i < pixels.size(); i++) {
        const Color color = pixels[i];
        screen[i] = (static_cast<uint32_t>(color.r) << 16) |
                    (static_cast<uint32_t>(color.g) << 8) |
                    static_cast<uint32_t>(color.b);
    }

    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // 負にすると、上の行から順に並んだ絵として扱われる
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    HWND hwnd = static_cast<HWND>(window);
    HDC dc = GetDC(hwnd);
    // 拡大するときに色を混ぜない（ピクセルをくっきり見せる）
    SetStretchBltMode(dc, COLORONCOLOR);
    StretchDIBits(dc,
                  0, 0, width * scale, height * scale,
                  0, 0, width, height,
                  screen.data(), &info, DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(hwnd, dc);
}

bool Canvas::is_key_down(int key) {
    // 別の窓を触っているときに反応しないよう、この窓が前面にあるときだけ調べる
    if (GetForegroundWindow() != static_cast<HWND>(window)) {
        return false;
    }
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

bool Canvas::is_mouse_down(int button) {
    if (GetForegroundWindow() != static_cast<HWND>(window)) {
        return false;
    }
    const int key = (button == 0) ? VK_LBUTTON : VK_RBUTTON;
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}
