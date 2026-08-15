/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// A small top-level drawing window used by portable, canvas-style dialogs.
// Native toolkit types stay in the platform implementations.

struct Gfx;
enum class CursorId;

#if OS_WIN
using NativeWnd = HWND;
#else
using NativeWnd = void*;
#endif

struct PlatformWindow;

struct PlatformWindowPaintEvent {
    PlatformWindow* window = nullptr;
    Gfx* gfx = nullptr;
    Rect clientRect;
};

enum class PlatformPointerEventType {
    Move,
    Leave,
    Down,
    Up,
};

struct PlatformPointerEvent {
    PlatformWindow* window = nullptr;
    PlatformPointerEventType type = PlatformPointerEventType::Move;
    Point pos;
    int button = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool didHandle = false;
};

struct PlatformKeyEvent {
    PlatformWindow* window = nullptr;
    int codepoint = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool didHandle = false;
};

struct PlatformWindow {
    struct CreateArgs {
        NativeWnd parent = nullptr;
        Str title;
        Size initialSize;
        bool visible = true;
        bool frameless = false;
        bool resizable = false;
        void* userData = nullptr;
    };

    NativeWnd native = nullptr;
    void* platformData = nullptr;
    void* userData = nullptr;

    Func1<PlatformWindowPaintEvent*> onPaint;
    Func1<PlatformPointerEvent*> onPointer;
    Func1<PlatformKeyEvent*> onKey;
    Func0 onCloseRequest;

    PlatformWindow() = default;
    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;
    ~PlatformWindow();

    static PlatformWindow* Create(const CreateArgs&);

    Rect ClientRect() const;
    Rect ScreenRect() const;
    void SetBounds(Rect);
    void Show(bool);
    void Focus();
    void Invalidate();
    void SetCursor(CursorId);
    void BeginMove(const PlatformPointerEvent&);
};

Rect PlatformWindowRect(NativeWnd);
Rect PlatformWindowWorkArea(NativeWnd);
bool PlatformWindowIsMaximized(NativeWnd);
void PlatformWindowActivateIfForeground(NativeWnd);
void PlatformPostTask(const Func0&);
