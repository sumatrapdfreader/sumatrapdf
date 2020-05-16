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
    TabInfo* tab = nullptr;
    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;

    ListBoxCtrl* listBox = nullptr;
    StaticCtrl* staticRect = nullptr;
    StaticCtrl* staticAuthor = nullptr;
    DropDownCtrl* dropDownAdd = nullptr;
    ButtonCtrl* buttonCancel = nullptr;
    ButtonCtrl* buttonDelete = nullptr;

    ListBoxModel* lbModel = nullptr;

    Vec<Annotation*>* annotations = nullptr;

    ~EditAnnotationsWindow();
    bool Create();
    void CreateMainLayout();
    void CloseHandler(WindowCloseEvent* ev);
    void SizeHandler(SizeEvent* ev);
    void ButtonCancelHandler();
    void ButtonDeleteHandler();
    void CloseWindow();
    void ListBoxSelectionChanged(ListBoxSelectionChangedEvent* ev);
    void DropDownAddSelectionChanged(DropDownSelectionChangedEvent* ev);
    void RebuildAnnotations();
};

void DeleteEditAnnotationsWindow(EditAnnotationsWindow* w) {
    delete w;
}

EditAnnotationsWindow::~EditAnnotationsWindow() {
    int nAnnots = annotations->isize();
    for (int i = 0; i < nAnnots; i++) {
        Annotation* a = annotations->at(i);
        if (a->pdf_annot) {
            // hacky: only annotations with pdf_annot set belong to us
            delete a;
        }
    }
    delete annotations;
    delete mainWindow;
    delete mainLayout;
    delete lbModel;
}

void EditAnnotationsWindow::CloseWindow() {
    // TODO: more?
    tab->editAnnotsWindow = nullptr;
    delete this;
}

void EditAnnotationsWindow::CloseHandler(WindowCloseEvent* ev) {
    // CrashIf(w != ev->w);
    CloseWindow();
}

void EditAnnotationsWindow::ButtonDeleteHandler() {
    MessageBoxNYI(mainWindow->hwnd);
}

void EditAnnotationsWindow::ButtonCancelHandler() {
    CloseWindow();
}

static void ShowAnnotationRect(EditAnnotationsWindow* w, int annotNo) {
    w->staticRect->SetIsVisible(annotNo >= 0);
    if (annotNo < 0) {
        return;
    }
    Annotation* annot = w->annotations->at(annotNo);
    str::Str s;
    int x = (int)annot->rect.x;
    int y = (int)annot->rect.y;
    int dx = (int)annot->rect.Dx();
    int dy = (int)annot->rect.Dy();
    s.AppendFmt("Rect: %d %d %d %d", x, y, dx, dy);
    w->staticRect->SetText(s.as_view());
}

static void ShowAnnotationAuthor(EditAnnotationsWindow* w, int annotNo) {
    w->staticAuthor->SetIsVisible(annotNo >= 0);
    if (annotNo < 0) {
        return;
    }
    Annotation* annot = w->annotations->at(annotNo);
    if (annot->author.empty()) {
        w->staticAuthor->SetIsVisible(false);
        return;
    }
    str::Str s;
    s.AppendFmt("Author: %s", annot->author.c_str());
    w->staticAuthor->SetText(s.as_view());
}

void EditAnnotationsWindow::ListBoxSelectionChanged(ListBoxSelectionChangedEvent* ev) {
    // TODO: finish me
    int itemNo = ev->idx;
    bool itemSelected = (itemNo >= 0);
    buttonDelete->SetIsEnabled(itemSelected);
    ShowAnnotationRect(this, itemNo);
    ShowAnnotationAuthor(this, itemNo);
    Relayout(mainLayout);
    // TODO: go to page with selected annotation
    // MessageBoxNYI(mainWindow->hwnd);
}

