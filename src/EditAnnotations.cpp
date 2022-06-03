/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "EngineMupdfImpl.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "GlobalPrefs.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "Toolbar.h"
#include "TabInfo.h"
#include "EditAnnotations.h"
#include "SumatraPDF.h"

#include "utils/Log.h"

using std::placeholders::_1;

constexpr int borderWidthMin = 0;
constexpr int borderWidthMax = 12;

// clang-format off
static const char *gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";
static const char *gSoundIcons = "Speaker\0Mic\0";
static const char *gStampIcons = "Approved\0AsIs\0Confidential\0Departmental\0Draft\0Experimental\0Expired\0Final\0ForComment\0ForPublicRelease\0NotApproved\0NotForPublicRelease\0Sold\0TopSecret\0";
static const char *gLineEndingStyles = "None\0Square\0Circle\0Diamond\0OpenArrow\0ClosedArrow\0Butt\0ROpenArrow\0RClosedArrow\0Slash\0";
static const char* gColors = "Transparent\0Aqua\0Black\0Blue\0Fuchsia\0Gray\0Green\0Lime\0Maroon\0Navy\0Olive\0Orange\0Purple\0Red\0Silver\0Teal\0White\0Yellow\0";
static const char *gFontNames = "Cour\0Helv\0TiRo\0";
static const char *gFontReadableNames = "Courier\0Helvetica\0TimesRoman\0";
static const char* gQuaddingNames = "Left\0Center\0Right\0";

static PdfColor gColorsValues[] = {
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
};

static AnnotationType gAnnotsWithBorder[] = {
    AnnotationType::FreeText,  AnnotationType::Ink,    AnnotationType::Line,
    AnnotationType::Square,    AnnotationType::Circle, AnnotationType::Polygon,
    AnnotationType::PolyLine,
};

static AnnotationType gAnnotsWithInteriorColor[] = {
    AnnotationType::Line, AnnotationType::Square, AnnotationType::Circle,
};

static AnnotationType gAnnotsWithColor[] = {
    AnnotationType::Stamp,     AnnotationType::Text,   AnnotationType::FileAttachment,
    AnnotationType::Sound,     AnnotationType::Caret,     AnnotationType::FreeText,
    AnnotationType::Ink,       AnnotationType::Line,      AnnotationType::Square,
    AnnotationType::Circle,    AnnotationType::Polygon,   AnnotationType::PolyLine,
    AnnotationType::Highlight, AnnotationType::Underline, AnnotationType::StrikeOut,
    AnnotationType::Squiggly,
};

// list of annotaions where GetColor() returns background color
// TODO: probably incomplete;
static AnnotationType gAnnotsIsColorBackground[] = {
    AnnotationType::FreeText,
};
// clang-format on

const char* GetKnownColorName(PdfColor c) {
    int n = (int)dimof(gColorsValues);
    for (int i = 0; i < n; i++) {
        if (c == gColorsValues[i]) {
            const char* s = seqstrings::IdxToStr(gColors, i);
            return s;
        }
    }
    return nullptr;
}

struct EditAnnotationsWindow : Wnd {
    void OnSize(UINT msg, UINT type, SIZE size) override;
    void OnClose() override;

    TabInfo* tab = nullptr;
    LayoutBase* mainLayout = nullptr;

    ListBox* listBox = nullptr;
    Static* staticRect = nullptr;
    Static* staticAuthor = nullptr;
    Static* staticModificationDate = nullptr;
    Static* staticPopup = nullptr;
    Static* staticContents = nullptr;
    Edit* editContents = nullptr;
    Static* staticTextAlignment = nullptr;
    DropDown* dropDownTextAlignment = nullptr;
    Static* staticTextFont = nullptr;
    DropDown* dropDownTextFont = nullptr;
    Static* staticTextSize = nullptr;
    Trackbar* trackbarTextSize = nullptr;
    Static* staticTextColor = nullptr;
    DropDown* dropDownTextColor = nullptr;

    Static* staticLineStart = nullptr;
    DropDown* dropDownLineStart = nullptr;
    Static* staticLineEnd = nullptr;
    DropDown* dropDownLineEnd = nullptr;

    Static* staticIcon = nullptr;
    DropDown* dropDownIcon = nullptr;

    Static* staticBorder = nullptr;
    Trackbar* trackbarBorder = nullptr;

    Static* staticColor = nullptr;
    DropDown* dropDownColor = nullptr;
    Static* staticInteriorColor = nullptr;
    DropDown* dropDownInteriorColor = nullptr;

    Static* staticOpacity = nullptr;
    Trackbar* trackbarOpacity = nullptr;

    Button* buttonSaveAttachment = nullptr;
    Button* buttonEmbedAttachment = nullptr;

    Button* buttonDelete = nullptr;

    Button* buttonSaveToCurrentFile = nullptr;
    Button* buttonSaveToNewFile = nullptr;

    Vec<Annotation*>* annotations = nullptr;
    // currently selected annotation
    Annotation* annot = nullptr;

    bool skipGoToPage = false;

    str::Str currTextColor;
    str::Str currCustomColor;
    str::Str currCustomInteriorColor;

    ~EditAnnotationsWindow();

    void ListBoxSelectionChanged();
};

static EngineMupdf* GetEngineMupdf(EditAnnotationsWindow* ew) {
    // TODO: shouldn't happen but seen in crash report
    if (!ew || !ew->tab) {
        return nullptr;
    }
    DisplayModel* dm = ew->tab->AsFixed();
    if (!dm) {
        return nullptr;
    }
    return AsEngineMupdf(dm->GetEngine());
}

