/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

extern "C" {
#include <mupdf/pdf.h>
}

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "EngineMupdf.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Toolbar.h"
#include "WindowTab.h"
#include "EditAnnotations.h"
#include "FormFields.h"
#include "SumatraPDF.h"
#include "DarkMode_win.h"

#include "Theme.h"

constexpr int borderWidthMin = 0;
constexpr int borderWidthMax = 12;

// clang-format off
static SeqStrings gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";
static SeqStrings gSoundIcons = "Speaker\0Mic\0";
static SeqStrings gStampIcons =
    "Approved\0AsIs\0Confidential\0Departmental\0Draft\0Experimental\0Expired\0Final\0ForComment\0ForPublicRelease\0NotApproved\0NotForPublicRelease\0Sold\0TopSecret\0";
// those are in order of pdf_line_ending enum in annot.h
static SeqStrings gLineEndingStyles =
    "None\0Square\0Circle\0Diamond\0OpenArrow\0ClosedArrow\0Butt\0ROpenArrow\0RClosedArrow\0Slash\0";
static SeqStrings gColors =
    "Transparent\0Aqua\0Black\0Blue\0Fuchsia\0Gray\0Green\0Lime\0Maroon\0Navy\0Olive\0Orange\0Purple\0Red\0Silver\0Teal\0White\0Yellow\0";
static SeqStrings gFontNames = "Cour\0Helv\0TiRo\0";
static SeqStrings gFontReadableNames = "Courier\0Helvetica\0TimesRoman\0";

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

// list of annotations where GetColor() returns background color
// TODO: probably incomplete;
static AnnotationType gAnnotsIsColorBackground[] = {
    AnnotationType::FreeText,
};
// clang-format on

static TempStr GetKnownColorNameTemp(PdfColor c) {
    int n = dimofi(gColorsValues);
    for (int i = 0; i < n; i++) {
        if (c == gColorsValues[i]) {
            return SeqStrByIndex(gColors, i);
        }
    }
    return {};
}

struct EditAnnotationsWindow : WindowBase {
    WindowTab* tab = nullptr;
    ILayout* mainLayout = nullptr;

    VirtListBox* listBox = nullptr;
    VirtText* staticRect = nullptr;
    VirtText* staticAuthor = nullptr;
    VirtText* staticModificationDate = nullptr;
    VirtText* staticPopup = nullptr;
    VirtText* staticContents = nullptr;
    Edit* editContents = nullptr;
    VirtText* staticTextAlignment = nullptr;
    DropDown* dropDownTextAlignment = nullptr;
    VirtText* staticTextFont = nullptr;
    DropDown* dropDownTextFont = nullptr;
    VirtText* staticTextSize = nullptr;
    Trackbar* trackbarTextSize = nullptr;
    VirtText* staticTextColor = nullptr;
    DropDown* dropDownTextColor = nullptr;

    VirtText* staticLineStart = nullptr;
    DropDown* dropDownLineStart = nullptr;
    VirtText* staticLineEnd = nullptr;
    DropDown* dropDownLineEnd = nullptr;

    VirtText* staticIcon = nullptr;
    DropDown* dropDownIcon = nullptr;

    VirtText* staticBorder = nullptr;
    Trackbar* trackbarBorder = nullptr;

    VirtText* staticColor = nullptr;
    DropDown* dropDownColor = nullptr;
    VirtText* staticInteriorColor = nullptr;
    DropDown* dropDownInteriorColor = nullptr;

    VirtText* staticOpacity = nullptr;
    Trackbar* trackbarOpacity = nullptr;

    VirtButton* buttonSaveAttachment = nullptr;
    VirtButton* buttonEmbedAttachment = nullptr;

    VirtButton* buttonDelete = nullptr;

    VirtButton* buttonSaveToCurrentFile = nullptr;
    VirtButton* buttonSaveToNewFile = nullptr;

    // those are
    Vec<Annotation*> annotations;

    bool skipGoToPage = false;
    // True while DoContents/etc. programmatically fill the edit; ignore EN_CHANGE.
    bool updatingControls = false;

    str::Builder currTextColor;
    str::Builder currCustomColor;
    str::Builder currCustomInteriorColor;

    void OnSize(WindowBase::SizeEvent* ev);
    void OnFocus(WindowBase::FocusEvent* ev);
    void OnKeyDown(KeyEvent* ev);

    void ListBoxSelectionChanged();

    ~EditAnnotationsWindow() override;
};

#if 0
static Annotation* PickNewSelectedAnnotation(EditAnnotationsWindow* ew, int prevIdx) {
    int nAnnots = ew->annotations.Size();
    if (nAnnots == 0) {
        return nullptr;
    }
    if (prevIdx >= nAnnots) {
        prevIdx = nAnnots - 1;
    }
    return ew->annotations[prevIdx];
}
#endif

// when deleting the selected annotation, pick another annotation on the same
// page to select next. restricting to the same page avoids jumping the view
// (the reason the broader auto-select experiments below stayed disabled).
static Annotation* FindAnnotationOnSamePage(WindowTab* tab, Annotation* annot) {
    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return nullptr;
    }
    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return nullptr;
    }
    int pageNo = annot->pageNo;
    Vec<Annotation*> annots;
    EngineGetAnnotations(engine, annots);
    for (Annotation* a : annots) {
        if (a != annot && a->pageNo == pageNo) {
            return a;
        }
    }
    return nullptr;
}

static void RebuildAnnotationsListBox(EditAnnotationsWindow* ew);

// Drop non-owning Annotation* held by UI (selection, drag, hover, form edit).
// Call before DeleteAnnotation frees the wrapper, or when the engine is about
// to die and raw Annotation* must not be used again.
void DetachAnnotationFromUI(Annotation* annot) {
    if (!annot) {
        return;
    }
    CancelFormFieldEditIfWidget(annot);
    for (MainWindow* win : gWindows) {
        if (win->annotationBeingDragged == annot) {
            win->annotationBeingDragged = nullptr;
            win->annotationBeingResized = false;
        }
        if (win->annotationUnderCursor == annot) {
            win->annotationUnderCursor = nullptr;
        }
        int nTabs = win->TabCount();
        for (int i = 0; i < nTabs; i++) {
            WindowTab* t = win->GetTab(i);
            if (t && t->selectedAnnotation == annot) {
                t->selectedAnnotation = nullptr;
            }
        }
    }
}

