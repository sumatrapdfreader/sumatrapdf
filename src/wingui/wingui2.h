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
    void SetBackgroundColor(COLORREF);

    Kind kind = nullptr;

    Insets insets{};
    Size childSize{};
    Rect lastBounds{};

    // data that can be set before calling Create()
    Visibility visibility{Visibility::Visible};

    WNDPROC prevWindowProc = nullptr;
    HWND hwnd = nullptr;
    ILayout* layout = nullptr;

    COLORREF backgroundColor{ColorUnset};
    HBRUSH backgroundColorBrush = nullptr;
};

bool PreTranslateMessage(MSG& msg);

} // namespace wg

//- Static

namespace wg {
using ClickedHandler = std::function<void()>;

struct StaticCreateArgs {
    HWND parent = nullptr;
    HFONT font = nullptr;
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
    HFONT font = nullptr;
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
    int idealSizeLines = 1;
    HFONT font = nullptr;
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

struct ListBoxCreateArgs {
    HWND parent = nullptr;
    int idealSizeLines = 0;
    HFONT font = nullptr;
};

struct ListBox : Wnd {
    ListBoxModel* model = nullptr;
    ListBoxSelectionChangedHandler onSelectionChanged = nullptr;
    ListBoxDoubleClickHandler onDoubleClick = nullptr;

    Size idealSize = {};
    int idealSizeLines = 0;

    ListBox();
    virtual ~ListBox();

    HWND Create(const ListBoxCreateArgs&);

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

//- CheckboxCtrl

namespace wg {

enum class CheckState {
    Unchecked = BST_UNCHECKED,
    Checked = BST_CHECKED,
    Indeterminate = BST_INDETERMINATE,
};

using CheckboxStateChangedHandler = std::function<void()>;

struct CheckboxCreateArgs {
    HWND parent = nullptr;
    const char* text = nullptr;
    CheckState initialState = CheckState::Unchecked;
};

struct Checkbox : Wnd {
    CheckboxStateChangedHandler onCheckStateChanged = nullptr;

    Checkbox();
    ~Checkbox() override = default;
    HWND Create(const CheckboxCreateArgs&);

    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;

    void SetCheckState(CheckState);
    CheckState GetCheckState() const;

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;
};

} // namespace wg

//- Progress

namespace wg {

struct ProgressCreateArgs {
    HWND parent = nullptr;
    int initialMax = 0;
};

struct Progress : Wnd {
    Progress();
    ~Progress() override = default;

    int idealDx = 0;
    int idealDy = 0;

    HWND Create(const ProgressCreateArgs&);

    void SetMax(int);
    void SetCurrent(int);
    int GetMax();
    int GetCurrent();

    Size GetIdealSize() override;
};

} // namespace wg

//- DropDown

namespace wg {

using DropDownSelectionChangedHandler = std::function<void()>;

struct DropDownCreateArgs {
    HWND parent = nullptr;
    // TODO: model or items
};

struct DropDown : Wnd {
    // TODO: use DropDownModel
    Vec<std::string_view> items;
    DropDownSelectionChangedHandler onSelectionChanged = nullptr;

    DropDown();
    ~DropDown() override = default;
    HWND Create(const DropDownCreateArgs&);

    Size GetIdealSize() override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;

    int GetCurrentSelection();
    void SetCurrentSelection(int n);
    void SetItems(Vec<std::string_view>& newItems);
    void SetItemsSeqStrings(const char* items);
    void SetCueBanner(std::string_view);
};

} // namespace wg

//- Trackbar

namespace wg {

struct Trackbar;

struct TrackbarPosChangingEvent {
    Trackbar* trackbar = nullptr;
    int pos = -1;
    NMTRBTHUMBPOSCHANGING* info = nullptr;
};

using TrackbarPoschangingHandler = std::function<void(TrackbarPosChangingEvent*)>;

struct TrackbarCreateArgs {
    HWND parent = nullptr;
    bool isHorizontal = true;
    int rangeMin = 1;
    int rangeMax = 5;
};

struct Trackbar : Wnd {
    Size idealSize{};

    // for WM_NOTIFY with TRBN_THUMBPOSCHANGING
    TrackbarPoschangingHandler onPosChanging = nullptr;

    Trackbar();
    ~Trackbar() override = default;

    HWND Create(const TrackbarCreateArgs&);

    LRESULT OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) override;

    Size GetIdealSize() override;
    void SetRange(int min, int max);
    int GetRangeMin();
    int getRangeMax();

    void SetValue(int);
    int GetValue();
};

} // namespace wg

// - Splitter
namespace wg {
enum class SplitterType {
    Horiz,
    Vert,
};

struct Splitter;

// called when user drags the splitter ('done' is false) and when drag is finished ('done' is
// true). the owner can constrain splitter by using current cursor
// position and setting resizeAllowed to false if it's not allowed to go there
struct SplitterMoveEvent {
    Splitter* w = nullptr;
    bool done = false; // TODO: rename to finishedDragging
    // user can set to false to forbid resizing here
    bool resizeAllowed = true;
};

using SplitterMoveHandler = std::function<void(SplitterMoveEvent*)>;

struct SplitterCreateArgs {
    HWND parent = nullptr;
    SplitterType type = SplitterType::Horiz;
    bool isLive = true;
    COLORREF backgroundColor = ColorUnset;
};

struct Splitter : public Wnd {
    SplitterType type = SplitterType::Horiz;
    bool isLive = true;
    SplitterMoveHandler onSplitterMove = nullptr;
    COLORREF backgroundColor = 0;

    HBITMAP bmp = nullptr;
    HBRUSH brush = nullptr;

    Point prevResizeLinePos{};
    // if a parent clips children, DrawXorBar() doesn't work, so for
    // non-live resize, we need to remove WS_CLIPCHILDREN style from
    // parent and restore it when we're done
    bool parentClipsChildren = false;

    Splitter();
    ~Splitter() override;

    HWND Create(const SplitterCreateArgs&);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
};

} // namespace wg