static void HidePerAnnotControls(EditAnnotationsWindow* ew) {
    ew->staticRect->SetIsVisible(false);
    ew->staticAuthor->SetIsVisible(false);
    ew->staticModificationDate->SetIsVisible(false);
    ew->staticPopup->SetIsVisible(false);
    ew->staticContents->SetIsVisible(false);
    ew->editContents->SetIsVisible(false);
    ew->staticTextAlignment->SetIsVisible(false);
    ew->dropDownTextAlignment->SetIsVisible(false);
    ew->staticTextFont->SetIsVisible(false);
    ew->dropDownTextFont->SetIsVisible(false);
    ew->staticTextSize->SetIsVisible(false);
    ew->trackbarTextSize->SetIsVisible(false);
    ew->staticTextColor->SetIsVisible(false);
    ew->dropDownTextColor->SetIsVisible(false);

    ew->staticLineStart->SetIsVisible(false);
    ew->dropDownLineStart->SetIsVisible(false);
    ew->staticLineEnd->SetIsVisible(false);
    ew->dropDownLineEnd->SetIsVisible(false);

    ew->staticIcon->SetIsVisible(false);
    ew->dropDownIcon->SetIsVisible(false);

    ew->staticBorder->SetIsVisible(false);
    ew->trackbarBorder->SetIsVisible(false);
    ew->staticColor->SetIsVisible(false);
    ew->dropDownColor->SetIsVisible(false);
    ew->staticInteriorColor->SetIsVisible(false);
    ew->dropDownInteriorColor->SetIsVisible(false);

    ew->staticOpacity->SetIsVisible(false);
    ew->trackbarOpacity->SetIsVisible(false);

    ew->buttonSaveAttachment->SetIsVisible(false);
    ew->buttonEmbedAttachment->SetIsVisible(false);

    ew->buttonDelete->SetIsVisible(false);
}

static int FindStringInArray(const char* items, const char* toFind, int valIfNotFound = -1) {
    int idx = seqstrings::StrToIdx(items, toFind);
    if (idx < 0) {
        idx = valIfNotFound;
    }
    return idx;
}

static bool IsAnnotationTypeInArray(AnnotationType* arr, size_t arrSize, AnnotationType toFind) {
    for (size_t i = 0; i < arrSize; i++) {
        if (toFind == arr[i]) {
            return true;
        }
    }
    return false;
}

void CloseAndDeleteEditAnnotationsWindow(EditAnnotationsWindow* ew) {
    // this will trigger closing the window
    delete ew;
}

static void DeleteAnnotations(EditAnnotationsWindow* ew) {
    if (ew->annotations) {
        DeleteVecMembers(*ew->annotations);
        delete ew->annotations;
    }
    ew->annotations = nullptr;
    ew->annot = nullptr;
}

EditAnnotationsWindow::~EditAnnotationsWindow() {
    DeleteAnnotations(this);
    delete mainLayout;
}

static bool DidAnnotationsChange(EditAnnotationsWindow* ew) {
    EngineMupdf* engine = GetEngineMupdf(ew);
    if (!engine) { // maybe seen in crash report
        ReportIf(true);
        return false;
    }
    return EngineMupdfHasUnsavedAnnotations(engine);
}

static void EnableSaveIfAnnotationsChanged(EditAnnotationsWindow* ew) {
    bool didChange = DidAnnotationsChange(ew);
    ew->buttonSaveToCurrentFile->SetIsEnabled(didChange);
    ew->buttonSaveToNewFile->SetIsEnabled(didChange);
}

static void RemoveDeletedAnnotations(Vec<Annotation*>* v) {
    if (!v) {
        return;
    }
again:
    auto n = v->isize();
    for (int i = 0; i < n; i++) {
        auto a = v->at(i);
        if (a->isDeleted) {
            v->RemoveAt((size_t)i, 1);
            delete a;
            goto again;
        }
    }
}

// Annotation* is a temporary wrapper. Find matching in list of annotations
static Annotation* FindMatchingAnnotation(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!ew || !ew->annotations) {
        return annot;
    }
    for (auto a : *ew->annotations) {
        if (IsAnnotationEq(a, annot)) {
            return a;
        }
    }
    return nullptr;
}

static void RebuildAnnotations(EditAnnotationsWindow* ew) {
    RemoveDeletedAnnotations(ew->annotations);
    auto model = new ListBoxModelStrings();
    int n = 0;
    if (ew->annotations) {
        n = ew->annotations->isize();
    }

    str::Str s;
    for (int i = 0; i < n; i++) {
        auto annot = ew->annotations->at(i);
        CrashIf(annot->isDeleted);
        s.Reset();
        s.AppendFmt("page %d, ", annot->pageNo);
        const char* name = AnnotationReadableName(annot->type);
        s.Append(name);
        model->strings.Append(s.Get());
    }

    ew->listBox->SetModel(model);
    EnableSaveIfAnnotationsChanged(ew);
}

void EditAnnotationsWindow::OnClose() {
    tab->editAnnotsWindow = nullptr;
    delete this; // sketchy
}

extern bool SaveAnnotationsToMaybeNewPdfFile(TabInfo* tab);
static void GetAnnotationsFromEngine(EditAnnotationsWindow* ew, TabInfo* tab);
static void UpdateUIForSelectedAnnotation(EditAnnotationsWindow* ew, int itemNo);

static void ButtonSaveToNewFileHandler(EditAnnotationsWindow* ew) {
    TabInfo* tab = ew->tab;
    bool ok = SaveAnnotationsToMaybeNewPdfFile(tab);
    if (!ok) {
        return;
    }
}

