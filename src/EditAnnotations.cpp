/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
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
#include "EngineFzUtil.h"
#include "EnginePdfImpl.h"
#include "EngineMulti.h"
#include "EngineCreate.h"

#include "Translations.h"
#include "SumatraConfig.h"
#include "DisplayMode.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "EditAnnotations.h"
#include "SumatraPDF.h"

using std::placeholders::_1;

constexpr int borderWidthMin = 0;
constexpr int borderWidthMax = 12;

// clang-format off
static const char *gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";
static const char *gSoundIcons = "Speaker\0Mic\0";
static const char* gTextIcons = "Comment\0Help\0Insert\0Key\0NewParagraph\0Note\0Paragraph\0";
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

static EnginePdf* GetEnginePdf(EditAnnotationsWindow* ew) {
    DisplayModel* dm = ew->tab->AsFixed();
    return AsEnginePdf(dm->GetEngine());
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
    EnginePdf* engine = GetEnginePdf(ew);
    return EnginePdfHasUnsavedAnnotations(engine);
}

static void EnableSaveIfAnnotationsChanged(EditAnnotationsWindow* ew) {
    bool didChange = DidAnnotationsChange(ew);
    ew->buttonSaveToCurrentFile->SetIsEnabled(didChange);
    ew->buttonSaveToNewFile->SetIsEnabled(didChange);
}

