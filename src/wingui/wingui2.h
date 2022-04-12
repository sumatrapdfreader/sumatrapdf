/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

namespace wg {

LRESULT TryReflectMessages(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

enum WindowBorderStyle { kWindowBorderNone, kWindowBorderClient, kWindowBorderStatic };

struct CreateControlArgs {
    HWND parent = nullptr;
    const WCHAR* className = nullptr;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos = {};
    HMENU ctrlId = 0;
    bool visible = true;
    HFONT font = nullptr;
    const char* text = nullptr;
};

struct CreateCustomArgs {
    HWND parent = nullptr;
    const WCHAR* className = nullptr;
    const WCHAR* title = nullptr;
    DWORD style = 0;
    DWORD exStyle = 0;
    Rect pos = {};
    HMENU menu = nullptr;
    bool visible = true;
    HFONT font = nullptr;
};

struct Wnd : public ILayout {
    Wnd();
    Wnd(HWND hwnd);
    virtual ~Wnd();
    virtual void Destroy();

    HWND CreateCustom(const CreateCustomArgs&);
    HWND CreateControl(const CreateControlArgs&);

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

    virtual bool PreTranslateMessage(MSG& msg);

    void Attach(HWND hwnd);
    void AttachDlgItem(UINT id, HWND parent);

    HWND Detach();
    void Cleanup();

    void Subclass();
    // void UnSubclass();

    // Message handlers that can be over-written
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

    void Close();
    void SetPos(RECT* r);
    void SetIsVisible(bool isVisible);
    bool IsVisible() const;
    void SetText(const WCHAR*);
    void SetText(const char*);
    void SetText(std::string_view);

    TempStr GetText();
    void SetIsEnabled(bool isEnabled) const;
    bool IsEnabled() const;
    void SetFocus() const;
    bool IsFocused() const;
    void SetRtl(bool) const;

    Kind kind = nullptr;

    Insets insets{};
    Size childSize{};
    Rect lastBounds{};

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    WNDPROC prevWindowProc = nullptr;
    HWND hwnd = nullptr;
    ILayout* layout = nullptr;
};

bool PreTranslateMessage(MSG& msg);

} // namespace wg

//- Static

namespace wg {
using ClickedHandler = std::function<void()>;

struct StaticCreateArgs {
    HWND parent = nullptr;
    const char* text = nullptr;
};

struct Static : Wnd {
    Static();
    ~Static() override = default;

    ClickedHandler onClicked = nullptr;

    HWND Create(const StaticCreateArgs&);

    Size GetIdealSize() override;

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

} // namespace wg

//- Button
namespace wg {

struct ButtonCreateArgs {
    HWND parent = nullptr;
    const char* text = nullptr;
};

struct Button : Wnd {
    ClickedHandler onClicked = nullptr;
    bool isDefault = false;

    Button();
    ~Button() override = default;

    HWND Create(const ButtonCreateArgs&);

    Size GetIdealSize() override;

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

Button* CreateButton(HWND parent, const WCHAR* s, const ClickedHandler& onClicked);
Button* CreateDefaultButton(HWND parent, const WCHAR* s);

} // namespace wg

// Edit

namespace wg {
using TextChangedHandler = std::function<void()>;

struct EditCreateArgs {
    HWND parent = nullptr;
    bool isMultiLine = false;
    bool withBorder = false;
    const char* cueText = nullptr;
};

struct Edit : Wnd {
    TextChangedHandler onTextChanged = nullptr;

    // set before Create()
    int idealSizeLines = 1;
    int maxDx = 0;

    Edit();
    ~Edit();

    HWND Create(const EditCreateArgs&);
    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetSelection(int start, int end);
    bool HasBorder();
};
} // namespace wg

namespace wg {

using ListBoxSelectionChangedHandler = std::function<void()>;
using ListBoxDoubleClickHandler = std::function<void()>;

struct ListBox : Wnd {
    ListBoxModel* model = nullptr;
    ListBoxSelectionChangedHandler onSelectionChanged = nullptr;
    ListBoxDoubleClickHandler onDoubleClick = nullptr;

    Size idealSize = {};
    int idealSizeLines = 0;

    ListBox();
    virtual ~ListBox();

    HWND Create(HWND parent);

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    int GetItemHeight(int);

    Size GetIdealSize() override;

    int GetCount();
    int GetCurrentSelection();
    bool SetCurrentSelection(int);
    void SetModel(ListBoxModel*);
};

} // namespace wg
