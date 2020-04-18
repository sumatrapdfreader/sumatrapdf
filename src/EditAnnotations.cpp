/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/TreeModel.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/StaticCtrl.h"
#include "wingui/ButtonCtrl.h"
#include "wingui/ListBoxCtrl.h"
#include "wingui/DropDownCtrl.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineMulti.h"
#include "EngineManager.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "EditAnnotations.h"

using std::placeholders::_1;

struct EditAnnotationsWindow {
    Window* mainWindow = nullptr;
    HWND hwnd = nullptr;
    ILayout* mainLayout = nullptr;
    ListBoxCtrl* listBox = nullptr;
    DropDownCtrl* dropDownAdd = nullptr;
    ButtonCtrl* buttonCancel = nullptr;
    ButtonCtrl* buttonDelete = nullptr;

    ListBoxModel* lbModel = nullptr;

    // Not owned by us
    Vec<Annotation*>* annotations = nullptr;

    ~EditAnnotationsWindow();
    bool Create(Vec<Annotation*>* annots);
    void CreateMainLayout();
    void CloseHandler(WindowCloseEvent* ev);
    void SizeHandler(SizeEvent* ev);
    void ButtonCancelHandler();
    void ButtonDeleteHandler();
    void OnFinished();
    void DropDownAddSelectionChanged(DropDownSelectionChangedEvent* ev);
    void RebuildAnnotations(Vec<Annotation*>* annots);
};

static EditAnnotationsWindow* gEditAnnotationsWindow = nullptr;

EditAnnotationsWindow::~EditAnnotationsWindow() {
    delete mainWindow;
    delete mainLayout;
    delete lbModel;
}

void EditAnnotationsWindow::OnFinished() {
    // TODO: write me
}

void EditAnnotationsWindow::CloseHandler(WindowCloseEvent* ev) {
    // CrashIf(w != ev->w);
    OnFinished();
    delete gEditAnnotationsWindow;
    gEditAnnotationsWindow = nullptr;
}

void EditAnnotationsWindow::ButtonDeleteHandler() {
    MessageBoxNYI(hwnd);
}

void EditAnnotationsWindow::ButtonCancelHandler() {
    // gEditAnnotationsWindow->onFinished(nullptr);
    OnFinished();
    delete gEditAnnotationsWindow;
    gEditAnnotationsWindow = nullptr;
}

void EditAnnotationsWindow::DropDownAddSelectionChanged(DropDownSelectionChangedEvent* ev) {
    UNUSED(ev);
    // TODO: implement me
    MessageBoxNYI(hwnd);
}

void EditAnnotationsWindow::SizeHandler(SizeEvent* ev) {
    int dx = ev->dx;
    int dy = ev->dy;
    HWND hwnd = ev->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    ev->didHandle = true;
    InvalidateRect(hwnd, nullptr, false);
    if (dx == mainLayout->lastBounds.Dx() && dy == mainLayout->lastBounds.Dy()) {
        // avoid un-necessary layout
        return;
    }
    Size windowSize{dx, dy};
    auto c = Tight(windowSize);
    auto size = mainLayout->Layout(c);
    Point min{0, 0};
    Point max{size.dx, size.dy};
    Rect bounds{min, max};
    mainLayout->SetBounds(bounds);
}

// clang-format off
const char* gAnnotationTypes[] = {
    "Text",
    "Free Text",
    "Stamp",
    "Caret",
    "Ink",
    "Square",
    "Circle",
    "Line",
    "Polygon",
    // TODO: more
};
// clang-format on

void GetDropDownAddItems(Vec<std::string_view>& items) {
    int n = (int)dimof(gAnnotationTypes);
    for (int i = 0; i < n; i++) {
        const char* s = gAnnotationTypes[i];
        items.Append(s);
    }
}

void EditAnnotationsWindow::CreateMainLayout() {
    HWND parent = mainWindow->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        dropDownAdd = w;
        w->onDropDownSelectionChanged = std::bind(&EditAnnotationsWindow::DropDownAddSelectionChanged, this, _1);
        auto l = NewDropDownLayout(w);
        vbox->addChild(l);
        Vec<std::string_view> annotTypes;
        GetDropDownAddItems(annotTypes);
        w->SetItems(annotTypes);
        w->SetCueBanner("Add annotation...");
    }

    {
        auto w = new ListBoxCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        listBox = w;
        auto l = NewListBoxLayout(w);
        vbox->addChild(l, 1);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetText("Delete annotation");
        w->onClicked = std::bind(&EditAnnotationsWindow::ButtonDeleteHandler, this);
        bool ok = w->Create();
        CrashIf(!ok);
        buttonDelete = w;
        auto l = NewButtonLayout(w);
        vbox->addChild(l);
        w->SetIsEnabled(false);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetText("Close");
        w->onClicked = std::bind(&EditAnnotationsWindow::ButtonCancelHandler, this);
        bool ok = w->Create();
        CrashIf(!ok);
        buttonCancel = w;
        auto l = NewButtonLayout(w);
        vbox->addChild(l);
    }

    lbModel = new ListBoxModelStrings();
    listBox->SetModel(lbModel);
    mainLayout = vbox;
}

void EditAnnotationsWindow::RebuildAnnotations(Vec<Annotation*>* annots) {
    annotations = annots;

    auto model = new ListBoxModelStrings();
    int n = 0;
    if (annots) {
        n = annots->isize();
    }

    str::Str s;
    for (int i = 0; i < n; i++) {
        auto annot = annots->at(i);
        s.Reset();
        s.AppendFmt("page %d, ", annot->pageNo);
        s.AppendView(AnnotationName(annot->type));
        model->strings.Append(s.AsView());
    }

    listBox->SetModel(model);
    delete lbModel;
    lbModel = model;
}

bool EditAnnotationsWindow::Create(Vec<Annotation*>* annots) {
    auto w = new Window();
    // w->isDialog = true;
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Annotations");
    int dx = DpiScale(nullptr, 480);
    int dy = DpiScale(nullptr, 640);
    w->initialSize = {dx, dy};
    // PositionCloseTo(w, args->hwndRelatedTo);
    // SIZE winSize = {w->initialSize.dx, w->initialSize.Height};
    // LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    mainWindow = w;
    hwnd = w->hwnd;

    w->onClose = std::bind(&EditAnnotationsWindow::CloseHandler, this, _1);
    w->onSize = std::bind(&EditAnnotationsWindow::SizeHandler, this, _1);

    CreateMainLayout();
    RebuildAnnotations(annots);
    LayoutAndSizeToContent(mainLayout, 320, 640, w->hwnd);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    return true;
}

void StartEditAnnotations(TabInfo* tab) {
    UNUSED(tab);
    if (gEditAnnotationsWindow) {
        return;
    }
    Vec<Annotation*>* annots = nullptr;
    DisplayModel* dm = tab->AsFixed();
    if (dm) {
        annots = dm->userAnnots;
    }
    auto win = new EditAnnotationsWindow();
    gEditAnnotationsWindow = win;
    bool ok = win->Create(annots);
    CrashIf(!ok);
}
