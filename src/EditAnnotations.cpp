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
#include "wingui/TrackbarCtrl.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EnginePdf.h"
#include "EngineMulti.h"
#include "EngineManager.h"

#include "Translations.h"
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
// TODO: more
static const char* gAnnotationTypes = "Text\0Free Text\0Stamp\0Caret\0Ink\0Square\0Circle\0Line\0Polygon\0";

static const char* gTextIcons = "Comment\0Help\0Insert\0Key\0NewParagraph\0Note\0Paragraph\0";

static const char *gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";

static const char *gSoundIcons = "Speaker\0Mic\0";

static const char *gStampIcons = "Approved\0AsIs\0Confidential\0Departmental\0Draft\0Experimental\0Expired\0Final\0ForComment\0ForPublicRelease\0NotApproved\0NotForPublicRelease\0Sold\0TopSecret\0";

static const char* gColors = "None\0Aqua\0Black\0Blue\0Fuchsia\0Gray\0Green\0Lime\0Maroon\0Navy\0Olive\0Orange\0Purple\0Red\0Silver\0Teal\0White\0Yellow\0";

static const char *gFontNames = "Cour\0Helv\0TiRo\0";
static const char *gFontReadableNames = "Courier\0Helvetica\0TimesRoman\0";

// TODO: change to qTextAlignmentNames?
static const char* gQuaddingNames = "Left\0Center\0Right\0";

// COLORREF is abgr format
static COLORREF gColorsValues[] = {
    //0x00000000, /* transparent */
    ColorUnset, /* transparent */
    //0xff00ffff, /* aqua */
    0xffffff00, /* aqua */
    0xff000000, /* black */
    //0xff0000ff, /* blue */
    0xffff0000, /* blue */
    //0xffff00ff, /* fuchsia */
    0xffff00ff, /* fuchsia */
    0xff808080, /* gray */
    0xff008000, /* green */
    0xff00ff00, /* lime */
    //0xff800000, /* maroon */
    0xff000080, /* maroon */
    //0xff000080, /* navy */
    0xff800000, /* navy */
    //0xff808000, /* olive */
    0xff008080, /* olive */
    //0xffffa500, /* orange */
    0xff00a5ff, /* orange */
    0xff800080, /* purple */
    //0xffff0000, /* red */
    0xff0000ff, /* red */
    0xffc0c0c0, /* silver */
    //0xff008080, /* teal */
    0xff808000, /* teal */
    0xffffffff, /* white */
    //0xffffff00, /* yellow */
    0xff00ffff, /* yellow */
};

AnnotationType gAnnotsWithBorder[] = {
    AnnotationType::FreeText,  AnnotationType::Ink,    AnnotationType::Line,
    AnnotationType::Square,    AnnotationType::Circle, AnnotationType::Polygon,
    AnnotationType::PolyLine,
};

AnnotationType gAnnotsWithInteriorColor[] = {
    AnnotationType::Line, AnnotationType::Square, AnnotationType::Circle,
};

AnnotationType gAnnotsWithColor[] = {
    AnnotationType::Stamp,     AnnotationType::Text,   AnnotationType::FileAttachment,
    AnnotationType::Sound,     AnnotationType::Caret,     AnnotationType::FreeText,
    AnnotationType::Ink,       AnnotationType::Line,      AnnotationType::Square,
    AnnotationType::Circle,    AnnotationType::Polygon,   AnnotationType::PolyLine,
    AnnotationType::Highlight, AnnotationType::Underline, AnnotationType::StrikeOut,
    AnnotationType::Squiggly,
};
// clang-format on

// in SumatraPDF.cpp
extern void RerenderForWindowInfo(WindowInfo*);

const char* GetKnownColorName(COLORREF c) {
    // TODO: handle this better?
    COLORREF c2 = ColorSetAlpha(c, 0xff);
    int n = (int)dimof(gColorsValues);
    for (int i = 1; i < n; i++) {
        if (c2 == gColorsValues[i]) {
            const char* s = seqstrings::IdxToStr(gColors, i);
            return s;
        }
    }
    return nullptr;
}

