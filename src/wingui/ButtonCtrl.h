/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

using ClickedHandler = std::function<void()>;

struct ButtonCtrl : WindowBase {
    ClickedHandler onClicked = nullptr;
    bool isDefault = false;

    explicit ButtonCtrl(HWND parent);
    ~ButtonCtrl() override;
    bool Create() override;

    Size GetIdealSize() override;
};

ButtonCtrl* CreateButton(HWND parent, std::string_view s, const ClickedHandler& onClicked);
