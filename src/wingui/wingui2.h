/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace wg {
enum WindowBorderStyle { kWindowBorderNone, kWindowBorderClient, kWindowBorderStatic };

struct Wnd {
    Wnd();
    Wnd(HWND hwnd);
    virtual ~Wnd();
    virtual void Destroy();

    virtual HWND Create(HWND parent);
    HWND Create(DWORD ex_style, LPCWSTR class_name, LPCWSTR window_name, DWORD style, int x, int y, int width,
                int height, HWND parent, HMENU menu, LPVOID param);
    virtual void PreCreate(CREATESTRUCT& cs);
    virtual void PreRegisterClass(WNDCLASSEX& wc);
    virtual bool PreTranslateMessage(MSG& msg);

    void Attach(HWND hwnd);
    HWND Detach();
    void SetWindowHandle(HWND hwnd);

    // message handlers
    virtual LRESULT WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    virtual LRESULT WindowProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    void Subclass(HWND hwnd);
    void UnSubclass();

    bool RegisterClass(WNDCLASSEX& wc) const;

    // Message handlers
    virtual LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);
    virtual BOOL OnCommand(WPARAM wparam, LPARAM lparam);
    // TODO: use Point
    virtual void OnContextMenu(HWND hwnd, POINT pt);
    virtual void OnCreate(HWND hwnd, CREATESTRUCT* create_struct);
    virtual BOOL OnDestroy();
    virtual void OnDropFiles(HDROP drop_info);
    virtual void OnGetMinMaxInfo(LPMINMAXINFO mmi);
    virtual LRESULT OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam);
    virtual void OnMove(POINTS* pts);
    virtual LRESULT OnNotify(int control_id, NMHDR* nmh);
    virtual void OnPaint(HDC hdc, PAINTSTRUCT* ps);
    virtual void OnSize(UINT msg, UINT type, SIZE size);
    virtual void OnTaskbarCallback(UINT msg, LPARAM lparam);
    virtual void OnTimer(UINT_PTR event_id);
    virtual void OnWindowPosChanging(WINDOWPOS* window_pos);

    CREATESTRUCT create_struct = {};
    WNDCLASSEX window_class = {};
    HFONT font = nullptr;
    HICON icon_large = nullptr;
    HICON icon_small = nullptr;
    HMENU menu = nullptr;
    WNDPROC prev_window_proc = nullptr;
    HWND parent = nullptr;
    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;
};

int MessageLoop();
bool PreTranslateMessage(MSG& msg);

} // namespace wg