static void ButtonSaveToCurrentPDFHandler(EditAnnotationsWindow* ew) {
    TabInfo* tab = ew->tab;
    EngineMupdf* engine = GetEngineMupdf(ew);
    const char* path = engine->FileName();
    bool ok = EngineMupdfSaveUpdated(engine, {}, [&tab, &path](const char* mupdfErr) {
        str::Str msg;
        // TODO: duplicated message
        msg.AppendFmt(_TRA("Saving of '%s' failed with: '%s'"), path, mupdfErr);
        NotificationCreateArgs args;
        args.hwndParent = tab->win->hwndCanvas;
        args.msg = msg.Get();
        args.warning = true;
        args.timeoutMs = 0;
        ShowNotification(args);
    });
    if (!ok) {
        return;
    }
    str::Str msg;
    msg.AppendFmt(_TRA("Saved annotations to '%s'"), path);
    NotificationCreateArgs args;
    args.hwndParent = tab->win->hwndCanvas;
    args.msg = msg.Get();
    ShowNotification(args);

    // TODO: hacky: set tab->editAnnotsWindow to nullptr to
    // disable a check in ReloadDocuments. Could pass additional argument
    auto tmpWin = tab->editAnnotsWindow;
    tab->editAnnotsWindow = nullptr;
    ReloadDocument(tab->win, false);
    tab->editAnnotsWindow = tmpWin;

    DeleteAnnotations(ew);
    GetAnnotationsFromEngine(ew, tab);
    UpdateUIForSelectedAnnotation(ew, -1);
}

static void ItemsFromSeqstrings(StrVec& items, const char* strings) {
    while (strings) {
        items.Append(strings);
        seqstrings::Next(strings);
    }
}

static void DropDownFillColors(DropDown* w, PdfColor col, str::Str& customColor) {
    StrVec items;
    ItemsFromSeqstrings(items, gColors);
    const char* colorName = GetKnownColorName(col);
    int idx = seqstrings::StrToIdx(gColors, colorName);
    if (idx < 0) {
        customColor.Reset();
        SerializePdfColor(col, customColor);
        items.Append(customColor.LendData());
        idx = items.Size() - 1;
    }
    w->SetItems(items);
    w->SetCurrentSelection(idx);
}

static PdfColor GetDropDownColor(const char* sv) {
    int idx = seqstrings::StrToIdx(gColors, sv);
    if (idx >= 0) {
        int nMaxColors = (int)dimof(gColorsValues);
        CrashIf(idx >= nMaxColors);
        if (idx < nMaxColors) {
            return gColorsValues[idx];
        }
        return 0;
    }
    ParsedColor col;
    ParseColor(col, sv);
    return col.pdfCol;
}

// TODO: mupdf shows it in 1.6 but not 1.7. Why?
bool gShowRect = true;

static void DoRect(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!gShowRect) {
        return;
    }
    str::Str s;
    RectF rect = GetRect(annot);
    int x = (int)rect.x;
    int y = (int)rect.y;
    int dx = (int)rect.dx;
    int dy = (int)rect.dy;
    s.AppendFmt(_TRA("Rect: x=%d y=%d dx=%d dy=%d"), x, y, dx, dy);
    ew->staticRect->SetText(s.Get());
    ew->staticRect->SetIsVisible(true);
}

static void DoAuthor(EditAnnotationsWindow* ew, Annotation* annot) {
    const char* author = Author(annot);
    bool isVisible = !str::IsEmpty(author);
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.AppendFmt(_TRA("Author: %s"), author);
    ew->staticAuthor->SetText(s.Get());
    ew->staticAuthor->SetIsVisible(true);
}

static void AppendPdfDate(str::Str& s, time_t secs) {
    struct tm tm;
    gmtime_s(&tm, &secs);
    char buf[100];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", &tm);
    s.Append(buf);
}

static void DoModificationDate(EditAnnotationsWindow* ew, Annotation* annot) {
    bool isVisible = (ModificationDate(annot) != 0);
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.Append(_TRA("Date:"));
    s.Append(" "); // apptranslator doesn't handle spaces at the end of translated string
    AppendPdfDate(s, ModificationDate(annot));
    ew->staticModificationDate->SetText(s.Get());
    ew->staticModificationDate->SetIsVisible(true);
}

static void DoPopup(EditAnnotationsWindow* ew, Annotation* annot) {
    int popupId = PopupId(annot);
    if (popupId < 0) {
        return;
    }
    str::Str s;
    s.AppendFmt(_TRA("Popup: %d 0 R"), popupId);
    ew->staticPopup->SetText(s.Get());
    ew->staticPopup->SetIsVisible(true);
}

static void DoContents(EditAnnotationsWindow* ew, Annotation* annot) {
    str::Str s = Contents(annot);
    // TODO: don't replace if already is "\r\n"
    Replace(s, "\n", "\r\n");
    ew->editContents->SetText(s.Get());
    ew->staticContents->SetIsVisible(true);
    ew->editContents->SetIsVisible(true);
}

static void DoTextAlignment(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    int itemNo = Quadding(annot);
    const char* items = gQuaddingNames;
    ew->dropDownTextAlignment->SetItemsSeqStrings(items);
    ew->dropDownTextAlignment->SetCurrentSelection(itemNo);
    ew->staticTextAlignment->SetIsVisible(true);
    ew->dropDownTextAlignment->SetIsVisible(true);
}

