/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

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
#include "wingui/EditCtrl.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineMulti.h"
#include "EngineManager.h"

#include "SumatraConfig.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "EditAnnotations.h"

using std::placeholders::_1;

// clang-format off
// TODO: convert to flat string
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
    nullptr,
};

const char* gTextIcons[] = {
    "Comment", 
    "Help", 
    "Insert", 
    "Key", 
    "NewParagraph", 
    "Note", 
    "Paragraph",
    nullptr,
};

const char *gFileAttachmentUcons[] = { 
    "Graph",
    "Paperclip",
    "PushPin",
    "Tag",
    nullptr,
 };

const char *gSoundIcons[] = {
    "Speaker",
    "Mic",
    nullptr,
};

const char *gStampIcons[] = {
    "Approved", 
    "AsIs",
    "Confidential",
    "Departmental",
    "Draft",
    "Experimental",
    "Expired",
    "Final",
    "ForComment",
    "ForPublicRelease",
    "NotApproved",
    "NotForPublicRelease",
    "Sold",
    "TopSecret",
    nullptr,
};

const char* gColors[] = {
    "None",
    "Aqua",
    "Black",
    "Blue",
    "Fuchsia",
    "Gray",
    "Green",
    "Lime",
    "Maroon",
    "Navy",
    "Olive",
    "Orange",
    "Purple",
    "Red",
    "Silver",
    "Teal",
    "White",
    "Yellow",
    nullptr,
};

static unsigned int gColorsValues[] = {
    0x00000000, /* transparent */
    0xff00ffff, /* aqua */
    0xff000000, /* black */
    0xff0000ff, /* blue */
    0xffff00ff, /* fuchsia */
    0xff808080, /* gray */
    0xff008000, /* green */
    0xff00ff00, /* lime */
    0xff800000, /* maroon */
    0xff000080, /* navy */
    0xff808000, /* olive */
    0xffffa500, /* orange */
    0xff800080, /* purple */
    0xffff0000, /* red */
    0xffc0c0c0, /* silver */
    0xff008080, /* teal */
    0xffffffff, /* white */
    0xffffff00, /* yellow */
    0, // match nullptr
};

// clang-format on

static_assert(dimof(gColors) == dimof(gColorsValues));

const char* GetKnownColorName(COLORREF c) {
    if (c == ColorUnset) {
        return gColors[0];
    }
    // convert COLORREF to a format in gColorsValues
    u8 r, g, b;
    UnpackRgb(c, r, g, b);
    COLORREF c2 = MkRgba(b, g, r, 0xff);
    for (int i = 1; gColors[i]; i++) {
        if (c2 == gColorsValues[i]) {
            return gColors[i];
        }
    }
    return nullptr;
}

struct EditAnnotationsWindow {
    TabInfo* tab = nullptr;
    Window* mainWindow = nullptr;
    ILayout* mainLayout = nullptr;

    DropDownCtrl* dropDownAdd = nullptr;

    ListBoxCtrl* listBox = nullptr;
    StaticCtrl* staticRect = nullptr;
    StaticCtrl* staticAuthor = nullptr;
    StaticCtrl* staticModificationDate = nullptr;

    StaticCtrl* staticPopup = nullptr;
    StaticCtrl* staticContents = nullptr;
    EditCtrl* editContents = nullptr;
    StaticCtrl* staticIcon = nullptr;
    DropDownCtrl* dropDownIcon = nullptr;
    StaticCtrl* staticColor = nullptr;
    DropDownCtrl* dropDownColor = nullptr;
    ButtonCtrl* buttonDelete = nullptr;

    ButtonCtrl* buttonSavePDF = nullptr;
    // TODO: not sure if want buttonCancel
    ButtonCtrl* buttonCancel = nullptr;

    ListBoxModel* lbModel = nullptr;

    Vec<Annotation*>* annotations = nullptr;
    // currently selected annotation
    Annotation* annot = nullptr;

    ~EditAnnotationsWindow();
    bool Create();
    void CreateMainLayout();
    void CloseHandler(WindowCloseEvent* ev);
    void SizeHandler(SizeEvent* ev);
    void ButtonCancelHandler();
    void ButtonSavePDFHandler();
    void ButtonDeleteHandler();
    void CloseWindow();
    void ListBoxSelectionChanged(ListBoxSelectionChangedEvent* ev);
    void DropDownAddSelectionChanged(DropDownSelectionChangedEvent* ev);
    void DropDownIconSelectionChanged(DropDownSelectionChangedEvent* ev);
    void DropDownColorSelectionChanged(DropDownSelectionChangedEvent* ev);
    void RebuildAnnotations();
};