// Clear the edit-annotations list (and listbox) before the engine is destroyed
// so ReloadDocument cannot leave dangling Annotation* in the open panel.
void InvalidateEditAnnotationsOnEngineChange(WindowTab* tab) {
    if (!tab || !tab->editAnnotsWindow) {
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    // selectedAnnotation is usually already null; do not FlushContents into a
    // dying engine. Drop non-owning list entries before ~EngineMupdf frees them.
    ew->annotations.Clear();
    RebuildAnnotationsListBox(ew);
}

void DeleteAnnotationAndUpdateUI(WindowTab* tab, Annotation* annot) {
    if (!annot) {
        // the listbox selection can be stale (see DeleteSelectedAnnotation), and
        // FindAnnotationOnSamePage() below dereferences annot
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    Annotation* selectNext = nullptr;
    if (annot != tab->selectedAnnotation) {
        // preserve current selection if we're not deleting it
        selectNext = tab->selectedAnnotation;
    } else {
        // deleting the selected annotation: select another one on the same page
        selectNext = FindAnnotationOnSamePage(tab, annot);
    }

    // Clear all UI holders before DeleteAnnotation frees the wrapper.
    DetachAnnotationFromUI(annot);
    DeleteAnnotation(annot);
    if (ew != nullptr) {
        // can be null if called from Menu.cpp and annotations window is not visible
        UpdateAnnotationsList(ew);
    }
    SetSelectedAnnotation(tab, selectNext);
}

static void DeleteSelectedAnnotation(EditAnnotationsWindow* ew) {
    int idx = ew->listBox->GetCurrentSelection();
    if (idx < 0) {
        // can get out of sync e.g. after UpdateAnnotationsList during save/reload
        ew->tab->selectedAnnotation = nullptr;
        return;
    }
    Annotation* annot = ew->annotations[idx];
    if (ew->tab->selectedAnnotation != annot) {
        // can get out of sync if e.g. keyboard navigation in listbox
        // hasn't triggered ListBoxSelectionChanged yet
        ew->tab->selectedAnnotation = annot;
    }
    DeleteAnnotationAndUpdateUI(ew->tab, annot);

    // Note: auto-selecting next annotation might cause page jumping
#if 0
    annot = PickNewSelectedAnnotation(this, idx);
    skipGoToPage = false;
    if (annot) {
        SetSelectedAnnotation(tab, annot);
    }
#endif
}

static NO_INLINE EngineMupdf* GetEngineMupdf(EditAnnotationsWindow* ew) {
    // Seen null in crash reports (window/tab closed while edit UI still fires)
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

static int FindStringInArray(SeqStrings items, Str toFind, int valIfNotFound = -1) {
    int idx = SeqStrIndex(items, toFind);
    if (idx < 0) {
        idx = valIfNotFound;
    }
    return idx;
}

static bool IsAnnotationTypeInArray(AnnotationType* arr, int arrSize, AnnotationType toFind) {
    for (int i = 0; i < arrSize; i++) {
        if (toFind == arr[i]) {
            return true;
        }
    }
    return false;
}

// return true if closed the window, false if there was no window to close
static void FlushContentsFromEdit(EditAnnotationsWindow* ew);

bool CloseAndDeleteEditAnnotationsWindow(WindowTab* tab) {
    if (!tab->editAnnotsWindow) {
        return false;
    }
    auto* ew = tab->editAnnotsWindow;
    FlushContentsFromEdit(ew);
    tab->editAnnotsWindow = nullptr;
    // this will trigger closing the window
    delete ew;
    return true;
}

EditAnnotationsWindow::~EditAnnotationsWindow() {
    // hacky: we want the position of the main window
    // but the size of client area
    tab->lastEditAnnotsWindowPos = HwndWindowRect(hwnd);
    auto cr = HwndClientRect(hwnd);
    tab->lastEditAnnotsWindowPos.dx = cr.dx;
    tab->lastEditAnnotsWindowPos.dy = cr.dy;

    if (tab->selectedAnnotation != nullptr) {
        tab->selectedAnnotation = nullptr;
        // tab->win can be null (SafeDeleteEditAnnotationsWindow checks it too)
        if (tab->win && !tab->win->isBeingClosed) {
            MainWindowRerender(tab->win);
            ToolbarUpdateStateForWindow(tab->win, false);
        }
    }
    // ~WindowBase deletes `layout`, which is the same tree as mainLayout
}

static bool DidAnnotationsChange(EditAnnotationsWindow* ew) {
    EngineMupdf* engine = GetEngineMupdf(ew);
    if (!engine) { // maybe seen in crash report
        ReportIf(true);
        return false;
    }
    return EngineMupdfHasUnsavedAnnotations(engine);
}

// Include the document basename on the "save to existing" button so it is
// obvious which file will be overwritten.
static void UpdateSaveButtonLabels(EditAnnotationsWindow* ew) {
    if (!ew || !ew->buttonSaveToCurrentFile) {
        return;
    }
    TempStr base = {};
    if (ew->tab) {
        base = path::GetBaseNameTemp(ew->tab->filePath);
    }
    if (len(base) > 0) {
        ew->buttonSaveToCurrentFile->SetText(fmt(_TRA("Save changes to %s").s, base));
    } else {
        ew->buttonSaveToCurrentFile->SetText(_TRA("Save changes to existing PDF"));
    }
}

static void EnableSaveIfAnnotationsChanged(EditAnnotationsWindow* ew) {
    if (!ew || !ew->buttonSaveToCurrentFile || !ew->buttonSaveToNewFile) {
        return;
    }
    bool didChange = DidAnnotationsChange(ew);
    ew->buttonSaveToCurrentFile->SetIsEnabled(didChange);
    ew->buttonSaveToNewFile->SetIsEnabled(didChange);
}

void NotifyAnnotationsChanged(EditAnnotationsWindow* ew) {
    if (!ew) {
        return;
    }
    EnableSaveIfAnnotationsChanged(ew);
}

static void RebuildAnnotationsListBox(EditAnnotationsWindow* ew) {
    auto* model = new ListBoxModelStrings();
    int n = 0;
    n = len(ew->annotations);

    str::Builder s;
    for (int i = 0; i < n; i++) {
        auto* annot = ew->annotations[i];
        s.Reset();
        s.Append(fmt(_TRA("page %d,").s, annot->pageNo));
        Str name = AnnotationReadableNameTemp(annot->type);
        s.Append(fmt(" %s", name));
        model->strings.Append(ToStr(s));
    }

    int prevScrollY = ew->listBox->scrollY;
    ew->listBox->SetModel(model); // resets the scroll position
    ew->listBox->ScrollTo(prevScrollY);
    EnableSaveIfAnnotationsChanged(ew);
}

// Delete off the stack of WM_CLOSE / WM_DESTROY (same pattern as
// AdvancedSettingsDialog / CommandPalette). Destroying the window while
// handling its own message is unsafe; uitask runs after the current
// dispatch finishes.
static void SafeDeleteEditAnnotationsWindow(EditAnnotationsWindow* w) {
    if (!w) {
        return;
    }
    HWND toActivate = nullptr;
    if (w->tab && w->tab->win) {
        toActivate = w->tab->win->hwndFrame;
    }
    if (w->tab && w->tab->editAnnotsWindow == w) {
        w->tab->editAnnotsWindow = nullptr;
    }
    delete w;
    if (toActivate) {
        SetActiveWindow(toActivate);
    }
}

static void ScheduleDeleteEditAnnotationsWindow(EditAnnotationsWindow* w) {
    if (!w) {
        return;
    }
    // Clear the tab pointer immediately so re-open and other paths do not
    // touch a window that is about to be deleted.
    if (w->tab && w->tab->editAnnotsWindow == w) {
        w->tab->editAnnotsWindow = nullptr;
    }
    auto fn = MkFunc0(SafeDeleteEditAnnotationsWindow, w);
    uitask::Post(fn, "SafeDeleteEditAnnotationsWindow");
}

// CloseEvent::didHandle defaults to true, so the framework skips its default
// Destroy(); we own teardown via the scheduled delete (which runs ~WindowBase and
// DestroyWindow).
static void OnClose(WindowBase::CloseEvent* ev) {
    auto* w = (EditAnnotationsWindow*)ev->e->self;
    FlushContentsFromEdit(w);
    ScheduleDeleteEditAnnotationsWindow(w);
}

// CloseAndDeleteEditAnnotationsWindow already nulls the tab pointer and
// deletes; only schedule if this is an unexpected destroy with the pointer
// still set.
static void OnDestroy(WindowBase::DestroyEvent* ev) {
    auto* w = (EditAnnotationsWindow*)ev->e->self;
    if (w->tab && w->tab->editAnnotsWindow == w) {
        ScheduleDeleteEditAnnotationsWindow(w);
    }
}

void EditAnnotationsWindow::OnFocus(WindowBase::FocusEvent*) {
    SelectTabInWindow(tab);
}

extern bool SaveAnnotationsToMaybeNewPdfFile(WindowTab*);

static void ButtonSaveToNewFileHandler(EditAnnotationsWindow* ew) {
    FlushContentsFromEdit(ew);
    WindowTab* tab = ew->tab;
    // On success SaveAnnotationsToMaybeNewPdfFile closes this window (and may
    // open a new one). Do not touch ew after a successful return.
    bool ok = SaveAnnotationsToMaybeNewPdfFile(tab);
    if (!ok) {
        return;
    }
    // New window (if any) is created inside Save*; labels/enabled state are
    // set when it opens. Old ew is already deleted.
}

extern bool SaveAnnotationsToExistingFile(WindowTab* tab);

static void ButtonSaveToCurrentPDFHandler(EditAnnotationsWindow* ew) {
    FlushContentsFromEdit(ew);
    // SaveAnnotationsToExistingFile closes this window and reloads the PDF
    // (engine/Annotation* become invalid). Do not touch ew after this call.
    SaveAnnotationsToExistingFile(ew->tab);
}

void EditAnnotationsWindow::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_TAB) {
        // Tab / Shift+Tab. The ring is the layout order and skips what is hidden,
        // HWND controls and virtual ones alike
        TabNavigate(!ev->isShift);
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_DELETE) {
        // When focus is in a text field, let the Edit control handle Delete /
        // Ctrl+Delete (word delete). Only delete the annotation when focus is
        // outside an edit control (issue #5815).
        HWND focused = ::GetFocus();
        TempStr cls = HwndGetClassName(focused);
        if (str::EqI(cls, StrL("Edit"))) {
            return;
        }
        // Ctrl+Delete (and plain Delete) remove the selected annotation
        DeleteSelectedAnnotation(this);
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == 'S' && ev->isShift && ev->isCtrl) {
        // TODO: delay by posting a message?
        // TODO: the keybinding could be changed so this should
        // be more sophisticated and match the shortcut
        ButtonSaveToCurrentPDFHandler(this);
        ev->didHandle = true;
    }
}

static void ItemsFromSeqstrings(StrVec& items, SeqStrings strings) {
    for (int off = 0; SeqStrAt(strings, off);) {
        items.Append(SeqStrAt(strings, off));
        if (!SeqStrAdvance(strings, off)) {
            break;
        }
    }
}

static void DropDownFillColors(DropDown* w, PdfColor col, str::Builder& customColor) {
    StrVec items;
    ItemsFromSeqstrings(items, gColors);
    TempStr colorName = GetKnownColorNameTemp(col);
    int idx = SeqStrIndex(gColors, colorName);
    if (idx < 0) {
        customColor.Reset();
        SerializePdfColor(col, customColor);
        items.Append(ToStr(customColor));
        idx = len(items) - 1;
    }
    w->SetItems(items);
    w->SetCurrentSelection(idx);
}

static PdfColor GetDropDownColor(Str sv) {
    int idx = SeqStrIndex(gColors, sv);
    if (idx >= 0) {
        int nMaxColors = dimofi(gColorsValues);
        ReportIf(idx >= nMaxColors);
        if (idx < nMaxColors) {
            return gColorsValues[idx];
        }
        return 0;
    }
    ParsedColor col;
    ParseColor(col, sv);
    return col.pdfCol;
}

static bool gShowRect = true;

// TODO: only limit to widgets that have rect?
static void DoRect(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!gShowRect) {
        return;
    }
    str::Builder s;
    RectF rect = GetBounds(annot);
    int x = (int)rect.x;
    int y = (int)rect.y;
    int dx = (int)rect.dx;
    int dy = (int)rect.dy;
    s.Append(fmt(_TRA("Rect: x=%d y=%d dx=%d dy=%d").s, x, y, dx, dy));
    ew->staticRect->SetText(ToStr(s));
    ew->staticRect->SetIsVisible(true);
}

