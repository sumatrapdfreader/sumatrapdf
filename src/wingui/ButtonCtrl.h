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
    i32 MinIntrinsicHeight(i32) override;
    i32 MinIntrinsicWidth(i32) override;
    void SetBounds(const Rect bounds) override;
};

bool IsButtonCtrl(Kind);

class CheckboxCtrl {
  public:
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

    // creation parameters. must be set before Create() call
    HWND parent = 0;
    RECT initialPos = {0, 0, 0, 0};
    int menuId = 0;
    DWORD dwStyle = WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX;
    DWORD dwExStyle = 0;

    HWND hwnd = nullptr;
};