void EditAnnotationsWindow::DropDownAddSelectionChanged(DropDownSelectionChangedEvent* ev) {
    UNUSED(ev);
    // TODO: implement me
    MessageBoxNYI(mainWindow->hwnd);
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
    if (mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    LayoutToSize(mainLayout, {dx, dy});
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
        w->onSelectionChanged = std::bind(&EditAnnotationsWindow::DropDownAddSelectionChanged, this, _1);
        auto l = NewDropDownLayout(w);
        vbox->AddChild(l);
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
        w->onSelectionChanged = std::bind(&EditAnnotationsWindow::ListBoxSelectionChanged, this, _1);
        auto l = NewListBoxLayout(w);
        vbox->AddChild(l, 1);
    }

    {
        auto w = new StaticCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        staticRect = w;
        auto l = NewStaticLayout(w);
        vbox->AddChild(l);
        w->SetIsVisible(false);
    }
    {
        auto w = new StaticCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        staticAuthor = w;
        auto l = NewStaticLayout(w);
        vbox->AddChild(l);
        w->SetIsVisible(false);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetText("Delete annotation");
        w->onClicked = std::bind(&EditAnnotationsWindow::ButtonDeleteHandler, this);
        bool ok = w->Create();
        CrashIf(!ok);
        buttonDelete = w;
        auto l = NewButtonLayout(w);
        vbox->AddChild(l);
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
        vbox->AddChild(l);
    }

    lbModel = new ListBoxModelStrings();
    listBox->SetModel(lbModel);
    mainLayout = vbox;
}

void EditAnnotationsWindow::RebuildAnnotations() {
    auto model = new ListBoxModelStrings();
    int n = 0;
    if (annotations) {
        n = annotations->isize();
    }

    str::Str s;
    for (int i = 0; i < n; i++) {
        auto annot = annotations->at(i);
        s.Reset();
        s.AppendFmt("page %d, ", annot->pageNo);
        s.AppendView(AnnotationName(annot->type));
        model->strings.Append(s.AsView());
    }

    listBox->SetModel(model);
    delete lbModel;
    lbModel = model;
}

bool EditAnnotationsWindow::Create() {
    auto w = new Window();
    // w->isDialog = true;
    w->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    w->SetTitle("Annotations");
    // int dx = DpiScale(nullptr, 480);
    // int dy = DpiScale(nullptr, 640);
    // w->initialSize = {dx, dy};
    // PositionCloseTo(w, args->hwndRelatedTo);
    // SIZE winSize = {w->initialSize.dx, w->initialSize.Height};
    // LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.cx, winSize.cy};
    bool ok = w->Create();
    CrashIf(!ok);

    mainWindow = w;

    w->onClose = std::bind(&EditAnnotationsWindow::CloseHandler, this, _1);
    w->onSize = std::bind(&EditAnnotationsWindow::SizeHandler, this, _1);

    CreateMainLayout();
    RebuildAnnotations();
    LayoutAndSizeToContent(mainLayout, 520, 720, w->hwnd);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    w->SetIsVisible(true);

    return true;
}

void StartEditAnnotations(TabInfo* tab) {
    if (tab->editAnnotsWindow) {
        HWND hwnd = tab->editAnnotsWindow->mainWindow->hwnd;
        BringWindowToTop(hwnd);
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    CrashIf(!dm);
    if (!dm) {
        return;
    }

    Vec<Annotation*>* annots = new Vec<Annotation*>();
    // those annotations are owned by us
    dm->GetEngine()->GetAnnotations(annots);

    // those annotations are owned by DisplayModel
    // TODO: for uniformity, make a copy of them
    if (dm->userAnnots) {
        for (auto a : *dm->userAnnots) {
            annots->Append(a);
        }
    }

    auto win = new EditAnnotationsWindow();
    win->tab = tab;
    tab->editAnnotsWindow = win;
    win->annotations = annots;
    bool ok = win->Create();
    CrashIf(!ok);
}