static void RebuildAnnotations(EditAnnotationsWindow* ew) {
    auto model = new ListBoxModelStrings();
    int n = 0;
    if (ew->annotations) {
        n = ew->annotations->isize();
    }

    str::Str s;
    for (int i = 0; i < n; i++) {
        auto annot = ew->annotations->at(i);
        if (annot->isDeleted) {
            continue;
        }
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

extern void ReloadDocument(WindowInfo* win, bool autorefresh);
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
    EnginePdf* engine = GetEnginePdf(ew);
    strconv::StackWstrToUtf8 path{engine->FileName()};
    bool ok = EnginePdfSaveUpdated(engine, {}, [&tab, &path](std::string_view mupdfErr) {
        str::Str msg;
        // TODO: duplicated message
        msg.AppendFmt(_TRU("Saving of '%s' failed with: '%s'"), path.Get(), mupdfErr.data());
        tab->win->ShowNotification(msg.AsView(), NotificationOptions::Warning);
    });
    if (!ok) {
        return;
    }
    str::Str msg;
    msg.AppendFmt(_TRU("Saved annotations to '%s'"), path.Get());
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

static void SerializePdfColor(PdfColor c, str::Str& out) {
    u8 r, g, b, a;
    UnpackPdfColor(c, r, g, b, a);
    out.AppendFmt("#%02x%02x%02x", r, g, b);
}

static bool ParsePdfColor(PdfColor* destColor, std::string_view sv) {
    CrashIf(!destColor);
    const char* txt = sv.data();
    if (str::StartsWith(txt, "0x")) {
        txt += 2;
    } else if (str::StartsWith(txt, "#")) {
        txt += 1;
    }
    unsigned int r, g, b;
    bool ok = str::Parse(txt, "%2x%2x%2x%$", &r, &g, &b);
    *destColor = MkPdfColor(r, g, b, 0xff);
    return ok;
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
    PdfColor col{0};
    ParsePdfColor(&col, sv);
    return col;
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
    s.AppendFmt(_TRU("Rect: x=%d y=%d dx=%d dy=%d"), x, y, dx, dy);
    ew->staticRect->SetText(s.AsView());
    ew->staticRect->SetIsVisible(true);
}

static void DoAuthor(EditAnnotationsWindow* ew, Annotation* annot) {
    bool isVisible = !Author(annot).empty();
    if (!isVisible) {
        return;
    }
    str::Str s;
    s.AppendFmt(_TRU("Author: %s"), Author(annot).data());
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
    s.Append(_TRU("Date:"));
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
    s.AppendFmt(_TRU("Popup: %d 0 R"), popupId);
    ew->staticPopup->SetText(s.AsView());
    ew->staticPopup->SetIsVisible(true);
}

static void DoContents(EditAnnotationsWindow* ew, Annotation* annot) {
    str::Str s = Contents(annot);
    // TODO: don't replace if already is "\r\n"
    s.Replace("\n", "\r\n");
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
    AutoFreeStr s = str::Format(_TRU("Text Size: %d"), fontSize);
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
    AutoFreeStr s = str::Format(_TRU("Text Size: %d"), fontSize);
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
    AutoFreeStr s = str::Format(_TRU("Border: %d"), borderWidth);
    ew->staticBorder->SetText(s.AsView());
    ew->trackbarBorder->SetValue(borderWidth);
    ew->staticBorder->SetIsVisible(true);
    ew->trackbarBorder->SetIsVisible(true);
}

static void BorderWidthChanging(EditAnnotationsWindow* ew, TrackbarPosChangingEvent* ev) {
    ev->didHandle = true;
    int borderWidth = ev->pos;
    SetBorderWidth(ew->annot, borderWidth);
    AutoFreeStr s = str::Format(_TRU("Border: %d"), borderWidth);
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
    AutoFreeStr s = str::Format(_TRU("Opacity: %d"), opacity);
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
    AutoFreeStr s = str::Format(_TRU("Opacity: %d"), opacity);
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

static void ButtonDeleteHandler(EditAnnotationsWindow* ew) {
    CrashIf(!ew->annot);
    Delete(ew->annot);
    RebuildAnnotations(ew);
    UpdateUIForSelectedAnnotation(ew, -1);
    WindowInfoRerender(ew->tab->win);
}

static void ListBoxSelectionChanged(EditAnnotationsWindow* ew, ListBoxSelectionChangedEvent* ev) {
    int itemNo = ev->idx;
    UpdateUIForSelectedAnnotation(ew, itemNo);
}

// TODO: text changes are not immediately reflected in tooltip
// TODO: there seems to be a leak
static void ContentsChanged(EditAnnotationsWindow* ew, EditTextChangedEvent* ev) {
    ev->didHandle = true;
    SetContents(ew->annot, ev->text);
    EnableSaveIfAnnotationsChanged(ew);
    WindowInfoRerender(ew->tab->win);
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
        w->onSelectionChanged = std::bind(ListBoxSelectionChanged, ew, _1);
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
        auto w = CreateStatic(parent, _TRU("Contents:"));
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
        w->onTextChanged = std::bind(ContentsChanged, ew, _1);
        ew->editContents = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Text Alignment:"));
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
        w->onSelectionChanged = std::bind(TextAlignmentSelectionChanged, ew, _1);
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
        w->onSelectionChanged = std::bind(TextFontSelectionChanged, ew, _1);
        ew->dropDownTextFont = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Text Size:"));
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
        w->onPosChanging = std::bind(TextFontSizeChanging, ew, _1);
        ew->trackbarTextSize = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Text Color:"));
        ew->staticTextColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = std::bind(TextColorSelectionChanged, ew, _1);
        ew->dropDownTextColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Line Start:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticLineStart = w;
        vbox->AddChild(w);
    }

    {
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->onSelectionChanged = std::bind(LineStartEndSelectionChanged, ew, _1);
        ew->dropDownLineStart = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Line End:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticLineEnd = w;
        vbox->AddChild(w);
    }

    {
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->onSelectionChanged = std::bind(LineStartEndSelectionChanged, ew, _1);
        ew->dropDownLineEnd = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Icon:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticIcon = w;
        vbox->AddChild(w);
    }

    {
        auto w = new DropDownCtrl(parent);
        w->SetInsetsPt(4, 0, 0, 0);
        bool ok = w->Create();
        CrashIf(!ok);
        w->onSelectionChanged = std::bind(IconSelectionChanged, ew, _1);
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
        w->onPosChanging = std::bind(BorderWidthChanging, ew, _1);
        ew->trackbarBorder = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Color:"));
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
        w->onSelectionChanged = std::bind(ColorSelectionChanged, ew, _1);
        ew->dropDownColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Interior Color:"));
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
        w->onSelectionChanged = std::bind(InteriorColorSelectionChanged, ew, _1);
        ew->dropDownInteriorColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRU("Opacity:"));
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
        w->onPosChanging = std::bind(OpacityChanging, ew, _1);
        ew->trackbarOpacity = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetInsetsPt(8, 0, 0, 0);
        w->SetText("Save...");
        bool ok = w->Create();
        CrashIf(!ok);
        w->onClicked = std::bind(&ButtonSaveAttachment, ew);
        ew->buttonSaveAttachment = w;
        vbox->AddChild(w);
    }

    {
        auto w = new ButtonCtrl(parent);
        w->SetInsetsPt(8, 0, 0, 0);
        w->SetText("Embed...");
        bool ok = w->Create();
        CrashIf(!ok);
        w->onClicked = std::bind(&ButtonEmbedAttachment, ew);
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
        w->onClicked = std::bind(&ButtonDeleteHandler, ew);
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
        w->onClicked = std::bind(&ButtonSaveToCurrentPDFHandler, ew);
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
        w->onClicked = std::bind(&ButtonSaveToNewFileHandler, ew);
        ew->buttonSaveToNewFile = w;
        vbox->AddChild(w);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(parent, 4, 8));
    ew->mainLayout = padding;
    HidePerAnnotControls(ew);
}

static void GetAnnotationsFromEngine(EditAnnotationsWindow* ew, TabInfo* tab) {
    Vec<Annotation*>* annots = new Vec<Annotation*>();
    EnginePdf* engine = GetEnginePdf(ew);
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
    if (!ew || !annot) {
        return;
    }
    ew->skipGoToPage = true;
    HWND hwnd = ew->mainWindow->hwnd;
    BringWindowToTop(hwnd);
    SelectAnnotationInListBox(ew, annot);
}

// takes ownership of selectedAnnot
void StartEditAnnotations(TabInfo* tab, Annotation* annot) {
    CrashIf(!tab->AsFixed()->GetEngine());
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    if (ew) {
        AddAnnotationToEditWindow(ew, annot);
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
    mainWindow->onClose = std::bind(WndCloseHandler, ew, _1);
    mainWindow->onSize = std::bind(WndSizeHandler, ew, _1);

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
    ew->skipGoToPage = (annot != nullptr);
    SelectAnnotationInListBox(ew, annot);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    mainWindow->SetIsVisible(true);

    delete annot;
}

static PdfColor ToPdfColor(COLORREF c) {
    u8 r, g, b, a;
    UnpackColor(c, r, g, b, a);
    // COLORREF has a of 0 for opaque but for PDF use
    // opaque is 0xff
    if (a == 0) {
        a = 0xff;
    }
    auto res = MkPdfColor(r, g, b, a);
    return res;
}

PdfColor GetAnnotationHighlightColor() {
    COLORREF col = gGlobalPrefs->annotations.highlightColor;
    return ToPdfColor(col);
}