static void DoAuthor(EditAnnotationsWindow* ew, Annotation* annot) {
    Str author = Author(annot);
    bool isVisible = len(author) > 0;
    if (!isVisible) {
        return;
    }
    str::Builder s;
    s.Append(fmt(_TRA("Author: %s").s, author));
    ew->staticAuthor->SetText(ToStr(s));
    ew->staticAuthor->SetIsVisible(true);
}

static void AppendPdfDate(str::Builder& s, time_t secs) {
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
    str::Builder s;
    s.Append(_TRA("Date:"));
    s.Append(" "); // apptranslator doesn't handle spaces at the end of translated string
    AppendPdfDate(s, ModificationDate(annot));
    ew->staticModificationDate->SetText(ToStr(s));
    ew->staticModificationDate->SetIsVisible(true);
}

static void DoPopup(EditAnnotationsWindow* ew, Annotation* annot) {
    int popupId = PopupId(annot);
    if (popupId < 0) {
        return;
    }
    str::Builder s;
    s.Append(fmt(_TRA("Popup: %d 0 R").s, popupId));
    ew->staticPopup->SetText(ToStr(s));
    ew->staticPopup->SetIsVisible(true);
}

// Push the contents edit into the selected annotation. Called on switch/save/
// close so unsaved last edits stick (plus df1b2aab8).
static void FlushContentsFromEdit(EditAnnotationsWindow* ew) {
    if (!ew || !ew->editContents || !ew->tab || ew->updatingControls) {
        return;
    }
    Annotation* a = ew->tab->selectedAnnotation;
    if (!AnnotationIsLive(a) || ew->annotations.Find(a) < 0) {
        return;
    }
    auto txt = ew->editContents->GetTextTemp();
    txt = str::ReplaceTemp(txt, StrL("\r\n"), StrL("\n"));
    SetContents(a, txt);
    EnableSaveIfAnnotationsChanged(ew);
}

