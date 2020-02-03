/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

typedef std::function<void()> ClickedHandler;

struct ButtonCtrl : public WindowBase {
    ClickedHandler onClicked = nullptr;

    ButtonCtrl(HWND parent);
    ~ButtonCtrl() override;
    bool Create() override;

    void WndProcParent(WndProcArgs*) override;

    SIZE GetIdealSize() override;
};

ILayout* NewButtonLayout(ButtonCtrl* b);

bool IsButton(Kind);
bool IsButton(ILayout*);

std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, const ClickedHandler& onClicked);
