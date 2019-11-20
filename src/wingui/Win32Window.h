/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */
   
class Win32Window {
  public:
    // creation parameters. must be set before Create() call
    HWND parent = nullptr;
    RECT initialPos = {};
    DWORD dwStyle = 0;
    DWORD dwExStyle = 0;

    // those tweak WNDCLASSEX for RegisterClass() class
    HICON hIcon = nullptr;
    HICON hIconSm = nullptr;
    LPCWSTR lpszMenuName = nullptr;

    // can be set any time
    MsgFilter preFilter = nullptr; // called at start of windows proc to allow intercepting commands
    WmCommandCb onCommand = nullptr;
    SizeCb onSize = nullptr;

    // public
    HWND hwnd = nullptr;

    explicit Win32Window(HWND parent, RECT* initialPosition);
    ~Win32Window();

    bool Create(const WCHAR* title);
};
