/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace wg {

LRESULT TryReflectNotify(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

enum WindowBorderStyle { kWindowBorderNone, kWindowBorderClient, kWindowBorderStatic };

struct Wnd : public ILayout {
    Wnd();
    Wnd(HWND hwnd);
    virtual ~Wnd();
    virtual void Destroy();

    virtual HWND Create(HWND parent);

    virtual Size GetIdealSize();

    // ILayout
    Kind GetKind() override;
    void SetVisibility(Visibility) override;
    Visibility GetVisibility() override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    Size Layout(Constraints bc) override;
    void SetBounds(Rect) override;
    void SetInsetsPt(int top, int right = -1, int bottom = -1, int left = -1);

    HWND CreateEx(DWORD exStyle, LPCTSTR className, LPCTSTR windowName, DWORD style, int x, int y, int width,
                  int height, HWND parent, HMENU idOrMenu, LPVOID lparam = NULL);

    virtual void PreCreate(CREATESTRUCT& cs);

    // we use wndClassName instead
    // virtual void PreRegisterClass(WNDCLASSEX& wc);

    virtual bool PreTranslateMessage(MSG& msg);

    void Attach(HWND hwnd);
    void AttachDlgItem(UINT id, HWND parent);

    HWND Detach();
    void Cleanup();

    void Subclass(HWND hwnd);
    // void UnSubclass();
    //  void SetDefaultFont();

    bool RegisterClass(WNDCLASSEX& wc) const;

    // Message handlers that can be
    virtual LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

    virtual void OnAttach();
    virtual bool OnCommand(WPARAM wparam, LPARAM lparam);
    virtual void OnClose();
    virtual int OnCreate(CREATESTRUCT*);
    virtual void OnDestroy();

    virtual void OnContextMenu(HWND hwnd, Point pt);
    virtual void OnDropFiles(HDROP drop_info);
    virtual void OnGetMinMaxInfo(MINMAXINFO* mmi);
    virtual LRESULT OnMouseEvent(UINT msg, WPARAM wparam, LPARAM lparam);
    virtual void OnMove(POINTS* pts);
    virtual LRESULT OnNotify(int controlId, NMHDR* nmh);
    virtual LRESULT OnNotifyReflect(WPARAM, LPARAM);
    virtual void OnPaint(HDC hdc, PAINTSTRUCT* ps);
    virtual bool OnEraseBkgnd(HDC dc);
    virtual void OnSize(UINT msg, UINT type, SIZE size);
    virtual void OnTaskbarCallback(UINT msg, LPARAM lparam);
    virtual void OnTimer(UINT_PTR event_id);
    virtual void OnWindowPosChanging(WINDOWPOS* window_pos);
    virtual LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);

    LRESULT WndProcDefault(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT FinalWindowProc(UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT MessageReflect(UINT msg, WPARAM wparam, LPARAM lparam);

    void SetPos(RECT* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(const WCHAR*);

    Kind kind = nullptr;
    // either a custom class that we registered or
    // a win32 control class. Assumed static so not freed
    const WCHAR* winClass = nullptr;

    Insets insets{};
    Size childSize{};
    Rect lastBounds{};

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    // TODO: move those to Frame subclass
    // HICON icon_large = nullptr;
    // HICON icon_small = nullptr;
    // HMENU menu = nullptr;

    // HFONT font = nullptr;
    WNDPROC prevWindowProc = nullptr;
    // HWND parent = nullptr;
    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;

    // set this for controls in the constructor
    const WCHAR* wndClassName = nullptr;
};

bool PreTranslateMessage(MSG& msg);

} // namespace wg

//- Button
namespace wg {
using ClickedHandler = std::function<void()>;

struct Button : Wnd {
    ClickedHandler onClicked = nullptr;
    bool isDefault = false;

    Button();
    ~Button() override;

    void PreCreate(CREATESTRUCT& cs) override;
    HWND Create(HWND parent) override;

    Size GetIdealSize() override;

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
};

Button* CreateButton(HWND parent, const WCHAR* s, const ClickedHandler& onClicked);

} // namespace wg

// Edit

namespace wg {
using TextChangedHandler = std::function<void()>;

struct Edit : Wnd {
    str::Str cueText;
    TextChangedHandler onTextChanged = nullptr;

    // set before Create()
    bool isMultiLine = false;
    int idealSizeLines = 1;
    int maxDx = 0;
    bool hasBorder = false;

    Edit();
    ~Edit();

    void PreCreate(CREATESTRUCT& cs) override;
    HWND Create(HWND parent) override;
    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetSelection(int start, int end);
    bool SetCueText(std::string_view);
};
} // namespace wg
