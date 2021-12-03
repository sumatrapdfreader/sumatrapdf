/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "EngineMupdfImpl.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
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

struct EditAnnotationsWindow {
    TabInfo* tab{nullptr};
    Window* mainWindow{nullptr};
    LayoutBase* mainLayout{nullptr};

    ListBoxCtrl* listBox{nullptr};
    StaticCtrl* staticRect{nullptr};
    StaticCtrl* staticAuthor{nullptr};
    StaticCtrl* staticModificationDate{nullptr};
    StaticCtrl* staticPopup{nullptr};
    StaticCtrl* staticContents{nullptr};
    EditCtrl* editContents{nullptr};
    StaticCtrl* staticTextAlignment{nullptr};
    DropDownCtrl* dropDownTextAlignment{nullptr};
    StaticCtrl* staticTextFont{nullptr};
    DropDownCtrl* dropDownTextFont{nullptr};
    StaticCtrl* staticTextSize{nullptr};
    TrackbarCtrl* trackbarTextSize{nullptr};
    StaticCtrl* staticTextColor{nullptr};
    DropDownCtrl* dropDownTextColor{nullptr};

    StaticCtrl* staticLineStart{nullptr};
    DropDownCtrl* dropDownLineStart{nullptr};
    StaticCtrl* staticLineEnd{nullptr};
    DropDownCtrl* dropDownLineEnd{nullptr};

    StaticCtrl* staticIcon{nullptr};
    DropDownCtrl* dropDownIcon{nullptr};

    StaticCtrl* staticBorder{nullptr};
    TrackbarCtrl* trackbarBorder{nullptr};

    StaticCtrl* staticColor{nullptr};
    DropDownCtrl* dropDownColor{nullptr};
    StaticCtrl* staticInteriorColor{nullptr};
    DropDownCtrl* dropDownInteriorColor{nullptr};

    StaticCtrl* staticOpacity{nullptr};
    TrackbarCtrl* trackbarOpacity{nullptr};

    ButtonCtrl* buttonSaveAttachment{nullptr};
    ButtonCtrl* buttonEmbedAttachment{nullptr};

    ButtonCtrl* buttonDelete{nullptr};

    ButtonCtrl* buttonSaveToCurrentFile{nullptr};
    ButtonCtrl* buttonSaveToNewFile{nullptr};

    ListBoxModel* lbModel{nullptr};

    Vec<Annotation*>* annotations{nullptr};
    // currently selected annotation
    Annotation* annot{nullptr};

    bool skipGoToPage{false};

    str::Str currTextColor;
    str::Str currCustomColor;
    str::Str currCustomInteriorColor;

    ~EditAnnotationsWindow();
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
    delete mainWindow;
    delete mainLayout;
    delete lbModel;
}

static bool DidAnnotationsChange(EditAnnotationsWindow* ew) {
    EngineMupdf* engine = GetEngineMupdf(ew);
    return EngineMupdfHasUnsavedAnnotations(engine);
}

static void EnableSaveIfAnnotationsChanged(EditAnnotationsWindow* ew) {
    bool didChange = DidAnnotationsChange(ew);
    ew->buttonSaveToCurrentFile->SetIsEnabled(didChange);
    ew->buttonSaveToNewFile->SetIsEnabled(didChange);
}

static void RemoveDeletedAnnotations(Vec<Annotation*>* v) {
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
        s.AppendView(AnnotationReadableName(annot->type));
        model->strings.Append(s.AsView());
    }

    ew->listBox->SetModel(model);
    delete ew->lbModel;
    ew->lbModel = model;
    EnableSaveIfAnnotationsChanged(ew);
}