// static
int FindStringInArray(const char** items, const char* toFind, int valIfNotFound = -1) {
    for (int i = 0; items[i] != nullptr; i++) {
        const char* s = items[i];
        if (str::Eq(s, toFind)) {
            return i;
        }
    }
    return valIfNotFound;
}

static void DropDownItemsFromStringArray(Vec<std::string_view>& items, const char** strings) {
    for (int i = 0; strings[i] != nullptr; i++) {
        const char* s = strings[i];
        items.Append(s);
    }
}

void DeleteEditAnnotationsWindow(EditAnnotationsWindow* w) {
    delete w;
}

EditAnnotationsWindow::~EditAnnotationsWindow() {
    int nAnnots = annotations->isize();
    for (int i = 0; i < nAnnots; i++) {
        Annotation* a = annotations->at(i);
        if (a->pdf) {
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

void EditAnnotationsWindow::ButtonSavePDFHandler() {
    MessageBoxNYI(mainWindow->hwnd);
}

void ShowAnnotationRect(EditAnnotationsWindow* w, Annotation* annot) {
    bool isVisible = (annot != nullptr);
    w->staticRect->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    str::Str s;
    RectD rect = annot->Rect();
    int x = (int)rect.x;
    int y = (int)rect.y;
    int dx = (int)rect.Dx();
    int dy = (int)rect.Dy();
    s.AppendFmt("Rect: %d %d %d %d", x, y, dx, dy);
    w->staticRect->SetText(s.as_view());
}

static void ShowAnnotationAuthor(EditAnnotationsWindow* w, Annotation* annot) {
    bool isVisible = (annot != nullptr) && !annot->Author().empty();
    w->staticAuthor->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.AppendFmt("Author: %s", annot->Author().data());
    w->staticAuthor->SetText(s.as_view());
}

static void AppendPdfDate(str::Str& s, time_t secs) {
    struct tm* tm = gmtime(&secs);
    char buf[100];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", tm);
    s.Append(buf);
}

static void ShowAnnotationModificationDate(EditAnnotationsWindow* w, Annotation* annot) {
    bool isVisible = (annot != nullptr) && (annot->ModificationDate() != 0);
    w->staticModificationDate->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.Append("Date: ");
    AppendPdfDate(s, annot->ModificationDate());
    w->staticModificationDate->SetText(s.as_view());
}

static void ShowAnnotationsPopup(EditAnnotationsWindow* w, Annotation* annot) {
    int popupId = annot ? annot->PopupId() : -1;
    bool isVisible = popupId >= 0;
    if (!isVisible) {
        w->staticPopup->SetIsVisible(isVisible);
        return;
    }
    str::Str s;
    s.AppendFmt("Popup: %d 0 R", popupId);
    w->staticPopup->SetIsVisible(isVisible);
    w->staticPopup->SetText(s.as_view());
}

static void ShowAnnotationsContents(EditAnnotationsWindow* w, Annotation* annot) {
    bool isVisible = (annot != nullptr);
    w->staticContents->SetIsVisible(isVisible);
    w->editContents->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    str::Str s = annot->Contents();
    // TODO: don't replace if already is "\r\n"
    s.Replace("\n", "\r\n");
    w->editContents->SetText(s.as_view());
}

static void ShowAnnotationsIcon(EditAnnotationsWindow* w, Annotation* annot) {
    std::string_view iconName;
    if (annot) {
        iconName = annot->IconName();
    }
    bool isVisible = !iconName.empty();
    const char** icons = nullptr;
    if (isVisible) {
        switch (annot->type) {
            case AnnotationType::Text:
                icons = gTextIcons;
                break;
            case AnnotationType::FileAttachment:
                icons = gFileAttachmentUcons;
                break;
            case AnnotationType::Sound:
                icons = gSoundIcons;
                break;
            case AnnotationType::Stamp:
                icons = gStampIcons;
                break;
        }
    }
    if (!icons) {
        isVisible = false;
    }
    w->staticIcon->SetIsVisible(isVisible);
    w->dropDownIcon->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    Vec<std::string_view> strings;
    DropDownItemsFromStringArray(strings, icons);
    w->dropDownIcon->SetItems(strings);
    int idx = FindStringInArray(icons, iconName.data(), 0);
    w->dropDownIcon->SetCurrentSelection(idx);
}

int ShouldEditBorder(AnnotationType subtype) {
    switch (subtype) {
        default:
            return 0;
        case AnnotationType::FreeText:
            return 1;
        case AnnotationType::Ink:
        case AnnotationType::Line:
        case AnnotationType::Square:
        case AnnotationType::Circle:
        case AnnotationType::Polygon:
        case AnnotationType::PolyLine:
            return 1;
    }
}

int ShouldEditInteriorColor(AnnotationType subtype) {
    switch (subtype) {
        default:
            return 0;
        case AnnotationType::Line:
        case AnnotationType::Square:
        case AnnotationType::Circle:
            return 1;
    }
}

// static
int ShouldEditColor(AnnotationType subtype) {
    switch (subtype) {
        default:
            return 0;
        case AnnotationType::Stamp:
        case AnnotationType::Text:
        case AnnotationType::FileAttachment:
        case AnnotationType::Sound:
        case AnnotationType::Caret:
            return 1;
        case AnnotationType::FreeText:
            return 1;
        case AnnotationType::Ink:
        case AnnotationType::Line:
        case AnnotationType::Square:
        case AnnotationType::Circle:
        case AnnotationType::Polygon:
        case AnnotationType::PolyLine:
            return 1;
        case AnnotationType::Highlight:
        case AnnotationType::Underline:
        case AnnotationType::StrikeOut:
        case AnnotationType::Squiggly:
            return 1;
    }
}

static void ShowAnnotationsColor(EditAnnotationsWindow* w, Annotation* annot) {
    auto annotType = annot ? annot->Type() : AnnotationType::Unknown;
    bool isVisible = ShouldEditColor(annotType);
    w->staticColor->SetIsVisible(isVisible);
    w->dropDownColor->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    COLORREF col = annot->Color();
    Vec<std::string_view> strings;
    DropDownItemsFromStringArray(strings, gColors);
    w->dropDownColor->SetItems(strings);
    const char* colorName = GetKnownColorName(col);
    int idx = FindStringInArray(gColors, colorName, 0);
    if (idx == -1) {
        // TODO: not a known color name, so add hex version to the list
    }
    w->dropDownColor->SetCurrentSelection(idx);
}

void EditAnnotationsWindow::ListBoxSelectionChanged(ListBoxSelectionChangedEvent* ev) {
    // TODO: finish me
    int itemNo = ev->idx;
    annot = nullptr;
    if (itemNo >= 0) {
        annot = annotations->at(itemNo);
    }
    // TODO: mupdf shows it in 1.6 but not 1.7. Why?
    // ShowAnnotationRect(this, annot);
    ShowAnnotationAuthor(this, annot);
    ShowAnnotationModificationDate(this, annot);
    ShowAnnotationsPopup(this, annot);
    ShowAnnotationsContents(this, annot);
    // TODO: PDF_ANNOT_FREE_TEXT
    // TODO: PDF_ANNOT_LINE
    ShowAnnotationsIcon(this, annot);
    // TODO: border
    ShowAnnotationsColor(this, annot);
    // TODO: icolor
    // TODO: quad points
    // TODO: vertices
    // TODO: ink list
    // TODO: PDF_ANNOT_FILE_ATTACHMENT
    buttonDelete->SetIsVisible(annot != nullptr);
    Relayout(mainLayout);
    // TODO: go to page with selected annotation
}

static void EnableSaveIfAnnotationsChanged(EditAnnotationsWindow* w) {
    bool didChange = false;
    if (w->annotations) {
        for (auto& annot : *w->annotations) {
            if (annot->isChanged || annot->isDeleted) {
                didChange = true;
                break;
            }
        }
    }
    w->buttonSavePDF->SetIsEnabled(didChange);
}

void EditAnnotationsWindow::DropDownAddSelectionChanged(DropDownSelectionChangedEvent* ev) {
    UNUSED(ev);
    // TODO: implement me
    MessageBoxNYI(mainWindow->hwnd);
}

void EditAnnotationsWindow::DropDownIconSelectionChanged(DropDownSelectionChangedEvent* ev) {
    annot->SetIconName(ev->item);
    EnableSaveIfAnnotationsChanged(this);
}

void EditAnnotationsWindow::DropDownColorSelectionChanged(DropDownSelectionChangedEvent* ev) {
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
    if (false && mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    LayoutToSize(mainLayout, {dx, dy});
}

static std::tuple<StaticCtrl*, ILayout*> CreateStatic(HWND parent, std::string_view sv = {}) {
    auto w = new StaticCtrl(parent);
    bool ok = w->Create();
    CrashIf(!ok);
    w->SetText(sv);
    w->SetIsVisible(false);
    auto l = NewStaticLayout(w);
    return {w, l};
}

void EditAnnotationsWindow::CreateMainLayout() {
    HWND parent = mainWindow->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    ILayout* l = nullptr;

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        dropDownAdd = w;
        w->onSelectionChanged = std::bind(&EditAnnotationsWindow::DropDownAddSelectionChanged, this, _1);
        l = NewDropDownLayout(w);
        vbox->AddChild(l);
        Vec<std::string_view> annotTypes;
        DropDownItemsFromStringArray(annotTypes, gAnnotationTypes);
        w->SetItems(annotTypes);
        w->SetCueBanner("Add annotation...");
    }

    {
        auto w = new ListBoxCtrl(parent);
        w->idealSizeLines = 5;
        bool ok = w->Create();
        CrashIf(!ok);
        listBox = w;
        w->onSelectionChanged = std::bind(&EditAnnotationsWindow::ListBoxSelectionChanged, this, _1);
        l = NewListBoxLayout(w);
        vbox->AddChild(l);

        lbModel = new ListBoxModelStrings();
        listBox->SetModel(lbModel);
    }

    {
        std::tie(staticRect, l) = CreateStatic(parent);
        vbox->AddChild(l);
    }

    {
        std::tie(staticAuthor, l) = CreateStatic(parent);
        vbox->AddChild(l);
    }

    {
        std::tie(staticModificationDate, l) = CreateStatic(parent);
        vbox->AddChild(l);
    }

    {
        std::tie(staticPopup, l) = CreateStatic(parent);
        vbox->AddChild(l);
    }

    {
        std::tie(staticContents, l) = CreateStatic(parent, "Contents:");
        vbox->AddChild(l);
    }

    {
        auto w = new EditCtrl(parent);
        w->isMultiLine = true;
        w->idealSizeLines = 5;
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        editContents = w;
        // TODO: hookup change request
        l = NewEditLayout(w);
        vbox->AddChild(l);
    }

    {
        std::tie(staticIcon, l) = CreateStatic(parent, "Icon:");
        vbox->AddChild(l);
    }

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        dropDownIcon = w;
        w->onSelectionChanged = std::bind(&EditAnnotationsWindow::DropDownIconSelectionChanged, this, _1);
        l = NewDropDownLayout(w);
        vbox->AddChild(l);
    }

    {
        std::tie(staticColor, l) = CreateStatic(parent, "Color:");
        vbox->AddChild(l);
    }

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        dropDownColor = w;
        w->onSelectionChanged = std::bind(&EditAnnotationsWindow::DropDownColorSelectionChanged, this, _1);
        l = NewDropDownLayout(w);
        vbox->AddChild(l);
        Vec<std::string_view> strings;
        DropDownItemsFromStringArray(strings, gColors);
        w->SetItems(strings);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetText("Delete annotation");
        w->onClicked = std::bind(&EditAnnotationsWindow::ButtonDeleteHandler, this);
        bool ok = w->Create();
        w->SetIsVisible(false);
        CrashIf(!ok);
        buttonDelete = w;
        l = NewButtonLayout(w);
        vbox->AddChild(l);
    }

    {
        // used to take all available space between the what's above and below
        l = new Spacer(0, 0);
        vbox->AddChild(l, 1);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetText("Save PDF...");
        w->onClicked = std::bind(&EditAnnotationsWindow::ButtonSavePDFHandler, this);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsEnabled(false); // only enable if there are changes
        buttonSavePDF = w;
        l = NewButtonLayout(w);
        vbox->AddChild(l);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetText("Close");
        w->onClicked = std::bind(&EditAnnotationsWindow::ButtonCancelHandler, this);
        bool ok = w->Create();
        CrashIf(!ok);
        buttonCancel = w;
        l = NewButtonLayout(w);
        vbox->AddChild(l);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(parent, 4, 8));
    mainLayout = padding;
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
    HMODULE h = GetModuleHandleW(nullptr);
    LPCWSTR iconName = MAKEINTRESOURCEW(GetAppIconID());
    w->hIcon = LoadIconW(h, iconName);

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
