/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ButtonCtrl : public WindowBase {
    // creation parameters. must be set before Create() call

    ButtonCtrl(HWND parent, int menuId, RECT* initialPos);
    ~ButtonCtrl() override;

    bool Create() override;

    SIZE SetTextAndResize(const WCHAR*);
    SIZE GetIdealSize();
    void SetPos(RECT*);
};

struct ButtonLayout : public ILayout {
    ButtonCtrl* button = nullptr;

    ButtonLayout(ButtonCtrl*);
    virtual ~ButtonLayout();

    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length) override;
    Length MinIntrinsicWidth(Length) override;
    void SetBounds(const Rect bounds) override;
};

bool IsButton(Kind);
bool IsButton(ILayout*);

typedef std::function<void(bool)> CheckboxChangeCb;

struct CheckboxCtrl : public ILayout {
    // creation parameters. must be set before Create() call
    HWND parent = 0;
    RECT initialPos = {0, 0, 0, 0};
    int menuId = 0;
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    DWORD dwExStyle = 0;

    // called when it's checked / unchecked
    // TODO: implement me
    CheckboxChangeCb OnChange = nullptr;
    HWND hwnd = nullptr;
    HFONT hfont = nullptr;
    str::Str text;

    CheckboxCtrl(HWND parent, int menuId, RECT* initialPos);
    ~CheckboxCtrl();

    bool Create();

    SIZE SetTextAndResize(const WCHAR*);
    SIZE GetIdealSize();
    void SetPos(RECT*);
    void SetFont(HFONT);

    WCHAR* GetTextW();
    void SetText(std::string_view sv);
    void SetIsChecked(bool isChecked);
    bool IsChecked() const;

    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length) override;
    Length MinIntrinsicWidth(Length) override;
    void SetBounds(const Rect bounds) override;
};

bool IsCheckbox(Kind);
bool IsCheckbox(ILayout*);