static void WndCloseHandler(EditAnnotationsWindow* ew, WindowCloseEvent* ev) {
    CrashIf(ew->mainWindow != ev->w);
    ew->tab->editAnnotsWindow = nullptr;
    delete ew;
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
    TempStr path = ToUtf8Temp(engine->FileName());
    bool ok = EngineMupdfSaveUpdated(engine, {}, [&tab, &path](std::string_view mupdfErr) {
        str::Str msg;
        // TODO: duplicated message
        msg.AppendFmt(_TRA("Saving of '%s' failed with: '%s'"), path.Get(), mupdfErr.data());
        tab->win->ShowNotification(msg.AsView(), NotificationOptions::Warning);
    });
    if (!ok) {
        return;
    }
    str::Str msg;
    msg.AppendFmt(_TRA("Saved annotations to '%s'"), path.Get());
    tab->win->ShowNotification(msg.AsView());

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

static void ItemsFromSeqstrings(Vec<std::string_view>& items, const char* strings) {
    while (*strings) {
        items.Append(strings);
        strings = seqstrings::SkipStr(strings);
    }
}

static void DropDownFillColors(DropDownCtrl* w, PdfColor col, str::Str& customColor) {
    Vec<std::string_view> items;
    ItemsFromSeqstrings(items, gColors);
    const char* colorName = GetKnownColorName(col);
    int idx = seqstrings::StrToIdx(gColors, colorName);
    if (idx == -1) {
        customColor.Reset();
        SerializePdfColor(col, customColor);
        items.Append(customColor.AsView());
        idx = items.isize() - 1;
    }
    w->SetItems(items);
    w->SetCurrentSelection(idx);
}

static PdfColor GetDropDownColor(std::string_view sv) {
    int idx = seqstrings::StrToIdx(gColors, sv.data());
    if (idx >= 0) {
        int nMaxColors = (int)dimof(gColorsValues);
        CrashIf(idx >= nMaxColors);
        if (idx < nMaxColors) {
            return gColorsValues[idx];
        }
        return 0;
    }
    ParsedColor col;
    ParseColor(col, sv.data());
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
    ew->staticRect->SetText(s.AsView());
    ew->staticRect->SetIsVisible(true);
}

static void DoAuthor(EditAnnotationsWindow* ew, Annotation* annot) {
    bool isVisible = !Author(annot).empty();
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.AppendFmt(_TRA("Author: %s"), Author(annot).data());
    ew->staticAuthor->SetText(s.AsView());
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
    ew->staticModificationDate->SetText(s.AsView());
    ew->staticModificationDate->SetIsVisible(true);
}

static void DoPopup(EditAnnotationsWindow* ew, Annotation* annot) {
    int popupId = PopupId(annot);
    if (popupId < 0) {
        return;
    }
    str::Str s;
    s.AppendFmt(_TRA("Popup: %d 0 R"), popupId);
    ew->staticPopup->SetText(s.AsView());
    ew->staticPopup->SetIsVisible(true);
}

static void DoContents(EditAnnotationsWindow* ew, Annotation* annot) {
    str::Str s = Contents(annot);
    // TODO: don't replace if already is "\r\n"
    Replace(s, "\n", "\r\n");
    ew->editContents->SetText(s.AsView());
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

static void TextAlignmentSelectionChanged(EditAnnotationsWindow* ew, DropDownSelectionChangedEvent* ev) {
    int newQuadding = ev->idx;
    SetQuadding(ew->annot, newQuadding);
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
}

static void DoTextFont(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    std::string_view fontName = DefaultAppearanceTextFont(annot);
    // TODO: might have other fonts, like "Symb" and "ZaDb"
    auto itemNo = seqstrings::StrToIdx(gFontNames, fontName.data());
    if (itemNo < 0) {
        return;
    }
    ew->dropDownTextFont->SetItemsSeqStrings(gFontReadableNames);
    ew->dropDownTextFont->SetCurrentSelection(itemNo);
    ew->staticTextFont->SetIsVisible(true);
    ew->dropDownTextFont->SetIsVisible(true);
}

static void TextFontSelectionChanged(EditAnnotationsWindow* ew, DropDownSelectionChangedEvent* ev) {
    ev->didHandle = true;
    const char* font = seqstrings::IdxToStr(gFontNames, ev->idx);
    SetDefaultAppearanceTextFont(ew->annot, font);
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
}

static void DoTextSize(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    int fontSize = DefaultAppearanceTextSize(annot);
    AutoFreeStr s = str::Format(_TRA("Text Size: %d"), fontSize);
    ew->staticTextSize->SetText(s.AsView());
    SetDefaultAppearanceTextSize(ew->annot, fontSize);
    ew->trackbarTextSize->SetValue(fontSize);
    ew->staticTextSize->SetIsVisible(true);
    ew->trackbarTextSize->SetIsVisible(true);
}

static void TextFontSizeChanging(EditAnnotationsWindow* ew, TrackbarPosChangingEvent* ev) {
    ev->didHandle = true;
    int fontSize = ev->pos;
    SetDefaultAppearanceTextSize(ew->annot, fontSize);
    AutoFreeStr s = str::Format(_TRA("Text Size: %d"), fontSize);
    ew->staticTextSize->SetText(s.AsView());
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
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

static void TextColorSelectionChanged(EditAnnotationsWindow* ew, DropDownSelectionChangedEvent* ev) {
    auto col = GetDropDownColor(ev->item);
    SetDefaultAppearanceTextColor(ew->annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
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
    ew->staticBorder->SetText(s.AsView());
    ew->trackbarBorder->SetValue(borderWidth);
    ew->staticBorder->SetIsVisible(true);
    ew->trackbarBorder->SetIsVisible(true);
}

static void BorderWidthChanging(EditAnnotationsWindow* ew, TrackbarPosChangingEvent* ev) {
    ev->didHandle = true;
    int borderWidth = ev->pos;
    SetBorderWidth(ew->annot, borderWidth);
    AutoFreeStr s = str::Format(_TRA("Border: %d"), borderWidth);
    ew->staticBorder->SetText(s.AsView());
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
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

static void LineStartEndSelectionChanged(EditAnnotationsWindow* ew, DropDownSelectionChangedEvent* ev) {
    int start = 0;
    int end = 0;
    GetLineEndingStyles(ew->annot, &start, &end);
    int newVal = ev->idx;
    if (ev->dropDown == ew->dropDownLineStart) {
        start = newVal;
    } else {
        CrashIf(ev->dropDown != ew->dropDownLineEnd);
        end = newVal;
    }
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
}

static void DoIcon(EditAnnotationsWindow* ew, Annotation* annot) {
    std::string_view itemName = IconName(annot);
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
    if (!items || itemName.empty()) {
        return;
    }
    ew->dropDownIcon->SetItemsSeqStrings(items);
    int idx = FindStringInArray(items, itemName.data(), 0);
    ew->dropDownIcon->SetCurrentSelection(idx);
    ew->staticIcon->SetIsVisible(true);
    ew->dropDownIcon->SetIsVisible(true);
}

static void IconSelectionChanged(EditAnnotationsWindow* ew, DropDownSelectionChangedEvent* ev) {
    SetIconName(ew->annot, ev->item);
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
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

static void ColorSelectionChanged(EditAnnotationsWindow* ew, DropDownSelectionChangedEvent* ev) {
    auto col = GetDropDownColor(ev->item);
    SetColor(ew->annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
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

static void InteriorColorSelectionChanged(EditAnnotationsWindow* ew, DropDownSelectionChangedEvent* ev) {
    auto col = GetDropDownColor(ev->item);
    SetInteriorColor(ew->annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
}

static void DoOpacity(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::Highlight) {
        return;
    }
    int opacity = Opacity(ew->annot);
    AutoFreeStr s = str::Format(_TRA("Opacity: %d"), opacity);
    ew->staticOpacity->SetText(s.AsView());
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
    ev->didHandle = true;
    int opacity = ev->pos;
    SetOpacity(ew->annot, opacity);
    AutoFreeStr s = str::Format(_TRA("Opacity: %d"), opacity);
    ew->staticOpacity->SetText(s.AsView());
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
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
    LayoutAndSizeToContent(ew->mainLayout, dx, dy, ew->mainWindow->hwnd);
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
    MessageBoxNYI(ew->mainWindow->hwnd);
}

static void ButtonEmbedAttachment(EditAnnotationsWindow* ew) {
    CrashIf(!ew->annot);
    // TODO: implement me
    MessageBoxNYI(ew->mainWindow->hwnd);
}

void DeleteAnnotationAndUpdateUI(TabInfo* tab, EditAnnotationsWindow* ew, Annotation* annot) {
    annot = FindMatchingAnnotation(ew, annot);
    Delete(annot);
    if (ew != nullptr) {
        // can be null if called from Menu.cpp and annotations window is not visible
        RebuildAnnotations(ew);
        UpdateUIForSelectedAnnotation(ew, -1);
    }
    WindowInfoRerender(tab->win);
}

static void ButtonDeleteHandler(EditAnnotationsWindow* ew) {
    CrashIf(!ew->annot);
    DeleteAnnotationAndUpdateUI(ew->tab, ew, ew->annot);
}

static void ListBoxSelectionChanged(EditAnnotationsWindow* ew, ListBoxSelectionChangedEvent* ev) {
    int itemNo = ev->idx;
    UpdateUIForSelectedAnnotation(ew, itemNo);
}

static UINT_PTR gWindowInfoRerenderTimer = 0;
static WindowInfo* gWindowInfoForRender = nullptr;

// TODO: there seems to be a leak
static void ContentsChanged(EditAnnotationsWindow* ew, EditTextChangedEvent* ev) {
    ev->didHandle = true;
    SetContents(ew->annot, ev->text);
    EnableSaveIfAnnotationsChanged(ew);

    WindowInfo* win = ew->tab->win;
    if (gWindowInfoRerenderTimer != 0) {
        // logf("ContentsChanged: killing existing timer for re-render of WindowInfo\n");
        KillTimer(win->hwndCanvas, gWindowInfoRerenderTimer);
        gWindowInfoRerenderTimer = 0;
    }
    UINT timeoutInMs = 1000;
    gWindowInfoForRender = win;
    gWindowInfoRerenderTimer = SetTimer(win->hwndCanvas, 1, timeoutInMs, [](HWND, UINT, UINT_PTR, DWORD) {
        if (WindowInfoStillValid(gWindowInfoForRender)) {
            // logf("ContentsChanged: re-rendering WindowInfo\n");
            WindowInfoRerender(gWindowInfoForRender);
        } else {
            // logf("ContentsChanged: NOT re-rendering WindowInfo because is not valid anymore\n");
        }
        gWindowInfoRerenderTimer = 0;
    });
}

static void WndSizeHandler(EditAnnotationsWindow* ew, SizeEvent* ev) {
    int dx = ev->dx;
    int dy = ev->dy;
    HWND hwnd = ev->hwnd;
    if (dx == 0 || dy == 0) {
        return;
    }
    ev->didHandle = true;
    InvalidateRect(hwnd, nullptr, false);
    if (false && ew->mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    LayoutToSize(ew->mainLayout, {dx, dy});
}

static StaticCtrl* CreateStatic(HWND parent, std::string_view sv = {}) {
    auto w = new StaticCtrl(parent);
    bool ok = w->Create();
    CrashIf(!ok);
    w->SetText(sv);
    return w;
}

static void CreateMainLayout(EditAnnotationsWindow* ew) {
    HWND parent = ew->mainWindow->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto w = new ListBoxCtrl(parent);
        w->idealSizeLines = 5;
        w->SetInsetsPt(4, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        ew->lbModel = new ListBoxModelStrings();
        w->SetModel(ew->lbModel);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return ListBoxSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new EditCtrl(parent);
        w->isMultiLine = true;
        w->idealSizeLines = 5;
        bool ok = w->Create();
        CrashIf(!ok);
        w->maxDx = 150;
        w->onTextChanged = [ew](auto&& PH1) { return ContentsChanged(ew, std::forward<decltype(PH1)>(PH1)); };
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return TextAlignmentSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return TextFontSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new TrackbarCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        w->rangeMin = 8;
        w->rangeMax = 36;
        bool ok = w->Create();
        CrashIf(!ok);
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return TextColorSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return LineStartEndSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return LineStartEndSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->onSelectionChanged = [ew](auto&& PH1) { return IconSelectionChanged(ew, std::forward<decltype(PH1)>(PH1)); };
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
        auto w = new TrackbarCtrl(parent);
        w->rangeMin = borderWidthMin;
        w->rangeMax = borderWidthMax;
        bool ok = w->Create();
        CrashIf(!ok);
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return ColorSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = [ew](auto&& PH1) {
            return InteriorColorSelectionChanged(ew, std::forward<decltype(PH1)>(PH1));
        };
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
        auto w = new TrackbarCtrl(parent);
        w->rangeMin = 0;
        w->rangeMax = 255;
        bool ok = w->Create();
        CrashIf(!ok);
        w->onPosChanging = [ew](auto&& PH1) { return OpacityChanging(ew, std::forward<decltype(PH1)>(PH1)); };
        ew->trackbarOpacity = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetInsetsPt(8, 0, 0, 0);
        w->SetText("Save...");
        bool ok = w->Create();
        CrashIf(!ok);
        w->onClicked = [ew] { return ButtonSaveAttachment(ew); };
        ew->buttonSaveAttachment = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetInsetsPt(8, 0, 0, 0);
        w->SetText("Embed...");
        bool ok = w->Create();
        CrashIf(!ok);
        w->onClicked = [ew] { return ButtonEmbedAttachment(ew); };
        ew->buttonEmbedAttachment = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetInsetsPt(11, 0, 0, 0);
        w->SetText("Delete annotation");
        // TODO: doesn't work
        w->SetTextColor(MkColor(0xff, 0, 0));
        bool ok = w->Create();
        CrashIf(!ok);
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
        auto w = new ButtonCtrl(parent);
        // TODO: maybe  file name e.g. "Save changes to foo.pdf"
        w->SetText(_TR("Save changes to existing PDF"));
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetIsEnabled(false); // only enabled if there are changes
        w->onClicked = [ew] { return ButtonSaveToCurrentPDFHandler(ew); };
        ew->buttonSaveToCurrentFile = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetInsetsPt(8, 0, 0, 0);
        w->SetText(_TR("Save changes to a new PDF"));
        bool ok = w->Create();
        CrashIf(!ok);
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
    HWND hwnd = ew->mainWindow->hwnd;
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
    HWND hwnd = ew->mainWindow->hwnd;
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
    auto mainWindow = new Window();
    HMODULE h = GetModuleHandleW(nullptr);
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    mainWindow->hIcon = LoadIconW(h, iconName);

    mainWindow->isDialog = true;
    mainWindow->backgroundColor = MkGray(0xee);
    mainWindow->SetText(_TR("Annotations"));
    // PositionCloseTo(w, args->hwndRelatedTo);
    // SIZE winSize = {w->initialSize.dx, w->initialSize.Height};
    // LimitWindowSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.cx, winSize.cy};
    bool ok = mainWindow->Create();
    CrashIf(!ok);
    mainWindow->onClose = [ew](auto&& PH1) { return WndCloseHandler(ew, std::forward<decltype(PH1)>(PH1)); };
    mainWindow->onSize = [ew](auto&& PH1) { return WndSizeHandler(ew, std::forward<decltype(PH1)>(PH1)); };

    ew->mainWindow = mainWindow;
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
    LayoutAndSizeToContent(ew->mainLayout, 520, minDy, mainWindow->hwnd);
    HwndPositionToTheRightOf(mainWindow->hwnd, tab->win->hwndFrame);
    ew->skipGoToPage = !annots.empty();
    if (!annots.empty()) {
        SelectAnnotationInListBox(ew, annots[0]);
    }

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    mainWindow->SetIsVisible(true);

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
            SetIconName(res, iconName.AsView());
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