static void DoContents(EditAnnotationsWindow* ew, Annotation* annot) {
    Str s = Contents(annot);
    // don't replace if already is "\r\n"
    s = str::ReplaceTemp(s, StrL("\r\n"), StrL("\n"));
    s = str::ReplaceTemp(s, StrL("\n"), StrL("\r\n"));
    ew->staticContents->SetIsVisible(true);
    ew->editContents->SetIsVisible(true);
    ew->updatingControls = true;
    ew->editContents->SetText(s);
    ew->updatingControls = false;
}

static void DoTextAlignment(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    int itemNo = Quadding(annot);
    SeqStrings items = gQuaddingNames;
    ew->dropDownTextAlignment->SetItemsSeqStrings(items);
    ew->dropDownTextAlignment->SetCurrentSelection(itemNo);
    ew->staticTextAlignment->SetIsVisible(true);
    ew->dropDownTextAlignment->SetIsVisible(true);
}

static void TextAlignmentSelectionChanged(EditAnnotationsWindow* ew) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownTextAlignment->GetCurrentSelection();
    int newQuadding = idx;
    SetQuadding(annot, newQuadding);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextFont(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    Str fontName = DefaultAppearanceTextFont(annot);
    // TODO: might have other fonts, like "Symb" and "ZaDb"
    auto itemNo = SeqStrIndex(gFontNames, fontName);
    if (itemNo < 0) {
        return;
    }
    ew->dropDownTextFont->SetItemsSeqStrings(gFontReadableNames);
    ew->dropDownTextFont->SetCurrentSelection(itemNo);
    ew->staticTextFont->SetIsVisible(true);
    ew->dropDownTextFont->SetIsVisible(true);
}

static void TextFontSelectionChanged(EditAnnotationsWindow* ew) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownTextFont->GetCurrentSelection();
    Str font = SeqStrByIndex(gFontNames, idx);
    SetDefaultAppearanceTextFont(annot, font);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextSize(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    int fontSize = DefaultAppearanceTextSize(annot);
    TempStr s = fmt(_TRA("Text Size: %d").s, fontSize);
    ew->staticTextSize->SetText(s);
    // TODO: DoTextSize() shouldn't modify the annotation but I'm not sure
    // if it's not needed to be called for free text annotations
    // at some point (i.e. when creating)
    // SetDefaultAppearanceTextSize(ew->tab->selectedAnnotation, fontSize);
    ew->trackbarTextSize->SetValue(fontSize);
    ew->staticTextSize->SetIsVisible(true);
    ew->trackbarTextSize->SetIsVisible(true);
}

static void TextFontSizeChanging(EditAnnotationsWindow* ew, Trackbar::PositionChangingEvent* ev) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    int fontSize = ev->pos;
    SetDefaultAppearanceTextSize(annot, fontSize);
    TempStr s = fmt(_TRA("Text Size: %d").s, fontSize);
    ew->staticTextSize->SetText(s);
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
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownTextColor->GetCurrentSelection();
    Str item = ew->dropDownTextColor->items[idx];
    auto col = GetDropDownColor(item);
    SetDefaultAppearanceTextColor(annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoBorder(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!AnnotationSupportsBorder(annot->type)) {
        return;
    }
    int borderWidth = BorderWidth(annot);
    borderWidth = setMinMax(borderWidth, borderWidthMin, borderWidthMax);
    TempStr s = fmt(_TRA("Border: %d").s, borderWidth);
    ew->staticBorder->SetText(s);
    ew->trackbarBorder->SetValue(borderWidth);
    ew->staticBorder->SetIsVisible(true);
    ew->trackbarBorder->SetIsVisible(true);
}

static void BorderWidthChanging(EditAnnotationsWindow* ew, Trackbar::PositionChangingEvent* ev) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    int borderWidth = ev->pos;
    SetBorderWidth(annot, borderWidth);
    TempStr s = fmt(_TRA("Border: %d").s, borderWidth);
    ew->staticBorder->SetText(s);
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
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto start = ew->dropDownLineStart->GetCurrentSelection();
    if (start < 0) {
        return;
    }
    SetLineStartStyles(annot, start);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void LineEndSelectionChanged(EditAnnotationsWindow* ew) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto end = ew->dropDownLineEnd->GetCurrentSelection();
    if (end < 0) {
        return;
    }
    SetLineEndStyles(annot, end);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoIcon(EditAnnotationsWindow* ew, Annotation* annot) {
    Str itemName = IconName(annot);
    SeqStrings items = nullptr;
    switch (Type(annot)) {
        case AnnotationType::Text:
            items = AnnotationTextIcons();
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
        default:
            // no-op
            break;
    }
    if (!items || len(itemName) == 0) {
        return;
    }
    ew->dropDownIcon->SetItemsSeqStrings(items);
    int idx = FindStringInArray(items, itemName, 0);
    ew->dropDownIcon->SetCurrentSelection(idx);
    ew->staticIcon->SetIsVisible(true);
    ew->dropDownIcon->SetIsVisible(true);
}

static void IconSelectionChanged(EditAnnotationsWindow* ew) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownIcon->GetCurrentSelection();
    auto item = ew->dropDownIcon->items[idx];
    SetIconName(annot, item);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoColor(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!AnnotationSupportsColor(annot->type)) {
        return;
    }
    PdfColor col = GetColor(annot);
    DropDownFillColors(ew->dropDownColor, col, ew->currCustomColor);
    int n = dimof(gAnnotsIsColorBackground);
    bool isBgCol = IsAnnotationTypeInArray(gAnnotsIsColorBackground, n, Type(annot));
    if (isBgCol) {
        ew->staticColor->SetText(_TRA("Background Color:"));
    } else {
        ew->staticColor->SetText(_TRA("Color:"));
    }
    ew->staticColor->SetIsVisible(true);
    ew->dropDownColor->SetIsVisible(true);
}

static void ColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownColor->GetCurrentSelection();
    auto item = ew->dropDownColor->items[idx];
    auto col = GetDropDownColor(item);
    SetColor(annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoInteriorColor(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!AnnotationSupportsInteriorColor(annot->type)) {
        return;
    }
    PdfColor col = InteriorColor(annot);
    DropDownFillColors(ew->dropDownInteriorColor, col, ew->currCustomInteriorColor);
    ew->staticInteriorColor->SetIsVisible(true);
    ew->dropDownInteriorColor->SetIsVisible(true);
}