struct EditAnnotationsWindow {
    TabInfo* tab = nullptr;
    Window* mainWindow = nullptr;
    LayoutBase* mainLayout = nullptr;

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
    StaticCtrl* staticTextAlignment = nullptr;
    DropDownCtrl* dropDownTextAlignment = nullptr;
    StaticCtrl* staticTextFont = nullptr;
    DropDownCtrl* dropDownTextFont = nullptr;
    StaticCtrl* staticTextSize = nullptr;
    TrackbarCtrl* trackbarTextSize = nullptr;

    ButtonCtrl* buttonDelete = nullptr;

    StaticCtrl* staticSaveTip = nullptr;
    ButtonCtrl* buttonSavePDF = nullptr;

    ListBoxModel* lbModel = nullptr;

    Vec<Annotation*>* annotations = nullptr;
    // currently selected annotation
    Annotation* annot = nullptr;

    ~EditAnnotationsWindow();
};

static int FindStringInArray(const char* items, const char* toFind, int valIfNotFound = -1) {
    int i = 0;
    while (*items) {
        if (str::Eq(items, toFind)) {
            return i;
        }
        i++;
        items = seqstrings::SkipStr(items);
    }
    return valIfNotFound;
}

void DeleteEditAnnotationsWindow(EditAnnotationsWindow* win) {
    delete win;
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

static void RebuildAnnotations(EditAnnotationsWindow* win) {
    auto model = new ListBoxModelStrings();
    int n = 0;
    if (win->annotations) {
        n = win->annotations->isize();
    }

    str::Str s;
    for (int i = 0; i < n; i++) {
        auto annot = win->annotations->at(i);
        if (annot->isDeleted) {
            continue;
        }
        s.Reset();
        s.AppendFmt("page %d, ", annot->pageNo);
        s.AppendView(AnnotationName(annot->type));
        model->strings.Append(s.AsView());
    }

    win->listBox->SetModel(model);
    delete win->lbModel;
    win->lbModel = model;
}

static void WndCloseHandler(EditAnnotationsWindow* win, WindowCloseEvent* ev) {
    CrashIf(win->mainWindow != ev->w);
    win->tab->editAnnotsWindow = nullptr;
    delete win;
}

static void ButtonSavePDFHandler(EditAnnotationsWindow* win) {
    OPENFILENAME ofn = {0};
    EngineBase* engine = win->tab->AsFixed()->GetEngine();
    WCHAR dstFileName[MAX_PATH + 1] = {0};
    if (IsCtrlPressed()) {
        str::WStr fileFilter(256);
        fileFilter.Append(_TR("PDF documents"));
        fileFilter.Append(L"\1*.pdf\1");
        fileFilter.Append(L"\1*.*\1");
        str::TransChars(fileFilter.Get(), L"\1", L"\0");

        // TODO: automatically construct "foo.pdf" => "foo Copy.pdf"
        const WCHAR* name = engine->FileName();
        str::BufSet(dstFileName, dimof(dstFileName), name);

        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = win->mainWindow->hwnd;
        ofn.lpstrFile = dstFileName;
        ofn.nMaxFile = dimof(dstFileName);
        ofn.lpstrFilter = fileFilter.Get();
        ofn.nFilterIndex = 1;
        // ofn.lpstrTitle = _TR("Rename To");
        // ofn.lpstrInitialDir = initDir;
        ofn.lpstrDefExt = L".pdf";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

        bool ok = GetSaveFileNameW(&ofn);
        if (!ok) {
            return;
        }
        AutoFreeStr dstFilePath = strconv::WstrToUtf8(dstFileName);
        EnginePdfSaveUpdated(engine, dstFilePath.as_view());
        // TODO: show a notification if saved or error message if failed to save
        return;
    }

    EnginePdfSaveUpdated(engine, {});
    // TODO: show a notification if saved or error message if failed to save
}

static void EnableSaveIfAnnotationsChanged(EditAnnotationsWindow* win) {
    bool didChange = false;
    if (win->annotations) {
        for (auto& annot : *win->annotations) {
            if (annot->isChanged || annot->isDeleted) {
                didChange = true;
                break;
            }
        }
    }
    if (didChange) {
        win->staticSaveTip->SetTextColor(MkRgb(0, 0, 0));
    } else {
        win->staticSaveTip->SetTextColor(MkRgb(0xcc, 0xcc, 0xcc));
    }
    win->buttonSavePDF->SetIsEnabled(didChange);
}

// TODO: mupdf shows it in 1.6 but not 1.7. Why?
static bool showRect = false;

static void DoRect(EditAnnotationsWindow* win, Annotation* annot) {
    bool isVisible = (annot != nullptr);
    if (!showRect) {
        isVisible = false;
    }
    win->staticRect->SetIsVisible(isVisible);
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
    win->staticRect->SetText(s.as_view());
}

static void DoAuthor(EditAnnotationsWindow* win, Annotation* annot) {
    bool isVisible = (annot != nullptr) && !annot->Author().empty();
    win->staticAuthor->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.AppendFmt("Author: %s", annot->Author().data());
    win->staticAuthor->SetText(s.as_view());
}

static void AppendPdfDate(str::Str& s, time_t secs) {
    struct tm* tm = gmtime(&secs);
    char buf[100];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", tm);
    s.Append(buf);
}

static void DoModificationDate(EditAnnotationsWindow* win, Annotation* annot) {
    bool isVisible = (annot != nullptr) && (annot->ModificationDate() != 0);
    win->staticModificationDate->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.Append("Date: ");
    AppendPdfDate(s, annot->ModificationDate());
    win->staticModificationDate->SetText(s.as_view());
}

static void DoPopup(EditAnnotationsWindow* win, Annotation* annot) {
    int popupId = annot ? annot->PopupId() : -1;
    bool isVisible = popupId >= 0;
    if (!isVisible) {
        win->staticPopup->SetIsVisible(isVisible);
        return;
    }
    str::Str s;
    s.AppendFmt("Popup: %d 0 R", popupId);
    win->staticPopup->SetIsVisible(isVisible);
    win->staticPopup->SetText(s.as_view());
}

static void DoContents(EditAnnotationsWindow* win, Annotation* annot) {
    bool isVisible = (annot != nullptr);
    win->staticContents->SetIsVisible(isVisible);
    win->editContents->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    str::Str s = annot->Contents();
    // TODO: don't replace if already is "\r\n"
    s.Replace("\n", "\r\n");
    win->editContents->SetText(s.as_view());
}

static void DoTextAlignment(EditAnnotationsWindow* win, Annotation* annot) {
    bool isVisible = false;
    int itemNo = -1;
    if (annot) {
        itemNo = annot->Quadding();
        switch (annot->Type()) {
            case AnnotationType::FreeText:
                isVisible = true;
                break;
        }
    }
    win->staticTextAlignment->SetIsVisible(isVisible);
    win->dropDownTextAlignment->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    const char* items = gQuaddingNames;
    win->dropDownTextAlignment->SetItemsSeqStrings(items);
    win->dropDownTextAlignment->SetCurrentSelection(itemNo);
}

static void TextAlignmentSelectionChanged(EditAnnotationsWindow* win, DropDownSelectionChangedEvent* ev) {
    int newQuadding = ev->idx;
    win->annot->SetQuadding(newQuadding);
    EnableSaveIfAnnotationsChanged(win);
    RerenderForWindowInfo(win->tab->win);
}

static void DoTextFont(EditAnnotationsWindow* win, Annotation* annot) {
    bool isVisible = false;
    if (annot) {
        switch (annot->Type()) {
            case AnnotationType::FreeText:
                isVisible = true;
                break;
        }
    }
    int itemNo = -1;
    if (isVisible) {
        std::string_view fontName = annot->DefaultAppearanceTextFont();
        // TODO: might have other fonts, like "Symb" and "ZaDb"
        itemNo = seqstrings::StrToIdx(gFontNames, fontName.data());
        if (itemNo < 0) {
            isVisible = false;
        }
    }
    win->staticTextFont->SetIsVisible(isVisible);
    win->dropDownTextFont->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    win->dropDownTextFont->SetItemsSeqStrings(gFontReadableNames);
    win->dropDownTextFont->SetCurrentSelection(itemNo);
}

static void DoTextSize(EditAnnotationsWindow* win, Annotation* annot) {
    bool isVisible = false;
    if (annot) {
        switch (annot->Type()) {
            case AnnotationType::FreeText:
                isVisible = true;
                break;
        }
    }
    win->staticTextSize->SetIsVisible(isVisible);
    win->trackbarTextSize->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    int fontSize = annot->DefaultAppearanceTextSize();
    AutoFreeStr s = str::Format("Text Size: %d", fontSize);
    win->staticTextSize->SetText(s.as_view());
    win->annot->SetDefaultAppearanceTextSize(fontSize);
    win->trackbarTextSize->SetPosition(fontSize);
}

static void TextFontSelectionChanged(EditAnnotationsWindow* win, DropDownSelectionChangedEvent* ev) {
    ev->didHandle = true;
    const char* font = seqstrings::IdxToStr(gFontNames, ev->idx);
    win->annot->SetDefaultAppearanceTextFont(font);
    EnableSaveIfAnnotationsChanged(win);
    RerenderForWindowInfo(win->tab->win);
}

static void TextFontSizeChanging(EditAnnotationsWindow* win, TrackbarPosChangingEvent* ev) {
    ev->didHandle = true;
    int fontSize = ev->pos;
    win->annot->SetDefaultAppearanceTextSize(fontSize);
    AutoFreeStr s = str::Format("Text Size: %d", fontSize);
    win->staticTextSize->SetText(s.as_view());
    EnableSaveIfAnnotationsChanged(win);
    RerenderForWindowInfo(win->tab->win);
}

static void DoIcon(EditAnnotationsWindow* win, Annotation* annot) {
    std::string_view itemName;
    bool isVisible = false;
    const char* items = nullptr;
    if (annot) {
        itemName = annot->IconName();
        switch (annot->type) {
            case AnnotationType::Text:
                items = gTextIcons;
                break;
            case AnnotationType::FileAttachment:
                items = gFileAttachmentUcons;
                break;
            case AnnotationType::Sound:
                items = gSoundIcons;
                break;
            case AnnotationType::Stamp:
                items = gStampIcons;
                break;
        }
    }
    if (itemName.empty() || !items) {
        isVisible = false;
    }
    win->staticIcon->SetIsVisible(isVisible);
    win->dropDownIcon->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    win->dropDownIcon->SetItemsSeqStrings(items);
    int idx = FindStringInArray(items, itemName.data(), 0);
    win->dropDownIcon->SetCurrentSelection(idx);
}

static void IconSelectionChanged(EditAnnotationsWindow* win, DropDownSelectionChangedEvent* ev) {
    win->annot->SetIconName(ev->item);
    EnableSaveIfAnnotationsChanged(win);
    // TODO: a better way
    RerenderForWindowInfo(win->tab->win);
}

static bool IsAnnotationTypeInArray(AnnotationType* arr, size_t arrSize, AnnotationType toFind) {
    for (size_t i = 0; i < arrSize; i++) {
        if (toFind == arr[i]) {
            return true;
        }
    }
    return false;
}

// static
int ShouldEditBorder(AnnotationType subtype) {
    size_t n = dimof(gAnnotsWithBorder);
    return IsAnnotationTypeInArray(gAnnotsWithBorder, n, subtype);
}

// static
int ShouldEditInteriorColor(AnnotationType subtype) {
    size_t n = dimof(gAnnotsWithInteriorColor);
    return IsAnnotationTypeInArray(gAnnotsWithInteriorColor, n, subtype);
}

static void DoColor(EditAnnotationsWindow* win, Annotation* annot) {
    auto annotType = annot ? annot->Type() : AnnotationType::Unknown;
    bool isVisible = IsAnnotationTypeInArray(gAnnotsWithColor, dimof(gAnnotsWithColor), annotType);
    win->staticColor->SetIsVisible(isVisible);
    win->dropDownColor->SetIsVisible(isVisible);
    if (!isVisible) {
        return;
    }
    COLORREF col = annot->Color();
    win->dropDownColor->SetItemsSeqStrings(gColors);
    const char* colorName = GetKnownColorName(col);
    int idx = FindStringInArray(gColors, colorName, 0);
    if (idx == -1) {
        // TODO: not a known color name, so add hex version to the list
    }
    win->dropDownColor->SetCurrentSelection(idx);
}

static void ColorSelectionChanged(EditAnnotationsWindow* win, DropDownSelectionChangedEvent* ev) {
    // get known color name
    int nItems = (int)dimof(gColorsValues);
    COLORREF col = ColorUnset;
    if (ev->idx < nItems) {
        col = gColorsValues[ev->idx];
    } else {
        // TODO: parse color from hex
    }
    // TODO: also opacity?
    win->annot->SetColor(col);
    EnableSaveIfAnnotationsChanged(win);
    RerenderForWindowInfo(win->tab->win);
}

// TODO: maybe hide all elements first to simplify Do* functions
static void UpdateUIForSelectedAnnotation(EditAnnotationsWindow* win, int itemNo) {
    int annotPageNo = -1;
    AnnotationType annotType = AnnotationType::Unknown;
    win->annot = nullptr;

    // get annotation at index itemNo, skipping deleted annotations
    int idx = 0;
    int nAnnots = win->annotations->isize();
    for (int i = 0; i < nAnnots; i++) {
        auto annot = win->annotations->at(i);
        if (annot->isDeleted) {
            continue;
        }
        if (idx < itemNo) {
            ++idx;
            continue;
        }
        win->annot = annot;
        annotPageNo = annot->PageNo();
        annotType = annot->Type();
        break;
    }

    DoRect(win, win->annot);
    DoAuthor(win, win->annot);
    DoModificationDate(win, win->annot);
    DoPopup(win, win->annot);
    DoContents(win, win->annot);

    DoTextAlignment(win, win->annot);
    DoTextFont(win, win->annot);
    DoTextSize(win, win->annot);

    // TODO: PDF_ANNOT_FREE_TEXT
    // TODO: PDF_ANNOT_LINE
    DoIcon(win, win->annot);
    // TODO: border
    DoColor(win, win->annot);
    // TODO: icolor
    // TODO: quad points
    // TODO: vertices
    // TODO: ink list
    // TODO: PDF_ANNOT_FILE_ATTACHMENT
    win->buttonDelete->SetIsVisible(win->annot != nullptr);
    // TODO: get from client size
    auto currBounds = win->mainLayout->lastBounds;
    int dx = currBounds.Dx();
    int dy = currBounds.Dy();
    LayoutAndSizeToContent(win->mainLayout, dx, dy, win->mainWindow->hwnd);
    if (annotPageNo > 0) {
        win->tab->AsFixed()->GoToPage(annotPageNo, false);
    }
}

static void ButtonDeleteHandler(EditAnnotationsWindow* win) {
    CrashIf(!win->annot);
    win->annot->Delete();
    RebuildAnnotations(win);
    UpdateUIForSelectedAnnotation(win, -1);
}

static void ListBoxSelectionChanged(EditAnnotationsWindow* win, ListBoxSelectionChangedEvent* ev) {
    // TODO: finish me
    int itemNo = ev->idx;
    UpdateUIForSelectedAnnotation(win, itemNo);
}

static void DropDownAddSelectionChanged(EditAnnotationsWindow* win, DropDownSelectionChangedEvent* ev) {
    UNUSED(ev);
    // TODO: implement me
    MessageBoxNYI(win->mainWindow->hwnd);
}

// TODO: text changes are not immediately reflected in tooltip
// TODO: there seems to be a leak
static void ContentsChanged(EditAnnotationsWindow* win, EditTextChangedEvent* ev) {
    ev->didHandle = true;
    win->annot->SetContents(ev->text);
    EnableSaveIfAnnotationsChanged(win);
    RerenderForWindowInfo(win->tab->win);
}

static void WndSizeHandler(EditAnnotationsWindow* win, SizeEvent* ev) {
    int dx = ev->dx;
    int dy = ev->dy;
    HWND hwnd = ev->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    ev->didHandle = true;
    InvalidateRect(hwnd, nullptr, false);
    if (false && win->mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    LayoutToSize(win->mainLayout, {dx, dy});
}

static void WndKeyHandler(EditAnnotationsWindow* win, KeyEvent* ev) {
    // dbglogf("key: %d\n", ev->keyVirtCode);

    // only interested in Ctrl
    if (ev->keyVirtCode != VK_CONTROL) {
        return;
    }
    if (!win->buttonSavePDF->IsEnabled()) {
        return;
    }
    if (ev->isDown) {
        win->buttonSavePDF->SetText("Save as new PDF");
    } else {
        win->buttonSavePDF->SetText("Save changes to PDF");
    }
}

static StaticCtrl* CreateStaticHidden(HWND parent, std::string_view sv = {}) {
    auto w = new StaticCtrl(parent);
    bool ok = w->Create();
    CrashIf(!ok);
    w->SetText(sv);
    w->SetIsVisible(false);
    return w;
}

static void CreateMainLayout(EditAnnotationsWindow* win) {
    HWND parent = win->mainWindow->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetItemsSeqStrings(gAnnotationTypes);
        w->SetCueBanner("Add annotation...");
        w->onSelectionChanged = std::bind(DropDownAddSelectionChanged, win, _1);
        win->dropDownAdd = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ListBoxCtrl(parent);
        w->idealSizeLines = 5;
        w->SetInsetsPt(4, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        win->lbModel = new ListBoxModelStrings();
        w->SetModel(win->lbModel);
        w->onSelectionChanged = std::bind(ListBoxSelectionChanged, win, _1);
        win->listBox = w;
        vbox->AddChild(w);
    }

    {
        win->staticRect = CreateStaticHidden(parent);
        vbox->AddChild(win->staticRect);
    }

    {
        win->staticAuthor = CreateStaticHidden(parent);
        // WindowBaseLayout* l2 = (WindowBaseLayout*)l;
        // l2->SetInsetsPt(20, 0, 0, 0);
        vbox->AddChild(win->staticAuthor);
    }

    {
        win->staticModificationDate = CreateStaticHidden(parent);
        vbox->AddChild(win->staticModificationDate);
    }

    {
        win->staticPopup = CreateStaticHidden(parent);
        vbox->AddChild(win->staticPopup);
    }

    {
        win->staticContents = CreateStaticHidden(parent, "Contents:");
        win->staticContents->SetInsetsPt(4, 0, 0, 0);
        vbox->AddChild(win->staticContents);
    }

    {
        auto w = new EditCtrl(parent);
        w->isMultiLine = true;
        w->idealSizeLines = 5;
        bool ok = w->Create();
        CrashIf(!ok);
        w->maxDx = 150;
        w->SetIsVisible(false);
        w->onTextChanged = std::bind(ContentsChanged, win, _1);
        win->editContents = w;
        vbox->AddChild(w);
    }

    {
        win->staticIcon = CreateStaticHidden(parent, "Icon:");
        win->staticIcon->SetInsetsPt(8, 0, 0, 0);
        vbox->AddChild(win->staticIcon);
    }

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        w->onSelectionChanged = std::bind(IconSelectionChanged, win, _1);
        win->dropDownIcon = w;
        vbox->AddChild(w);
    }

    {
        win->staticColor = CreateStaticHidden(parent, "Color:");
        win->staticColor->SetInsetsPt(8, 0, 0, 0);
        vbox->AddChild(win->staticColor);
    }

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = std::bind(ColorSelectionChanged, win, _1);
        win->dropDownColor = w;
        vbox->AddChild(w);
    }

    {
        win->staticTextAlignment = CreateStaticHidden(parent, "Text Alignment:");
        vbox->AddChild(win->staticTextAlignment);
    }

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = std::bind(TextAlignmentSelectionChanged, win, _1);
        win->dropDownTextAlignment = w;
        vbox->AddChild(w);
    }

    {
        win->staticTextFont = CreateStaticHidden(parent, "Text Font:");
        vbox->AddChild(win->staticTextFont);
    }

    {
        auto w = new DropDownCtrl(parent);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = std::bind(TextFontSelectionChanged, win, _1);
        win->dropDownTextFont = w;
        vbox->AddChild(w);
    }

    {
        win->staticTextSize = CreateStaticHidden(parent, "Text Size:");
        vbox->AddChild(win->staticTextSize);
    }

    {
        auto w = new TrackbarCtrl(parent);
        w->rangeMin = 8;
        w->rangeMax = 36;
        w->idealSize.dx = 360;
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        w->onPosChanging = std::bind(TextFontSizeChanging, win, _1);
        win->trackbarTextSize = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetInsetsPt(8, 0, 0, 0);
        w->SetText("Delete annotation");
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsVisible(false);
        w->onClicked = std::bind(&ButtonDeleteHandler, win);
        win->buttonDelete = w;
        vbox->AddChild(w);
    }

    {
        // used to take all available space between the what's above and below
        auto l = new Spacer(0, 0);
        vbox->AddChild(l, 1);
    }

    {
        win->staticSaveTip = CreateStaticHidden(parent, "Tip: use Ctrl to save as a new PDF");
        win->staticSaveTip->SetIsVisible(true);
        win->staticSaveTip->SetTextColor(MkRgb(0xcc, 0xcc, 0xcc));
        // make invisible until buttonSavePDF is enabled
        vbox->AddChild(win->staticSaveTip);
    }

    {
        auto w = new ButtonCtrl(parent);
        // TODO: maybe show file name e.g. "Save changes to foo.pdf"
        w->SetText("Save changes to PDF");
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsEnabled(false); // only enabled if there are changes
        w->onClicked = std::bind(&ButtonSavePDFHandler, win);
        win->buttonSavePDF = w;
        vbox->AddChild(w);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(parent, 4, 8));
    win->mainLayout = padding;
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

    auto mainWindow = new Window();
    // w->isDialog = true;
    HMODULE h = GetModuleHandleW(nullptr);
    LPCWSTR iconName = MAKEINTRESOURCEW(GetAppIconID());
    mainWindow->hIcon = LoadIconW(h, iconName);

    mainWindow->isDialog = true;
    mainWindow->backgroundColor = MkRgb((u8)0xee, (u8)0xee, (u8)0xee);
    // PositionCloseTo(w, args->hwndRelatedTo);
    // SIZE winSize = {w->initialSize.dx, w->initialSize.Height};
    // LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.cx, winSize.cy};
    bool ok = mainWindow->Create();
    CrashIf(!ok);
    mainWindow->onClose = std::bind(WndCloseHandler, win, _1);
    mainWindow->onSize = std::bind(WndSizeHandler, win, _1);
    mainWindow->onKeyDownUp = std::bind(WndKeyHandler, win, _1);

    win->mainWindow = mainWindow;
    CreateMainLayout(win);
    RebuildAnnotations(win);

    // size our editor window to be the same height as main window
    int minDy = 720;
    // TODO: this is slightly less that wanted
    HWND hwnd = tab->win->hwndCanvas;
    auto rc = ClientRect(hwnd);
    if (rc.Dy() > 0) {
        minDy = rc.Dy();
        // if it's a tall window, up the number of items in list box
        // from 5 to 14
        if (minDy > 1024) {
            win->listBox->idealSizeLines = 14;
        }
    }
    LayoutAndSizeToContent(win->mainLayout, 520, minDy, mainWindow->hwnd);
    HwndPositionToTheRightOf(mainWindow->hwnd, tab->win->hwndFrame);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    mainWindow->SetIsVisible(true);

    CrashIf(!ok);
}
