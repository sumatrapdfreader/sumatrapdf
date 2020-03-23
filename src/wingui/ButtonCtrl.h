/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void()> ClickedHandler;

struct ButtonCtrl : WindowBase {
    ClickedHandler onClicked = nullptr;
    bool isDefault = false;

    ButtonCtrl(HWND parent);
    ~ButtonCtrl() override;
    bool Create() override;

    void WndProcParent(WndEvent*) override;

    SIZE GetIdealSize() override;
};

ILayout* NewButtonLayout(ButtonCtrl* b);

bool IsButton(Kind);
bool IsButton(ILayout*);

std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, const ClickedHandler& onClicked);