static void InteriorColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownInteriorColor->GetCurrentSelection();
    auto item = ew->dropDownInteriorColor->items[idx];
    auto col = GetDropDownColor(item);
    SetInteriorColor(annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoOpacity(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::Highlight) {
        return;
    }
    int opacity = Opacity(ew->tab->selectedAnnotation);
    TempStr s = fmt(_TRA("Opacity: %d").s, opacity);
    ew->staticOpacity->SetText(s);
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

static void OpacityChanging(EditAnnotationsWindow* ew, Trackbar::PositionChangingEvent* ev) {
    auto* annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    int opacity = ev->pos;
    SetOpacity(annot, opacity);
    TempStr s = fmt(_TRA("Opacity: %d").s, opacity);
    ew->staticOpacity->SetText(s);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

// TODO: maybe use ew->tab->selectedAnnotation instead of annot
static void UpdateUIForSelectedAnnotation(EditAnnotationsWindow* ew, Annotation* annot, bool isNew = false,
                                          EditAnnotFocus focus = EditAnnotFocus::Default) {
    HidePerAnnotControls(ew);
    if (annot) {
        int itemNo = ew->annotations.Find(annot);
        if (itemNo < 0) {
            // can happen if annotations list is out of sync (e.g. after reload)
            return;
        }

        DoRect(ew, annot);
        DoAuthor(ew, annot);
        DoModificationDate(ew, annot);
        DoPopup(ew, annot);
        DoContents(ew, annot);

        DoTextAlignment(ew, annot);
        DoTextFont(ew, annot);
        DoTextSize(ew, annot);
        DoTextColor(ew, annot);

        DoLineStartEnd(ew, annot);

        DoIcon(ew, annot);

        DoBorder(ew, annot);
        DoColor(ew, annot);
        DoInteriorColor(ew, annot);

        DoOpacity(ew, annot);
        DoSaveEmbed(ew, annot);

        ew->listBox->SetCurrentSelection(itemNo);
        ew->buttonDelete->SetIsVisible(true);

        // NOLINTNEXTLINE(bugprone-branch-clone): branch order matters, an explicit focus wins over isNew
        if (focus == EditAnnotFocus::Edit) {
            HwndSetFocus(ew->editContents->hwnd);
            ew->editContents->SelectAll();
        } else if (focus == EditAnnotFocus::List) { // NOLINT(bugprone-branch-clone)
            ew->SetFocusTo(ew->listBox);
        } else if (isNew && annot->type == AnnotationType::FreeText) {
            HwndSetFocus(ew->editContents->hwnd);
            // ew->editContents->SetCursorPositionAtEnd();
            ew->editContents->SelectAll();
        } else {
            ew->SetFocusTo(ew->listBox);
        }
    }

    // Prefer the live client size so re-layout matches the window after resize;
    // fall back to last layout bounds if the window is not sized yet.
    Rect client = HwndClientRect(ew->hwnd);
    int dx = client.dx;
    int dy = client.dy;
    if (dx <= 0 || dy <= 0) {
        auto currBounds = ew->mainLayout->lastBounds;
        dx = currBounds.dx;
        dy = currBounds.dy;
    }
    LayoutAndSizeToContent(ew->mainLayout, dx, dy, ew->hwnd);
    // pick up the virtual controls so we paint them and they get their input
    ew->DoLayout(HwndClientRect(ew->hwnd).Size());

    if (!annot) {
        return;
    }
    // skipGoToPage: set when the edit window was opened on an annot that is
    // already under the user's view. isNew: creating an annotation implies the
    // page was already visible (cursor / selection / placement), so don't scroll.
    if (ew->skipGoToPage || isNew) {
        ew->skipGoToPage = false;
        return;
    }

    int annotPageNo = annot->pageNo;
    DisplayModel* dm = ew->tab->AsFixed();
    int nPages = dm->PageCount();
    if (annotPageNo > nPages) {
        // see https://github.com/sumatrapdfreader/sumatrapdf/issues/1701
        logf("UpdateUIForSelectedAnnotation: invalid annotPageNo (%d), should be <= than nPages (%d)\n", annotPageNo,
             nPages);
        ReportIf(annotPageNo > nPages);
        return;
    }

    // don't switch pages if already visible. needed for cases where
    // we show more than one page at a time and GoToPage() scrolls
    // to top page
    if (!dm->PageVisible(annotPageNo)) {
        dm->GoToPage(annotPageNo, true);
    }
}

static void ButtonSaveAttachment(EditAnnotationsWindow* ew) {
    Annotation* annot = ew->tab->selectedAnnotation;
    ReportIf(!annot);
    if (!annot || annot->type != AnnotationType::FileAttachment) {
        return;
    }
    EngineMupdf* engine = GetEngineMupdf(ew);
    if (!engine) {
        return;
    }
    fz_context* ctx = engine->Ctx();
    pdf_annot* pdfannot = annot->pdfannot;
    if (!pdfannot) {
        return;
    }

    int objNum = pdf_to_num(ctx, pdf_annot_obj(ctx, pdfannot));
    Str data = EngineMupdfLoadAnnotAttachment((EngineBase*)engine, objNum);
    if (len(data) == 0) {
        return;
    }

    Str fileName;
    pdf_obj* fs = pdf_annot_filespec(ctx, pdfannot);
    if (fs) {
        pdf_filespec_params fileParams = {};
        pdf_get_filespec_params(ctx, fs, &fileParams);
        fileName = str::DupTemp(Str(fileParams.filename));
    }
    if (!fileName) {
        fileName = StrL("attachment");
    }

    TempStr dir = path::GetDirTemp(ew->tab->filePath);
    TempStr baseName = path::GetBaseNameTemp(fileName);
    TempStr dstPath = path::JoinTemp(dir, baseName);
    SaveDataToFile(ew->hwnd, dstPath, data);
    str::Free(data);
}

// Pick a file and embed its contents into the selected FileAttachment annot.
static void ButtonEmbedAttachment(EditAnnotationsWindow* ew) {
    Annotation* annot = ew->tab ? ew->tab->selectedAnnotation : nullptr;
    ReportIf(!annot);
    if (!annot || annot->type != AnnotationType::FileAttachment) {
        return;
    }
    if (!CanAccessDisk()) {
        return;
    }
    EngineMupdf* engine = GetEngineMupdf(ew);
    if (!engine || !engine->pdfdoc || !AnnotationIsLive(annot)) {
        return;
    }

    WCHAR pathW[MAX_PATH + 1]{};
    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ew->hwnd;
    ofn.lpstrFile = pathW;
    ofn.nMaxFile = dimof(pathW);
    TempStr fileFilterA = fmt("%s\1*.*\1", _TRA("All files"));
    TempWStr fileFilter = ToWStrTemp(fileFilterA);
    wstr::TransCharsInPlace(fileFilter, WStrL(L"\1"), WStrL(L"\0"));
    ofn.lpstrFilter = fileFilter.s;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&ofn)) {
        return;
    }

    // Dialog can pump messages (reload/close); re-check after return.
    if (!AnnotationIsLive(annot)) {
        return;
    }

    TempStr path = ToUtf8Temp(pathW);
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        TempStr msg = fmt(_TRA("Failed to read '%s'").s, path::GetBaseNameTemp(path));
        MsgBox(ew->hwnd, msg, _TRA("Error"), MB_OK | MB_ICONERROR);
        return;
    }

    TempStr baseName = path::GetBaseNameTemp(path);
    TempStr baseNameZ = str::DupTemp(baseName);
    fz_context* ctx = engine->Ctx();
    bool ok = false;
    {
        ScopedRecursiveMutex cs(&engine->docLock);
        pdf_obj* fs = nullptr;
        fz_buffer* contents = nullptr;
        fz_var(fs);
        fz_var(contents);
        fz_try(ctx) {
            contents = fz_new_buffer_from_copied_data(ctx, (const u8*)data.s, (size_t)data.len);
            // created/modified unknown (-1); no checksum (matches mupdf gl-annotate)
            fs = pdf_add_embedded_file(ctx, engine->pdfdoc, baseNameZ.s, nullptr, contents, -1, -1, 0);
            pdf_set_annot_filespec(ctx, annot->pdfannot, fs);
            pdf_update_annot(ctx, annot->pdfannot);
            ok = true;
        }
        fz_always(ctx) {
            pdf_drop_obj(ctx, fs);
            fz_drop_buffer(ctx, contents);
        }
        fz_catch(ctx) {
            fz_report_error(ctx);
            ok = false;
        }
    }
    str::Free(data);

    if (!ok) {
        MsgBox(ew->hwnd, _TRA("Failed to embed file"), _TRA("Error"), MB_OK | MB_ICONERROR);
        return;
    }
    MarkNotificationAsModified(engine, annot);
    EnableSaveIfAnnotationsChanged(ew);
}

