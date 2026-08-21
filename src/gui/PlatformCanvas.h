/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct Gfx;
enum class CursorId;

struct PlatformCanvas;

struct PlatformCanvasPaintEvent {
    PlatformCanvas* canvas = nullptr;
    Gfx* gfx = nullptr;
    Rect clientRect;
};

enum class PlatformCanvasPointerEventType {
    Move,
    Leave,
    Down,
    Up,
};

struct PlatformCanvasPointerEvent {
    PlatformCanvas* canvas = nullptr;
    PlatformCanvasPointerEventType type = PlatformCanvasPointerEventType::Move;
    Point pos;
    int button = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool didHandle = false;
};

struct PlatformCanvasScrollEvent {
    PlatformCanvas* canvas = nullptr;
    Point pos;
    double deltaX = 0;
    double deltaY = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool didHandle = false;
};

enum class PlatformKey {
    None,
    Escape,
    Enter,
    Home,
    End,
    PageUp,
    PageDown,
    Left,
    Up,
    Right,
    Down,
    Plus,
    Minus,
    Zero,
};

struct PlatformCanvasKeyEvent {
    PlatformCanvas* canvas = nullptr;
    PlatformKey key = PlatformKey::None;
    int codepoint = 0;
    bool isCtrl = false;
    bool isShift = false;
    bool isAlt = false;
    bool didHandle = false;
};

struct PlatformCanvas {
    void* native = nullptr;
    void* platformData = nullptr;
    void* userData = nullptr;

    Func1<PlatformCanvasPaintEvent*> onPaint;
    Func1<PlatformCanvasPointerEvent*> onPointer;
    Func1<PlatformCanvasScrollEvent*> onScroll;
    Func1<PlatformCanvasKeyEvent*> onKey;

    PlatformCanvas() = default;
    PlatformCanvas(const PlatformCanvas&) = delete;
    PlatformCanvas& operator=(const PlatformCanvas&) = delete;
    ~PlatformCanvas();

    static PlatformCanvas* Create(void* userData = nullptr);

    void* NativeWidget() const;
    Rect ClientRect() const;
    void Focus();
    void Invalidate();
    void SetCursor(CursorId);
};
