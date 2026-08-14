/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// A VirtHost is a platform window whose content is a tree of virtual controls.
// It owns the window, the layout tree and the VirtRoot: it paints the tree
// double-buffered and routes the window's input into it.
//
// It exists so the code that builds the tree doesn't name an OS windowing API.
// Instead of an HWND and SetWindowPos / InvalidateRect / SetTimer / GetFocus,
// that code asks the host to move itself, repaint, run a timer, or say where it
// is on the screen. The window is created per platform (VirtHost_win.cpp).
//
// Whatever the host doesn't model - the colors of a native child control,
// dragging the window by its background - goes through onNativeMsg, which only
// exists on Windows and is meant to stay small.

struct ILayout;
struct VirtRoot;
struct Gfx;
struct PlatformFont;
struct VirtHost;

// the platform's window handle; only the platform-specific files touch it
#if OS_WIN
using NativeWnd = HWND;
#else
using NativeWnd = void*;
#endif

struct VirtHostPaintEvent {
    VirtHost* host = nullptr;
    Gfx* gfx = nullptr;
    Rect clientRect;
};

#if OS_WIN
struct VirtHostNativeMsg {
    VirtHost* host = nullptr;
    UINT msg = 0;
    WPARAM wp = 0;
    LPARAM lp = 0;
    // the value to return from the window procedure when didHandle is set
    LRESULT res = 0;
    bool didHandle = false;
};
#endif

struct VirtHost {
    struct CreateArgs {
        NativeWnd parent = nullptr;
        // unique per kind of host; the window class is registered on demand.
        // Must stay alive for the whole run (a string literal via WStrL)
        WStr className;
        Size initialSize;
        Color bgColor = kColorUnset;
        bool isRtl = false;
        bool visible = true;
        // clicking the host doesn't move the keyboard focus, so the frame keeps
        // it and its accelerators keep working
        bool noActivate = false;
        // don't let a lower-Z sibling paint over us (for a floating host)
        bool clipSiblings = false;
        // the owner, for the callbacks to find their way back
        void* userData = nullptr;
    };

    NativeWnd native = nullptr;
    // the tree of controls the host shows; owned
    ILayout* layout = nullptr;
    // the virtual controls of the tree, created on demand by Relayout(); owned
    VirtRoot* vroot = nullptr;
    Color bgColor = kColorUnset;
    bool noActivate = false;
    void* userData = nullptr;

    // fills the background; the host fills it with bgColor when this is not set
    Func1<VirtHostPaintEvent*> onPaintBackground;
    // draws on top of the tree
    Func1<VirtHostPaintEvent*> onPaint;
    // the mouse moved over the host, or left it
    Func0 onMouseMove;
    Func0 onMouseLeave;
    // a timer started with SetTimer() fired; the argument is the timer's id
    Func1<int> onTimer;
#if OS_WIN
    Func1<VirtHostNativeMsg*> onNativeMsg;
#endif

    VirtHost() = default;
    VirtHost(const VirtHost&) = delete;
    VirtHost& operator=(const VirtHost&) = delete;
    ~VirtHost();

    static VirtHost* Create(const CreateArgs&);

    // takes ownership of the tree and lays it out
    void SetLayout(ILayout*);
    void Relayout();

    Rect ClientRect() const;
    // the host's rectangle, in screen coordinates
    Rect ScreenRect() const;
    // a screen point in the host's client coordinates
    Point FromScreen(Point) const;
    // a rect in the host's client coordinates, in screen coordinates
    Rect ToScreen(Rect) const;
    // move and resize (in the parent's client coordinates), and show or hide
    void SetPos(Rect, bool visible);
    bool IsVisible() const;
    void Invalidate(bool erase = true);
    // invalidate and paint right away
    void Repaint();
    // the host or one of its children has the keyboard focus
    bool HasFocus() const;
    // the screen point is over the host or one of its children
    bool ContainsScreenPoint(Point) const;
    void SetTimer(int id, int delayMs);
    void KillTimer(int id);
    // the font the host's native children inherit
    void SetFont(PlatformFont*);
};

// where the mouse cursor is, in screen coordinates
Point UiCursorScreenPos();
// height of a horizontal scrollbar the OS draws
int UiHScrollbarDy();