void SetSelectedAnnotation(WindowTab* tab, Annotation* annot, bool isNew, EditAnnotFocus focus) {
    // when we delete an annotation we automatically pick one to
    // set as selected and it might end up as currently selected
    // we still want to redraw to not show deleted annotation
    // but not do the rest of the logic as it triggers infinite loop
    // TODO: maybe if we already have selected annotation, do not auto-pick
    MainWindow* win = tab->win;
    auto* ew = tab->editAnnotsWindow;
    if (annot == tab->selectedAnnotation) {
        MainWindowRerender(win);
        if (ew) {
            UpdateUIForSelectedAnnotation(ew, annot, isNew, focus);
        }
        ToolbarUpdateStateForWindow(win, false);
        return;
    }
    // Commit contents of the previous selection before switching away.
    if (ew) {
        FlushContentsFromEdit(ew);
    }
    tab->selectedAnnotation = annot;
    tab->didScrollToSelectedAnnotation = false;
    // go to page with a given annotations before triggering repaint
    if (ew) {
        UpdateUIForSelectedAnnotation(ew, annot, isNew, focus);
        HwndShowWithoutActivate(ew->hwnd);
    }
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, false);
}

void UpdateAnnotationsList(EditAnnotationsWindow* ew) {
    if (!ew) {
        return;
    }
    auto* engine = GetEngineMupdf(ew);
    EngineMupdfGetAnnotations(engine, ew->annotations);
    RebuildAnnotationsListBox(ew);
}

static void ButtonDeleteHandler(EditAnnotationsWindow* ew) {
    ReportIf(!ew->tab->selectedAnnotation);
    DeleteSelectedAnnotation(ew);
}

static void ListBoxSelectionChanged(EditAnnotationsWindow* ew) {
    ew->ListBoxSelectionChanged();
}

void EditAnnotationsWindow::ListBoxSelectionChanged() {
    int itemNo = listBox->GetCurrentSelection();
    if (itemNo < 0) {
        // an item has been deselected because e.g. selected annotation was deleted
        return;
    }
    if (!annotations.isValidIndex(itemNo)) {
        logfa("EditAnnotationsWindow::ListBoxSelectionChanged: invalid itemNo=%d, len(annotations)=%d\n", itemNo,
              len(annotations));
        ReportDebugIf(true);
        return;
    }
    Annotation* annot = annotations[itemNo];
    SetSelectedAnnotation(tab, annot);
}

static UINT_PTR gMainWindowRerenderTimer = 0;
static MainWindow* gMainWindowForRender = nullptr;

// Called from the contents edit onTextChanged (EN_CHANGE / EN_KILLFOCUS).
// Can fire after the selected annotation was deleted/deselected, or after
// the window/tab went away — never assume selectedAnnotation is non-null.
static void ContentsChanged(EditAnnotationsWindow* ew) {
    if (!ew || !ew->tab || !ew->editContents || ew->updatingControls) {
        return;
    }
    Annotation* a = ew->tab->selectedAnnotation;
    if (!AnnotationIsLive(a)) {
        return;
    }
    auto txt = ew->editContents->GetTextTemp();
    txt = str::ReplaceTemp(txt, StrL("\r\n"), StrL("\n"));
    // SetContents returns false when the text is unchanged; skip save-enable
    // and re-render debounce in that case.
    if (!SetContents(a, txt)) {
        return;
    }
    EnableSaveIfAnnotationsChanged(ew);

    MainWindow* win = ew->tab->win;
    if (!win || !win->hwndCanvas) {
        return;
    }
    if (gMainWindowRerenderTimer != 0) {
        KillTimer(win->hwndCanvas, gMainWindowRerenderTimer);
        gMainWindowRerenderTimer = 0;
    }
    UINT timeoutInMs = 1000;
    gMainWindowForRender = win;
    gMainWindowRerenderTimer = SetTimer(win->hwndCanvas, 1, timeoutInMs, [](HWND, UINT, UINT_PTR, DWORD) {
        if (IsMainWindowValid(gMainWindowForRender)) {
            MainWindowRerender(gMainWindowForRender);
        }
        gMainWindowRerenderTimer = 0;
    });
}

// how much taller a multi-line edit gets for one more line of text
static int EditLineDy(Edit* e) {
    int prev = e->idealSizeLines;
    e->idealSizeLines = 1;
    int dy1 = e->GetIdealSize().dy;
    e->idealSizeLines = 2;
    int dy2 = e->GetIdealSize().dy;
    e->idealSizeLines = prev;
    return std::max(dy2 - dy1, 1);
}

// Hand the vertical space the rest of the window doesn't need to the two
// controls that can use it: the list of annotations and, when an annotation is
// selected, its Contents box. Both used to be a fixed number of lines, so a
// taller window just grew its empty area - the list stayed put (#3769) and a
// multi-line annotation stayed cut off with room to spare below it (#5834).
static void SetGrowingControlsToFit(EditAnnotationsWindow* ew, int targetClientDy) {
    constexpr int kPreferredLines = 5;
    if (!ew->listBox || !ew->mainLayout) {
        return;
    }
    int listLineDy = ew->listBox->GetItemHeight();
    if (listLineDy <= 0 || targetClientDy <= 0) {
        return;
    }
    Edit* contents = ew->editContents;
    bool growContents = contents && contents->IsVisible();

    // how tall everything is with the smallest list and Contents box we'd like
    // to show
    ew->listBox->idealSizeLines = kPreferredLines;
    if (growContents) {
        contents->idealSizeLines = kPreferredLines;
    }
    Size natural = ew->mainLayout->Layout(ExpandInf());
    int extraDy = targetClientDy - natural.dy;

    if (extraDy > 0 && growContents) {
        // share it: the list is for finding an annotation, the Contents box for
        // reading and writing the one you found, and both want the room
        int forContents = extraDy / 2;
        extraDy -= forContents;
        contents->idealSizeLines = kPreferredLines + (forContents / EditLineDy(contents));
    }
    // one line is the floor: better a cramped list than a window taller than
    // the screen
    ew->listBox->idealSizeLines = std::max(kPreferredLines + (extraDy / listLineDy), 1);
}