static void TextAlignmentSelectionChanged(EditAnnotationsWindow* ew) {
    auto idx = ew->dropDownTextAlignment->GetCurrentSelection();
    int newQuadding = idx;
    SetQuadding(ew->annot, newQuadding);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextFont(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    const char* fontName = DefaultAppearanceTextFont(annot);
    // TODO: might have other fonts, like "Symb" and "ZaDb"
    auto itemNo = seqstrings::StrToIdx(gFontNames, fontName);
    if (itemNo < 0) {
        return;
    }
    ew->dropDownTextFont->SetItemsSeqStrings(gFontReadableNames);
    ew->dropDownTextFont->SetCurrentSelection(itemNo);
    ew->staticTextFont->SetIsVisible(true);
    ew->dropDownTextFont->SetIsVisible(true);
}

static void TextFontSelectionChanged(EditAnnotationsWindow* ew) {
    auto idx = ew->dropDownTextFont->GetCurrentSelection();
    const char* font = seqstrings::IdxToStr(gFontNames, idx);
    SetDefaultAppearanceTextFont(ew->annot, font);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextSize(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    int fontSize = DefaultAppearanceTextSize(annot);
    AutoFreeStr s = str::Format(_TRA("Text Size: %d"), fontSize);
    ew->staticTextSize->SetText(s.Get());
    SetDefaultAppearanceTextSize(ew->annot, fontSize);
    ew->trackbarTextSize->SetValue(fontSize);
    ew->staticTextSize->SetIsVisible(true);
    ew->trackbarTextSize->SetIsVisible(true);
}

static void TextFontSizeChanging(EditAnnotationsWindow* ew, TrackbarPosChangingEvent* ev) {
    int fontSize = ev->pos;
    SetDefaultAppearanceTextSize(ew->annot, fontSize);
    AutoFreeStr s = str::Format(_TRA("Text Size: %d"), fontSize);
    ew->staticTextSize->SetText(s.Get());
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextColor(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    PdfColor col = DefaultAppearanceTextColor(annot);
    DropDownFillColors(ew->dropDownTextColor, col, ew->currTextColor);
    ew->staticTextColor->SetIsVisible(true);
    ew->dropDownTextColor->SetIsVisible(true);
}

static void TextColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto idx = ew->dropDownTextColor->GetCurrentSelection();
    char* item = ew->dropDownTextColor->items.at(idx);
    auto col = GetDropDownColor(item);
    SetDefaultAppearanceTextColor(ew->annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoBorder(EditAnnotationsWindow* ew, Annotation* annot) {
    size_t n = dimof(gAnnotsWithBorder);
    bool isVisible = IsAnnotationTypeInArray(gAnnotsWithBorder, n, Type(annot));
    if (!isVisible) {
        return;
    }
    int borderWidth = BorderWidth(annot);
    borderWidth = std::clamp(borderWidth, borderWidthMin, borderWidthMax);
    AutoFreeStr s = str::Format(_TRA("Border: %d"), borderWidth);
    ew->staticBorder->SetText(s.Get());
    ew->trackbarBorder->SetValue(borderWidth);
    ew->staticBorder->SetIsVisible(true);
    ew->trackbarBorder->SetIsVisible(true);
}

static void BorderWidthChanging(EditAnnotationsWindow* ew, TrackbarPosChangingEvent* ev) {
    int borderWidth = ev->pos;
    SetBorderWidth(ew->annot, borderWidth);
    AutoFreeStr s = str::Format(_TRA("Border: %d"), borderWidth);
    ew->staticBorder->SetText(s.Get());
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoLineStartEnd(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::Line) {
        return;
    }
    int start = 0;
    int end = 0;
    GetLineEndingStyles(annot, &start, &end);
    ew->dropDownLineStart->SetItemsSeqStrings(gLineEndingStyles);
    ew->dropDownLineStart->SetCurrentSelection(start);
    ew->dropDownLineEnd->SetItemsSeqStrings(gLineEndingStyles);
    ew->dropDownLineEnd->SetCurrentSelection(end);
    ew->staticLineStart->SetIsVisible(true);
    ew->dropDownLineStart->SetIsVisible(true);
    ew->staticLineEnd->SetIsVisible(true);
    ew->dropDownLineEnd->SetIsVisible(true);
}

static void LineStartSelectionChanged(EditAnnotationsWindow* ew) {
    int start = 0;
    int end = 0;
    GetLineEndingStyles(ew->annot, &start, &end);
    auto idx = ew->dropDownLineStart->GetCurrentSelection();
    int newVal = idx;
    start = newVal;
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void LineEndSelectionChanged(EditAnnotationsWindow* ew) {
    int start = 0;
    int end = 0;
    GetLineEndingStyles(ew->annot, &start, &end);
    auto idx = ew->dropDownLineEnd->GetCurrentSelection();
    int newVal = idx;
    end = newVal;
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoIcon(EditAnnotationsWindow* ew, Annotation* annot) {
    const char* itemName = IconName(annot);
    const char* items = nullptr;
    switch (Type(annot)) {
        case AnnotationType::Text:
            items = gAnnotationTextIcons;
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
    if (!items || str::IsEmpty(itemName)) {
        return;
    }
    ew->dropDownIcon->SetItemsSeqStrings(items);
    int idx = FindStringInArray(items, itemName, 0);
    ew->dropDownIcon->SetCurrentSelection(idx);
    ew->staticIcon->SetIsVisible(true);
    ew->dropDownIcon->SetIsVisible(true);
}

static void IconSelectionChanged(EditAnnotationsWindow* ew) {
    auto idx = ew->dropDownIcon->GetCurrentSelection();
    auto item = ew->dropDownIcon->items.at(idx);
    SetIconName(ew->annot, item);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoColor(EditAnnotationsWindow* ew, Annotation* annot) {
    size_t n = dimof(gAnnotsWithColor);
    bool isVisible = IsAnnotationTypeInArray(gAnnotsWithColor, n, Type(annot));
    if (!isVisible) {
        return;
    }
    PdfColor col = GetColor(annot);
    DropDownFillColors(ew->dropDownColor, col, ew->currCustomColor);
    n = dimof(gAnnotsIsColorBackground);
    bool isBgCol = IsAnnotationTypeInArray(gAnnotsIsColorBackground, n, Type(annot));
    if (isBgCol) {
        ew->staticColor->SetText(_TR("Background Color:"));
    } else {
        ew->staticColor->SetText(_TR("Color:"));
    }
    ew->staticColor->SetIsVisible(true);
    ew->dropDownColor->SetIsVisible(true);
}

static void ColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto idx = ew->dropDownColor->GetCurrentSelection();
    auto item = ew->dropDownColor->items.at(idx);
    auto col = GetDropDownColor(item);
    SetColor(ew->annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoInteriorColor(EditAnnotationsWindow* ew, Annotation* annot) {
    size_t n = dimof(gAnnotsWithInteriorColor);
    bool isVisible = IsAnnotationTypeInArray(gAnnotsWithInteriorColor, n, Type(annot));
    if (!isVisible) {
        return;
    }
    PdfColor col = InteriorColor(annot);
    DropDownFillColors(ew->dropDownInteriorColor, col, ew->currCustomInteriorColor);
    ew->staticInteriorColor->SetIsVisible(true);
    ew->dropDownInteriorColor->SetIsVisible(true);
}

static void InteriorColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto idx = ew->dropDownInteriorColor->GetCurrentSelection();
    auto item = ew->dropDownInteriorColor->items.at(idx);
    auto col = GetDropDownColor(item);
    SetInteriorColor(ew->annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoOpacity(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::Highlight) {
        return;
    }
    int opacity = Opacity(ew->annot);
    AutoFreeStr s = str::Format(_TRA("Opacity: %d"), opacity);
    ew->staticOpacity->SetText(s.Get());
    ew->staticOpacity->SetIsVisible(true);
    ew->trackbarOpacity->SetIsVisible(true);
    ew->trackbarOpacity->SetValue(opacity);
}

static void DoSaveEmbed(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FileAttachment) {
        return;
    }
    ew->buttonSaveAttachment->SetIsVisible(true);
    ew->buttonEmbedAttachment->SetIsVisible(true);
}

static void OpacityChanging(EditAnnotationsWindow* ew, TrackbarPosChangingEvent* ev) {
    int opacity = ev->pos;
    SetOpacity(ew->annot, opacity);
    AutoFreeStr s = str::Format(_TRA("Opacity: %d"), opacity);
    ew->staticOpacity->SetText(s.Get());
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void UpdateUIForSelectedAnnotation(EditAnnotationsWindow* ew, int itemNo) {
    int annotPageNo = -1;
    ew->annot = nullptr;

    // get annotation at index itemNo, skipping deleted annotations
    int idx = 0;
    int nAnnots = ew->annotations->isize();
    for (int i = 0; itemNo >= 0 && i < nAnnots; i++) {
        auto annot = ew->annotations->at(i);
        if (annot->isDeleted) {
            continue;
        }
        if (idx < itemNo) {
            ++idx;
            continue;
        }
        ew->annot = annot;
        annotPageNo = PageNo(annot);
        break;
    }

    HidePerAnnotControls(ew);
    if (ew->annot) {
        DoRect(ew, ew->annot);
        DoAuthor(ew, ew->annot);
        DoModificationDate(ew, ew->annot);
        DoPopup(ew, ew->annot);
        DoContents(ew, ew->annot);

        DoTextAlignment(ew, ew->annot);
        DoTextFont(ew, ew->annot);
        DoTextSize(ew, ew->annot);
        DoTextColor(ew, ew->annot);

        DoLineStartEnd(ew, ew->annot);

        DoIcon(ew, ew->annot);

        DoBorder(ew, ew->annot);
        DoColor(ew, ew->annot);
        DoInteriorColor(ew, ew->annot);

        DoOpacity(ew, ew->annot);
        DoSaveEmbed(ew, ew->annot);

        ew->buttonDelete->SetIsVisible(true);
    }

    // TODO: get from client size
    auto currBounds = ew->mainLayout->lastBounds;
    int dx = currBounds.dx;
    int dy = currBounds.dy;
    LayoutAndSizeToContent(ew->mainLayout, dx, dy, ew->hwnd);
    if (annotPageNo < 1) {
        return;
    }
    if (ew->skipGoToPage) {
        ew->skipGoToPage = false;
        return;
    }
    DisplayModel* dm = ew->tab->AsFixed();
    int nPages = dm->PageCount();
    if (annotPageNo > nPages) {
        // see https://github.com/sumatrapdfreader/sumatrapdf/issues/1701
        logf("UpdateUIForSelectedAnnotation: invalid annotPageNo (%d), should be <= than nPages (%d)\n", annotPageNo,
             nPages);
    }
    // TODO: should skip if annot is already visible but need
    // DisplayModel::IsPageAreaVisible() function
    // TODO: use GoToPage() with x/y position
    dm->GoToPage(annotPageNo, false);
}

static void ButtonSaveAttachment(EditAnnotationsWindow* ew) {
    CrashIf(!ew->annot);
    // TODO: implement me
    MessageBoxNYI(ew->hwnd);
}

static void ButtonEmbedAttachment(EditAnnotationsWindow* ew) {
    CrashIf(!ew->annot);
    // TODO: implement me
    MessageBoxNYI(ew->hwnd);
}

void DeleteAnnotationAndUpdateUI(TabInfo* tab, EditAnnotationsWindow* ew, Annotation* annot) {
    annot = FindMatchingAnnotation(ew, annot);
    DeleteAnnotation(annot);
    if (ew != nullptr) {
        // can be null if called from Menu.cpp and annotations window is not visible
        RebuildAnnotations(ew);
        UpdateUIForSelectedAnnotation(ew, 0);
        ew->listBox->SetCurrentSelection(0);
    }
    MainWindowRerender(tab->win);
    ToolbarUpdateStateForWindow(tab->win, false);
}

static void ButtonDeleteHandler(EditAnnotationsWindow* ew) {
    CrashIf(!ew->annot);
    DeleteAnnotationAndUpdateUI(ew->tab, ew, ew->annot);
}

void EditAnnotationsWindow::ListBoxSelectionChanged() {
    int itemNo = listBox->GetCurrentSelection();
    UpdateUIForSelectedAnnotation(this, itemNo);
}

static UINT_PTR gMainWindowRerenderTimer = 0;
static MainWindow* gMainWindowForRender = nullptr;

// TODO: there seems to be a leak
static void ContentsChanged(EditAnnotationsWindow* ew) {
    auto txt = ew->editContents->GetTextTemp();
    SetContents(ew->annot, txt);
    EnableSaveIfAnnotationsChanged(ew);

    MainWindow* win = ew->tab->win;
    if (gMainWindowRerenderTimer != 0) {
        // logf("ContentsChanged: killing existing timer for re-render of MainWindow\n");
        KillTimer(win->hwndCanvas, gMainWindowRerenderTimer);
        gMainWindowRerenderTimer = 0;
    }
    UINT timeoutInMs = 1000;
    gMainWindowForRender = win;
    gMainWindowRerenderTimer = SetTimer(win->hwndCanvas, 1, timeoutInMs, [](HWND, UINT, UINT_PTR, DWORD) {
        if (MainWindowStillValid(gMainWindowForRender)) {
            // logf("ContentsChanged: re-rendering MainWindow\n");
            MainWindowRerender(gMainWindowForRender);
        } else {
            // logf("ContentsChanged: NOT re-rendering MainWindow because is not valid anymore\n");
        }
        gMainWindowRerenderTimer = 0;
    });
}

void EditAnnotationsWindow::OnSize(UINT msg, UINT type, SIZE size) {
    if (msg != WM_SIZE) {
        return;
    }
    if (!mainLayout) {
        return;
    }
    int dx = (int)size.cx;
    int dy = (int)size.cy;
    if (dx == 0 || dy == 0) {
        return;
    }
    InvalidateRect(hwnd, nullptr, false);
    if (false && mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    LayoutToSize(mainLayout, {dx, dy});
}

static Static* CreateStatic(HWND parent, const char* s = nullptr) {
    auto w = new Static();
    StaticCreateArgs args;
    args.parent = parent;
    args.text = s;
    HWND hwnd = w->Create(args);
    CrashIf(!hwnd);
    return w;
}

static void CreateMainLayout(EditAnnotationsWindow* ew) {
    HWND parent = ew->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        ListBoxCreateArgs args;
        args.parent = parent;
        args.idealSizeLines = 5;
        auto w = new ListBox();
        w->SetInsetsPt(4, 0);
        w->Create(args);
        auto lbModel = new ListBoxModelStrings();
        w->SetModel(lbModel);
        w->onSelectionChanged = std::bind(&EditAnnotationsWindow::ListBoxSelectionChanged, ew);
        ew->listBox = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        ew->staticRect = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        // WindowBaseLayout* l2 = (WindowBaseLayout*)l;
        // l2->SetInsetsPt(20, 0, 0, 0);
        ew->staticAuthor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        ew->staticModificationDate = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        ew->staticPopup = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Contents:"));
        ew->staticContents = w;
        w->SetInsetsPt(4, 0, 0, 0);
        vbox->AddChild(w);
    }

    {
        EditCreateArgs args;
        args.parent = parent;
        args.isMultiLine = true;
        args.idealSizeLines = 5;
        auto w = new Edit();
        HWND hwnd = w->Create(args);
        CrashIf(!hwnd);
        w->maxDx = 150;
        w->onTextChanged = [ew]() { return ContentsChanged(ew); };
        ew->editContents = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Text Alignment:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticTextAlignment = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = [ew]() { return TextAlignmentSelectionChanged(ew); };
        ew->dropDownTextAlignment = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, "Text Font:");
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticTextFont = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;
        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);

        w->Create(args);
        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = [ew]() { return TextFontSelectionChanged(ew); };
        ew->dropDownTextFont = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Text Size:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticTextSize = w;
        vbox->AddChild(w);
    }

    {
        TrackbarCreateArgs args;
        args.parent = parent;
        args.rangeMin = 8;
        args.rangeMax = 36;

        auto w = new Trackbar();
        w->SetInsetsPt(4, 0, 0, 0);

        w->Create(args);

        w->onPosChanging = [ew](auto&& PH1) { return TextFontSizeChanging(ew, std::forward<decltype(PH1)>(PH1)); };
        ew->trackbarTextSize = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Text Color:"));
        ew->staticTextColor = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;
        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = [ew]() { return TextColorSelectionChanged(ew); };
        ew->dropDownTextColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Line Start:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticLineStart = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = [ew]() { return LineStartSelectionChanged(ew); };
        ew->dropDownLineStart = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Line End:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticLineEnd = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;
        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = [ew]() { return LineEndSelectionChanged(ew); };
        ew->dropDownLineEnd = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Icon:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticIcon = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;
        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = [ew]() { return IconSelectionChanged(ew); };
        ew->dropDownIcon = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, "Border:");
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticBorder = w;
        vbox->AddChild(w);
    }

    {
        TrackbarCreateArgs args;
        args.parent = parent;
        args.rangeMin = borderWidthMin;
        args.rangeMax = borderWidthMax;
        auto w = new Trackbar();
        w->Create(args);
        w->onPosChanging = [ew](auto&& PH1) { return BorderWidthChanging(ew, std::forward<decltype(PH1)>(PH1)); };
        ew->trackbarBorder = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Color:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticColor = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = [ew]() { return ColorSelectionChanged(ew); };
        ew->dropDownColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Interior Color:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticInteriorColor = w;
        vbox->AddChild(w);
    }

    {
        DropDownCreateArgs args;
        args.parent = parent;

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = [ew]() { return InteriorColorSelectionChanged(ew); };
        ew->dropDownInteriorColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Opacity:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticOpacity = w;
        vbox->AddChild(w);
    }

    {
        TrackbarCreateArgs args;
        args.parent = parent;
        args.rangeMin = 0;
        args.rangeMax = 255;

        auto w = new Trackbar();
        w->Create(args);

        w->onPosChanging = [ew](auto&& PH1) { return OpacityChanging(ew, std::forward<decltype(PH1)>(PH1)); };
        ew->trackbarOpacity = w;
        vbox->AddChild(w);
    }

    {
        ButtonCreateArgs args;
        args.parent = parent;
        args.text = "Save...";

        auto w = new Button();
        w->SetInsetsPt(8, 0, 0, 0);
        HWND hwnd = w->Create(args);
        CrashIf(!hwnd);

        w->onClicked = [ew] { return ButtonSaveAttachment(ew); };
        ew->buttonSaveAttachment = w;
        vbox->AddChild(w);
    }

    {
        ButtonCreateArgs args;
        args.parent = parent;
        args.text = "Embed...";

        auto w = new Button();
        w->SetInsetsPt(8, 0, 0, 0);
        HWND hwnd = w->Create(args);
        CrashIf(!hwnd);

        w->onClicked = [ew] { return ButtonEmbedAttachment(ew); };
        ew->buttonEmbedAttachment = w;
        vbox->AddChild(w);
    }

    {
        ButtonCreateArgs args;
        args.parent = parent;
        args.text = "Delete annotation";

        auto w = new Button();
        w->SetInsetsPt(11, 0, 0, 0);
        HWND hwnd = w->Create(args);
        CrashIf(!hwnd);

        // TODO: doesn't work
        // w->SetTextColor(MkColor(0xff, 0, 0));

        w->onClicked = [ew] { return ButtonDeleteHandler(ew); };
        ew->buttonDelete = w;
        vbox->AddChild(w);
    }

    {
        // used to take all available space between the what's above and below
        auto w = new Spacer(0, 0);
        vbox->AddChild(w, 1);
    }

    {
        ButtonCreateArgs args;
        args.parent = parent;
        // TODO: maybe  file name e.g. "Save changes to foo.pdf"
        args.text = _TRA("Save changes to existing PDF");

        auto w = new Button();
        HWND hwnd = w->Create(args);
        CrashIf(!hwnd);

        w->SetIsEnabled(false); // only enabled if there are changes
        w->onClicked = [ew] { return ButtonSaveToCurrentPDFHandler(ew); };
        ew->buttonSaveToCurrentFile = w;
        vbox->AddChild(w);
    }

    {
        ButtonCreateArgs args;
        args.parent = parent;
        // TODO: maybe  file name e.g. "Save changes to foo.pdf"
        args.text = _TRA("Save changes to a new PDF");

        auto w = new Button();
        w->SetInsetsPt(8, 0, 0, 0);
        HWND hwnd = w->Create(args);
        CrashIf(!hwnd);

        w->SetIsEnabled(false); // only enabled if there are changes
        w->onClicked = [ew] { return ButtonSaveToNewFileHandler(ew); };
        ew->buttonSaveToNewFile = w;
        vbox->AddChild(w);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(parent, 4, 8));
    ew->mainLayout = padding;
    HidePerAnnotControls(ew);
}

static void GetAnnotationsFromEngine(EditAnnotationsWindow* ew, TabInfo* tab) {
    Vec<Annotation*>* annots = new Vec<Annotation*>();
    EngineMupdf* engine = GetEngineMupdf(ew);
    EngineGetAnnotations(engine, annots);

    ew->tab = tab;
    tab->editAnnotsWindow = ew;
    ew->annotations = annots;
    RebuildAnnotations(ew);
}

static bool SelectAnnotationInListBox(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!annot) {
        ew->listBox->SetCurrentSelection(-1);
        return false;
    }
    int n = ew->annotations->isize();
    for (int i = 0; i < n; i++) {
        Annotation* a = ew->annotations->at(i);
        if (IsAnnotationEq(a, annot)) {
            ew->listBox->SetCurrentSelection(i);
            UpdateUIForSelectedAnnotation(ew, i);
            return true;
        }
    }
    return false;
}

void AddAnnotationToEditWindow(EditAnnotationsWindow* ew, Annotation* annot) {
    HWND hwnd = ew->hwnd;
    BringWindowToTop(hwnd);
    if (!annot) {
        return;
    }
    ew->skipGoToPage = true;
    bool alreadyExists = SelectAnnotationInListBox(ew, annot);
    if (alreadyExists) {
        delete annot;
        return;
    }
    ew->annotations->Append(annot);
    RebuildAnnotations(ew);
    SelectAnnotationInListBox(ew, annot);
}

void SelectAnnotationInEditWindow(EditAnnotationsWindow* ew, Annotation* annot) {
    CrashIf(!ew);
    if (!ew || !annot) {
        return;
    }
    ew->skipGoToPage = true;
    HWND hwnd = ew->hwnd;
    BringWindowToTop(hwnd);
    SelectAnnotationInListBox(ew, annot);
}

void StartEditAnnotations(TabInfo* tab, Annotation* annot) {
    Vec<Annotation*> annots;
    annots.Append(annot);
    StartEditAnnotations(tab, annots);
}

// takes ownership of annots
void StartEditAnnotations(TabInfo* tab, Vec<Annotation*>& annots) {
    CrashIf(!tab->AsFixed()->GetEngine());
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    if (ew) {
        for (auto annot : annots) {
            AddAnnotationToEditWindow(ew, annot);
        }
        return;
    }
    ew = new EditAnnotationsWindow();
    CreateCustomArgs args;
    HMODULE h = GetModuleHandleW(nullptr);
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    args.icon = LoadIconW(h, iconName);
    // mainWindow->isDialog = true;
    args.bgColor = MkGray(0xee);
    args.title = _TRA("Annotations");

    // PositionCloseTo(w, args->hwndRelatedTo);
    // SIZE winSize = {w->initialSize.dx, w->initialSize.Height};
    // LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.cx, winSize.cy};
    ew->CreateCustom(args);

    CreateMainLayout(ew);
    ew->tab = tab;
    tab->editAnnotsWindow = ew;

    GetAnnotationsFromEngine(ew, tab);

    // size our editor window to be the same height as main window
    int minDy = 720;
    // TODO: this is slightly less that wanted
    HWND hwnd = tab->win->hwndCanvas;
    auto rc = ClientRect(hwnd);
    if (rc.dy > 0) {
        minDy = rc.dy;
        // if it's a tall window, up the number of items in list box
        // from 5 to 14
        if (minDy > 1024) {
            ew->listBox->idealSizeLines = 14;
        }
    }
    LayoutAndSizeToContent(ew->mainLayout, 520, minDy, ew->hwnd);
    HwndPositionToTheRightOf(ew->hwnd, tab->win->hwndFrame);
    ew->skipGoToPage = !annots.empty();
    if (!annots.empty()) {
        SelectAnnotationInListBox(ew, annots[0]);
    }

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    ew->SetIsVisible(true);

    DeleteVecMembers(annots);
}

static PdfColor GetAnnotationHighlightColor() {
    auto& a = gGlobalPrefs->annotations;
    ParsedColor* parsedCol = GetParsedColor(a.highlightColor, a.highlightColorParsed);
    return parsedCol->pdfCol;
}

static PdfColor GetAnnotationUnderlineColor() {
    auto& a = gGlobalPrefs->annotations;
    ParsedColor* parsedCol = GetParsedColor(a.underlineColor, a.underlineColorParsed);
    return parsedCol->pdfCol;
}

static PdfColor GetAnnotationTextIconColor() {
    auto& a = gGlobalPrefs->annotations;
    ParsedColor* parsedCol = GetParsedColor(a.textIconColor, a.textIconColorParsed);
    return parsedCol->pdfCol;
}

// caller needs to free()
static char* GetAnnotationTextIcon() {
    char* s = str::Dup(gGlobalPrefs->annotations.textIconType);
    // this way user can use "new paragraph" and we'll match "NewParagraph"
    str::RemoveCharsInPlace(s, " ");
    int idx = seqstrings::StrToIdxIS(gAnnotationTextIcons, s);
    if (idx < 0) {
        str::ReplaceWithCopy(&s, "Note");
    } else {
        const char* real = seqstrings::IdxToStr(gAnnotationTextIcons, idx);
        str::ReplaceWithCopy(&s, real);
    }
    return s;
}

static const char* getuser(void) {
    const char* u;
    u = getenv("USER");
    if (!u) {
        u = getenv("USERNAME");
    }
    if (!u) {
        u = "user";
    }
    return u;
}

Annotation* EngineMupdfCreateAnnotation(EngineBase* engine, AnnotationType typ, int pageNo, PointF pos) {
    EngineMupdf* epdf = AsEngineMupdf(engine);
    fz_context* ctx = epdf->ctx;

    auto pageInfo = epdf->GetFzPageInfo(pageNo, true);

    ScopedCritSec cs(epdf->ctxAccess);

    auto page = pdf_page_from_fz_page(ctx, pageInfo->page);
    enum pdf_annot_type atyp = (enum pdf_annot_type)typ;

    auto annot = pdf_create_annot(ctx, page, atyp);

    pdf_set_annot_modification_date(ctx, annot, time(nullptr));
    if (pdf_annot_has_author(ctx, annot)) {
        char* defAuthor = gGlobalPrefs->annotations.defaultAuthor;
        // if "(none)" we don't set it
        if (!str::Eq(defAuthor, "(none)")) {
            const char* author = getuser();
            if (!str::EmptyOrWhiteSpaceOnly(defAuthor)) {
                author = defAuthor;
            }
            pdf_set_annot_author(ctx, annot, author);
        }
    }

    switch (typ) {
        case AnnotationType::Text:
        case AnnotationType::FreeText:
        case AnnotationType::Stamp:
        case AnnotationType::Caret:
        case AnnotationType::Square:
        case AnnotationType::Circle: {
            fz_rect trect = pdf_annot_rect(ctx, annot);
            float dx = trect.x1 - trect.x0;
            trect.x0 = pos.x;
            trect.x1 = trect.x0 + dx;
            float dy = trect.y1 - trect.y0;
            trect.y0 = pos.y;
            trect.y1 = trect.y0 + dy;
            pdf_set_annot_rect(ctx, annot, trect);
        } break;
        case AnnotationType::Line: {
            fz_point a{pos.x, pos.y};
            fz_point b{pos.x + 100, pos.y + 50};
            pdf_set_annot_line(ctx, annot, a, b);
        } break;
    }
    if (typ == AnnotationType::FreeText) {
        pdf_set_annot_contents(ctx, annot, "This is a text...");
        pdf_set_annot_border(ctx, annot, 1);
    }

    pdf_update_annot(ctx, annot);
    auto res = MakeAnnotationPdf(epdf, annot, pageNo);
    if (typ == AnnotationType::Text) {
        AutoFreeStr iconName = GetAnnotationTextIcon();
        if (!str::EqI(iconName, "Note")) {
            SetIconName(res, iconName.Get());
        }
        auto col = GetAnnotationTextIconColor();
        SetColor(res, col);
    } else if (typ == AnnotationType::Underline) {
        auto col = GetAnnotationUnderlineColor();
        SetColor(res, col);
    } else if (typ == AnnotationType::Highlight) {
        auto col = GetAnnotationHighlightColor();
        SetColor(res, col);
    }
    pdf_drop_annot(ctx, annot);
    return res;
}
