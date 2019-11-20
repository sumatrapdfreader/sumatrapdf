class ButtonCtrl : public ILayout {
  public:
    // creation parameters. must be set before Create() call
    HWND parent = 0;
    RECT initialPos = {0, 0, 0, 0};
    int menuId = 0;
    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    DWORD dwExStyle = 0;

    HWND hwnd = nullptr;

    ButtonCtrl(HWND parent, int menuId, RECT* initialPos);
    ~ButtonCtrl() override;

    bool Create(const WCHAR*);

    SIZE SetTextAndResize(const WCHAR*);
    SIZE GetIdealSize();
    void SetPos(RECT*);
    void SetFont(HFONT);

    WCHAR* GetTextW();

    // ILayout
    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length) override;
    Length MinIntrinsicWidth(Length) override;
    void SetBounds(const Rect bounds) override;
};

bool IsButton(Kind);
bool IsButton(ILayout*);

typedef std::function<void(bool)> CheckboxChangeCb;

class CheckboxCtrl : public ILayout {
  public:
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

    CheckboxCtrl(HWND parent, int menuId, RECT* initialPos);
    ~CheckboxCtrl();

    bool Create(const WCHAR*);

    SIZE SetTextAndResize(const WCHAR*);
    SIZE GetIdealSize();
    void SetPos(RECT*);
    void SetFont(HFONT);

    WCHAR* GetTextW();

    void SetIsChecked(bool isChecked);
    bool IsChecked() const;

    // ILayout
    Size Layout(const Constraints bc) override;
    Length MinIntrinsicHeight(Length) override;
    Length MinIntrinsicWidth(Length) override;
    void SetBounds(const Rect bounds) override;
};

bool IsCheckbox(Kind);
bool IsCheckbox(ILayout*);