void EditAnnotationsWindow::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg != WM_SIZE) {
        return;
    }
    if (!mainLayout) {
        return;
    }
    int dx = ev->size.dx;
    int dy = ev->size.dy;
    if (dx == 0 || dy == 0) {
        return;
    }
    HwndInvalidate(hwnd);
    if (false && mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    SetGrowingControlsToFit(this, dy);
    DoLayout({dx, dy});
}

static VirtText* CreateStatic(Str s = nullptr) {
    return NewVirtText({
        .s = s,
        .font = GetAppFont(),
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });
}

static VirtButton* CreateVirtButton(Str text) {
    auto* b = new VirtButton(text, GetAppFont());
    b->textPadding = DpiScaledInsets(5, 12);
    return b;
}

static void CreateMainLayout(EditAnnotationsWindow* ew) {
    HWND parent = ew->hwnd;
    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    PlatformFont* fnt = GetAppFont();

    {
        auto* w = new VirtListBox();
        w->dpi = ew->GetDpi();
        w->font = fnt;
        w->padding = DpiScaledInsets(4, 0);
        w->idealSizeLines = 5;
        auto* lbModel = new ListBoxModelStrings();
        w->SetModel(lbModel);
        w->onSelectionChanged = MkFunc0(ListBoxSelectionChanged, ew);
        ew->listBox = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic();
        ew->staticRect = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic();
        // WindowBaseLayout* l2 = (WindowBaseLayout*)l;
        // l2->SetInsetsPt(20, 0, 0, 0);
        ew->staticAuthor = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic();
        ew->staticModificationDate = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic();
        ew->staticPopup = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Contents:"));
        ew->staticContents = w;
        w->padding = DpiScaledInsets(4, 0, 0, 0);
        vbox->AddChild(w);
    }

    {
        Edit::CreateArgs args;
        args.parent = parent;
        args.isMultiLine = true;
        args.idealSizeLines = 5;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto* w = new Edit();
        HWND hwnd = w->Create(args);
        ReportIf(!hwnd);
        w->maxDx = 150;
        w->onTextChanged = MkFunc0(ContentsChanged, ew);
        // flush Contents on blur (EN_KILLFOCUS); do not fold this into
        // onTextChanged — that must stay EN_CHANGE-only (Advanced Settings UAF)
        w->onKillFocus = MkFunc0(ContentsChanged, ew);
        ew->editContents = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Text Alignment:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticTextAlignment = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = MkFunc0(TextAlignmentSelectionChanged, ew);
        ew->dropDownTextAlignment = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Text Font:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticTextFont = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);

        w->Create(args);
        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = MkFunc0(TextFontSelectionChanged, ew);
        ew->dropDownTextFont = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Text Size:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticTextSize = w;
        vbox->AddChild(w);
    }

    {
        Trackbar::CreateArgs args;
        args.parent = parent;
        args.rangeMin = 8;
        args.rangeMax = 36;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new Trackbar();
        w->SetInsetsPt(4, 0, 0, 0);

        w->Create(args);

        w->onPositionChanging = MkFunc1(TextFontSizeChanging, ew);
        ew->trackbarTextSize = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Text Color:"));
        ew->staticTextColor = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = MkFunc0(TextColorSelectionChanged, ew);
        ew->dropDownTextColor = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Line Start:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticLineStart = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = MkFunc0(LineStartSelectionChanged, ew);
        ew->dropDownLineStart = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Line End:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticLineEnd = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = MkFunc0(LineEndSelectionChanged, ew);
        ew->dropDownLineEnd = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Icon:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticIcon = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = MkFunc0(IconSelectionChanged, ew);
        ew->dropDownIcon = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic("Border:");
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticBorder = w;
        vbox->AddChild(w);
    }

    {
        Trackbar::CreateArgs args;
        args.parent = parent;
        args.rangeMin = borderWidthMin;
        args.rangeMax = borderWidthMax;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new Trackbar();
        w->Create(args);
        w->onPositionChanging = MkFunc1(BorderWidthChanging, ew);
        ew->trackbarBorder = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Color:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticColor = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = MkFunc0(ColorSelectionChanged, ew);
        ew->dropDownColor = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Interior Color:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticInteriorColor = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = MkFunc0(InteriorColorSelectionChanged, ew);
        ew->dropDownInteriorColor = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateStatic(_TRA("Opacity:"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        ew->staticOpacity = w;
        vbox->AddChild(w);
    }

    {
        Trackbar::CreateArgs args;
        args.parent = parent;
        args.rangeMin = 0;
        args.rangeMax = 255;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto* w = new Trackbar();
        w->Create(args);

        w->onPositionChanging = MkFunc1(OpacityChanging, ew);
        ew->trackbarOpacity = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateVirtButton(_TRA("Save..."));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        w->onClick = MkFunc0(ButtonSaveAttachment, ew);
        ew->buttonSaveAttachment = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateVirtButton(_TRA("Embed..."));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        w->onClick = MkFunc0(ButtonEmbedAttachment, ew);
        ew->buttonEmbedAttachment = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateVirtButton(_TRA("Delete Annotation"));
        w->padding = DpiScaledInsets(11, 0, 0, 0);
        w->onClick = MkFunc0(ButtonDeleteHandler, ew);
        ew->buttonDelete = w;
        vbox->AddChild(w);
    }

    {
        // used to take all available space between the what's above and below
        auto* w = new Spacer(0, 0);
        vbox->AddChild(w, 1);
    }

    {
        // text set by UpdateSaveButtonLabels once tab is attached
        auto* w = CreateVirtButton(_TRA("Save changes to existing PDF"));
        w->SetIsEnabled(false); // only enabled if there are changes
        w->onClick = MkFunc0(ButtonSaveToCurrentPDFHandler, ew);
        ew->buttonSaveToCurrentFile = w;
        vbox->AddChild(w);
    }

    {
        auto* w = CreateVirtButton(_TRA("Save changes to a new PDF"));
        w->padding = DpiScaledInsets(8, 0, 0, 0);
        w->SetIsEnabled(false); // only enabled if there are changes
        w->onClick = MkFunc0(ButtonSaveToNewFileHandler, ew);
        ew->buttonSaveToNewFile = w;
        vbox->AddChild(w);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    ew->mainLayout = padding;
    // WindowBase owns and lays out `layout`; mainLayout is the same tree, kept
    // as an ILayout* for its lastBounds
    ew->layout = padding;
    HidePerAnnotControls(ew);
}

static void LimitEditAnnotationsClientSizeToScreen(HWND hwnd, HWND hwndRelative, Size& size) {
    Rect work = GetWorkAreaRect(HwndWindowRect(hwndRelative), hwndRelative);
    WINDOWINFO wi{};
    wi.cbSize = sizeof(wi);
    if (!GetWindowInfo(hwnd, &wi)) {
        size = HwndLimitSizeToScreen(hwndRelative, size);
        return;
    }

    int nonClientDx = RectDx(wi.rcWindow) - RectDx(wi.rcClient);
    int nonClientDy = RectDy(wi.rcWindow) - RectDy(wi.rcClient);
    int maxClientDx = work.dx - nonClientDx;
    int maxClientDy = work.dy - nonClientDy;
    size.dx = std::min(size.dx, maxClientDx);
    size.dy = std::min(size.dy, maxClientDy);
}

void ShowEditAnnotationsWindow(WindowTab* tab, Annotation* annot, EditAnnotFocus focus) {
    if (!tab) return;
    auto* engine = tab->GetEngine();
    auto canAnnotate = EngineSupportsAnnotations(engine);
    if (!canAnnotate) {
        ReportDebugIf(true);
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    if (ew) {
        bool isNew = annot != ew->tab->win->annotationUnderCursor;
        HwndShowWithoutActivate(ew->hwnd);
        SetForegroundWindow(ew->hwnd);
        if (ew->listBox && ew->listBox->model->ItemsCount() > 0) {
            ew->SetFocusTo(ew->listBox);
        }
        if (!annot) return;
        SetSelectedAnnotation(tab, annot, isNew, focus);
        return;
    }
    ew = new EditAnnotationsWindow();
    // OnSize grows the list / Contents box before DoLayout
    ew->autoLayout = false;
    // Esc does not close — user may be editing text (issue #5934)
    ew->closeOnCtrlW = true;
    ew->onClose = MkFunc1Void(OnClose);
    ew->onDestroy = MkFunc1Void(OnDestroy);
    ew->onSize = MkMethod1<EditAnnotationsWindow, WindowBase::SizeEvent*, &EditAnnotationsWindow::OnSize>(ew);
    ew->onFocus = MkMethod1<EditAnnotationsWindow, WindowBase::FocusEvent*, &EditAnnotationsWindow::OnFocus>(ew);
    ew->onKeyDown = MkMethod1<EditAnnotationsWindow, KeyEvent*, &EditAnnotationsWindow::OnKeyDown>(ew);
    CreateCustomArgs args;
    HMODULE h = GetModuleHandleW(nullptr);
    args.icon = LoadIconW(h, MAKEINTRESOURCEW(GetAppIconID()));
    // mainWindow->isDialog = true;
    args.bgColor = DarkModeDialogBgColor();

    args.title = str::JoinTemp(_TRA("Annotations"), StrL(": "), tab->GetTabTitle());
    args.visible = false;
    args.font = GetAppFont();

    // PositionCloseTo(w, args->hwndRelatedTo);
    // Size winSize = {w->initialSize.dx, w->initialSize.Height};
    // winSize = HwndLimitSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.dx, winSize.dy};
    ew->CreateCustom(args);

    CreateMainLayout(ew);
    ew->tab = tab;
    tab->editAnnotsWindow = ew;
    UpdateSaveButtonLabels(ew);

    UpdateAnnotationsList(ew);

    Rect lastPos = tab->lastEditAnnotsWindowPos;
    // size our editor window to be the same height as main window
    int minDy = lastPos.dy;
    if (minDy == 0) {
        minDy = 720;
        // TODO: this is slightly less that wanted
        HWND hwnd = tab->win->hwndCanvas;
        auto rc = HwndClientRect(hwnd);
        if (rc.dy > 0) {
            minDy = rc.dy;
        }
    }

    if (lastPos.IsEmpty()) {
        Size size = {520, minDy};
        LimitEditAnnotationsClientSizeToScreen(ew->hwnd, tab->win->hwndFrame, size);
        SetGrowingControlsToFit(ew, size.dy);
        LayoutAndSizeToContent(ew->mainLayout, size.dx, size.dy, ew->hwnd);
        ew->DoLayout(HwndClientRect(ew->hwnd).Size());
        HwndPositionToTheRightOf(ew->hwnd, tab->win->hwndFrame);
    } else {
        Size size = {lastPos.dx, minDy};
        LimitEditAnnotationsClientSizeToScreen(ew->hwnd, tab->win->hwndFrame, size);
        SetGrowingControlsToFit(ew, size.dy);
        LayoutAndSizeToContent(ew->mainLayout, size.dx, size.dy, ew->hwnd);
        ew->DoLayout(HwndClientRect(ew->hwnd).Size());
        // pass nullptr for hwnd so ShiftRectToWorkArea uses the saved rect
        // to find the correct monitor (not the monitor the hwnd is currently on)
        Rect r = HwndWindowRect(ew->hwnd);
        r.x = lastPos.x;
        r.y = lastPos.y;
        r = ShiftRectToWorkArea(r, nullptr, true);
        SetWindowPos(ew->hwnd, nullptr, r.x, r.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }
    if (!annot) annot = ew->tab->selectedAnnotation;
    ew->skipGoToPage = (annot != nullptr);
    if (annot) {
        bool isNew = annot != ew->tab->win->annotationUnderCursor;
        SetSelectedAnnotation(tab, annot, isNew, focus);
    }
    DarkModeApplyToNotifyWindowAndEraseBg(ew->hwnd);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    ew->SetIsVisible(true);
}

// Resize the annotation editor to clientDy and report list / Contents / gap
// sizes for tests/issue-3769.ts and tests/issue-5834.ts. selectItem is
// 1-based; 0 leaves the selection alone.
TempStr AnnotEditorLayoutResultTemp(int clientDy, int selectItem, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !EngineSupportsAnnotations(tab->GetEngine())) {
        return finish(StrL("ERROR no-annot-engine"), 1);
    }

    ShowEditAnnotationsWindow(tab, nullptr);
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    if (!ew || !ew->hwnd || !ew->listBox) {
        return finish(StrL("ERROR no-editor"), 1);
    }

    if (selectItem > 0) {
        int idx = selectItem - 1;
        if (!ew->annotations.isValidIndex(idx)) {
            return finish(fmt("ERROR no-annot item=%d n=%d", selectItem, len(ew->annotations)), 1);
        }
        ew->listBox->SetCurrentSelection(idx);
        ew->ListBoxSelectionChanged();
    }

    if (clientDy > 0) {
        ResizeHwndToClientArea(ew->hwnd, 520, clientDy, false);
        // MoveWindow can skip WM_SIZE when the outer size is unchanged;
        // force the grow-to-fit layout either way.
        Rect crNow = HwndClientRect(ew->hwnd);
        SetGrowingControlsToFit(ew, crNow.dy);
        ew->DoLayout(crNow.Size());
    }

    Rect cr = HwndClientRect(ew->hwnd);
    Rect listR = ew->listBox->lastBounds;
    int contentsDy = 0;
    if (ew->editContents && ew->editContents->IsVisible() && ew->editContents->hwnd) {
        contentsDy = HwndClientRect(ew->editContents->hwnd).dy;
    }
    int gapBelow = cr.dy - (listR.y + listR.dy);
    return finish(fmt("OK windowDy=%d listDy=%d contentsDy=%d gapBelow=%d", cr.dy, listR.dy, contentsDy, gapBelow), 0);
}
