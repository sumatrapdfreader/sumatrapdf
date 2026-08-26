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
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/VirtHost.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
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
#include "FormFields.h"
#include "SumatraPDF.h"
#include "Commands.h"
#include "DarkMode_win.h"

#include "Theme.h"
#include "FilterHighlightDraw.h"
#include "EditAnnotations.h"

constexpr int kBorderWidthMin = 0;
constexpr int kBorderWidthMax = 12;
constexpr int kMinAnnotListLines = 6;
constexpr int kMaxAnnotListLines = 12;
constexpr int kPreferredContentsLines = 6;
// pdf_update_annot on every EN_CHANGE stalls typing (annot-stress, FreeText).
constexpr UINT_PTR kContentsDebounceTimerId = 1;
constexpr UINT kContentsDebounceMs = 500;
constexpr UINT_PTR kSelectionDebounceTimerId = 2;
constexpr UINT kSelectionDebounceMs = 300;

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

    Edit* editFilter = nullptr;
    VirtListBox* listBox = nullptr;
    VirtText* staticRect = nullptr;
    VirtText* staticAuthor = nullptr;
    VirtText* staticModificationDate = nullptr;
    VirtText* staticPopupLabel = nullptr;
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
    Vec<Annotation*> visibleAnnots;
    StrVec filterWords;
    Vec<u8> filterHlScratch;

    bool skipGoToPage = false;
    // True while DoContents/etc. programmatically fill the edit; ignore EN_CHANGE.
    bool updatingControls = false;
    UINT_PTR contentsDebounceTimer = 0;
    UINT_PTR selectionDebounceTimer = 0;

    bool reloadSelectionValid = false;
    int reloadSelectionPageNo = 0;
    AnnotationType reloadSelectionType = AnnotationType::Unknown;
    RectF reloadSelectionBounds;
    int reloadScrollY = 0;

    str::Builder currTextColor;
    str::Builder currCustomColor;
    str::Builder currCustomInteriorColor;

    void OnSize(WindowBase::SizeEvent* ev);
    void OnFocus(WindowBase::FocusEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void OnTimer(WindowBase::TimerEvent* ev);

    void ListBoxSelectionChanged();

    ~EditAnnotationsWindow() override;
};

static Annotation* VisibleAnnotAt(EditAnnotationsWindow* ew, int idx);

static void RebuildAnnotationsListBox(EditAnnotationsWindow* ew);
static void FlushContentsFromEdit(EditAnnotationsWindow* ew);
static void DrawAnnotationListItem(EditAnnotationsWindow* ew, VirtListBox::DrawItemEvent* ev);
static void KillContentsDebounce(EditAnnotationsWindow* ew);
static void KillSelectionDebounce(EditAnnotationsWindow* ew);
static void ApplyListSelectionNow(EditAnnotationsWindow* ew);

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
            HideAnnotationHoverOverlay(win);
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

// Preserve enough UI state to rematch the selection after reload, then clear
// every non-owning Annotation* before the old engine is destroyed.
void InvalidateEditAnnotationsOnEngineChange(WindowTab* tab) {
    if (!tab || !tab->editAnnotsWindow) {
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    Annotation* selected = tab->selectedAnnotation;
    ew->reloadSelectionValid = AnnotationIsLive(selected);
    if (ew->reloadSelectionValid) {
        ew->reloadSelectionPageNo = selected->pageNo;
        ew->reloadSelectionType = selected->type;
        ew->reloadSelectionBounds = selected->bounds;
    }
    ew->reloadScrollY = ew->listBox ? ew->listBox->scrollY : 0;
    KillContentsDebounce(ew);
    KillSelectionDebounce(ew);
    // Do not FlushContents into an engine being replaced. In particular, a
    // pending edit from the old file must not overwrite an externally updated
    // annotation after the new engine is installed.
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
    // Preserve an existing selection only when deleting a different annotation.
    Annotation* keepSelected = annot == tab->selectedAnnotation ? nullptr : tab->selectedAnnotation;

    // Clear all UI holders before DeleteAnnotation frees the wrapper.
    DetachAnnotationFromUI(annot);
    DeleteAnnotation(annot);
    if (ew != nullptr) {
        // can be null if called from Menu.cpp and annotations window is not visible
        UpdateAnnotationsList(ew);
    }
    SetSelectedAnnotation(tab, keepSelected);
    // SetSelectedAnnotation only ScheduleRepaint (overlay handles). The page
    // bitmap still has the deleted annot until we re-render.
    if (IsMainWindowValidAndNotClosing(tab->win)) {
        MainWindowRerender(tab->win);
    }
}

static void DeleteSelectedAnnotation(EditAnnotationsWindow* ew) {
    if (!ew || !ew->listBox || !ew->tab) {
        return;
    }
    Vec<int> idxs;
    ew->listBox->GetSelectedIndices(idxs);
    if (len(idxs) == 0) {
        int idx = ew->listBox->GetCurrentSelection();
        if (idx >= 0) {
            idxs.Append(idx);
        }
    }
    if (len(idxs) == 0) {
        // can get out of sync e.g. after UpdateAnnotationsList during save/reload
        ew->tab->selectedAnnotation = nullptr;
        return;
    }
    Vec<Annotation*> toDelete;
    for (int idx : idxs) {
        Annotation* a = VisibleAnnotAt(ew, idx);
        if (a) {
            toDelete.Append(a);
        }
    }
    if (len(toDelete) == 0) {
        return;
    }
    Annotation* keepSelected = ew->tab->selectedAnnotation;
    if (toDelete.Contains(keepSelected)) {
        keepSelected = nullptr;
    }
    FlushContentsFromEdit(ew);
    for (Annotation* annot : toDelete) {
        DetachAnnotationFromUI(annot);
        DeleteAnnotation(annot);
    }
    UpdateAnnotationsList(ew);
    SetSelectedAnnotation(ew->tab, keepSelected);
    // SetSelectedAnnotation only ScheduleRepaint (overlay handles). The page
    // bitmap still has the deleted annot until we re-render.
    if (IsMainWindowValidAndNotClosing(ew->tab->win)) {
        MainWindowRerender(ew->tab->win);
    }
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
    ew->staticPopupLabel->SetIsVisible(false);
    ew->staticPopup->SetIsVisible(false);
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
    if (contentsDebounceTimer && hwnd) {
        KillTimer(hwnd, contentsDebounceTimer);
        contentsDebounceTimer = 0;
    }
    if (selectionDebounceTimer && hwnd) {
        KillTimer(hwnd, selectionDebounceTimer);
        selectionDebounceTimer = 0;
    }
    // hacky: we want the position of the main window
    // but the size of client area
    tab->lastEditAnnotsWindowPos = HwndWindowRect(hwnd);
    auto cr = HwndClientRect(hwnd);
    tab->lastEditAnnotsWindowPos.dx = cr.dx;
    tab->lastEditAnnotsWindowPos.dy = cr.dy;
    gGlobalPrefs->annotationsWindowSize = {cr.dx, cr.dy};

    if (tab->selectedAnnotation != nullptr) {
        tab->selectedAnnotation = nullptr;
        // tab->win can be null (SafeDeleteEditAnnotationsWindow checks it too)
        if (IsMainWindowValidAndNotClosing(tab->win)) {
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

static bool AnnotMatchesFilter(Annotation* annot, const StrVec& words) {
    if (len(words) == 0) {
        return true;
    }
    return FilterMatches(Contents(annot), words);
}

static Annotation* VisibleAnnotAt(EditAnnotationsWindow* ew, int idx) {
    if (!ew || !ew->visibleAnnots.isValidIndex(idx)) {
        return nullptr;
    }
    return ew->visibleAnnots[idx];
}

static int AnnotListIdealLines(int n) {
    if (n < kMinAnnotListLines) {
        return kMinAnnotListLines;
    }
    if (n > kMaxAnnotListLines) {
        return kMaxAnnotListLines;
    }
    return n;
}

static void RebuildAnnotationsListBox(EditAnnotationsWindow* ew) {
    ew->visibleAnnots.Reset();
    auto* model = new ListBoxModelStrings();
    int nAll = len(ew->annotations);
    for (int i = 0; i < nAll; i++) {
        Annotation* annot = ew->annotations[i];
        if (!AnnotMatchesFilter(annot, ew->filterWords)) {
            continue;
        }
        ew->visibleAnnots.Append(annot);
        model->strings.Append(AnnotationReadableNameTemp(annot->type));
    }

    int prevScrollY = ew->listBox->scrollY;
    Annotation* keep = ew->tab ? ew->tab->selectedAnnotation : nullptr;
    ew->listBox->SetModel(model); // resets the scroll position
    ew->listBox->ScrollTo(prevScrollY);
    if (keep) {
        int idx = ew->visibleAnnots.Find(keep);
        if (idx >= 0) {
            ew->listBox->SetCurrentSelection(idx);
        }
    }
    if (ew->editFilter) {
        ew->editFilter->SetCue(fmt(_TRA("filter %d annotations").s, len(ew->annotations)));
    }
    ew->listBox->idealSizeLines = AnnotListIdealLines(len(ew->visibleAnnots));
    EnableSaveIfAnnotationsChanged(ew);
}

static void FilterAnnotationsChanged(EditAnnotationsWindow* ew) {
    if (!ew || !ew->editFilter || ew->updatingControls) {
        return;
    }
    Annotation* keep = ew->tab ? ew->tab->selectedAnnotation : nullptr;
    ew->filterWords.Reset();
    SplitFilterToWords(ew->editFilter->GetTextTemp(), ew->filterWords);
    RebuildAnnotationsListBox(ew);
    if (ew->hwnd && ew->mainLayout) {
        Rect cr = HwndClientRect(ew->hwnd);
        if (cr.dx > 0 && cr.dy > 0) {
            ew->DoLayout(cr.Size());
        }
    }
    int idx = keep ? ew->visibleAnnots.Find(keep) : -1;
    if (idx >= 0) {
        ew->listBox->SetCurrentSelection(idx);
        return;
    }
    if (len(ew->visibleAnnots) > 0) {
        ew->listBox->SetCurrentSelection(0);
        ew->ListBoxSelectionChanged();
        return;
    }
    if (ew->tab) {
        SetSelectedAnnotation(ew->tab, nullptr);
    }
}

// Type on the left, optional contents in muted color, page number on the right.
// Contents is clipped so it cannot paint over the page column.
static void DrawAnnotationListItem(EditAnnotationsWindow* ew, VirtListBox::DrawItemEvent* ev) {
    Annotation* annot = VisibleAnnotAt(ew, ev->itemIndex);
    if (!annot) {
        return;
    }
    VirtListBox* lb = ev->listBox;
    Gfx* gfx = ev->gfx;
    Rect rc = ev->itemRect;

    Color colBg = lb->GetColor(kColListBg);
    Color colText = lb->GetColor(kColListText);
    if (IsSpecialColor(colBg)) {
        colBg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(colText)) {
        colText = GetSysColor(COLOR_WINDOWTEXT);
    }
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }
    gfx->FillRect(rc, colBg);

    int pad = DpiScale(6);
    Rect rcText = rc;
    rcText.x += pad;
    rcText.dx -= 2 * pad;
    if (rcText.dx <= 0) {
        return;
    }

    TempStr pageStr = fmt("%d", annot->pageNo);
    int pageGap = DpiScale(10);
    int pageColDx = gfx->MeasureText(pageStr, lb->font).dx;
    Rect rcPage = rcText;
    rcPage.x = std::max(rcText.x, rcText.x + rcText.dx - pageColDx);
    rcPage.dx = rcText.x + rcText.dx - rcPage.x;

    Str typeName = AnnotationReadableNameTemp(annot->type);
    int typeDx = gfx->MeasureText(typeName, lb->font).dx;
    int typeMaxDx = std::max(0, rcPage.x - pageGap - rcText.x);
    Rect rcType = rcText;
    rcType.dx = std::min(typeDx, typeMaxDx);
    if (rcType.dx > 0) {
        gfx->DrawText(typeName, rcType, gfxTextEllipsis | gfxTextVCenter | gfxTextLeft, lb->font, colText);
    }

    Str contents = Contents(annot);
    if (contents && rcType.dx > 0) {
        TempStr oneLine = str::NormalizeWSTemp(contents);
        if (oneLine) {
            int typeContentsGap = DpiScale(8);
            Rect rcContents = rcText;
            rcContents.x = rcType.x + rcType.dx + typeContentsGap;
            rcContents.dx = rcPage.x - pageGap - rcContents.x;
            if (rcContents.dx > 0) {
                gfx->PushClip(rcContents);
                DrawMaybeHighlightedText(gfx, rcContents, oneLine, ew->filterWords, ew->filterHlScratch, colBg, false,
                                         false, gfxTextEllipsis | gfxTextVCenter | gfxTextLeft, lb->font,
                                         ThemeWindowTextDisabledColor());
                gfx->PopClip();
            }
        }
    }

    gfx->FillRect(rcPage, colBg);
    gfx->DrawText(pageStr, rcPage, gfxTextEllipsis | gfxTextVCenter | gfxTextRight, lb->font, colText);
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
    KillSelectionDebounce(w);
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

static void FocusAnnotationsList(EditAnnotationsWindow* ew) {
    if (!ew || !ew->listBox || ew->listBox->ItemsCount() == 0) {
        return;
    }
    ew->SetFocusTo(ew->listBox);
}

static void FocusAnnotationsFilter(EditAnnotationsWindow* ew) {
    if (!ew || !ew->editFilter || !ew->editFilter->hwnd) {
        return;
    }
    HwndSetFocus(ew->editFilter->hwnd);
    ew->editFilter->SelectAll();
}

void EditAnnotationsWindow::OnFocus(WindowBase::FocusEvent*) {
    SelectTabInWindow(tab);
    // WM_KILLFOCUS clears virtual focus. If the window itself got HWND focus
    // back (not a child Edit), put it on the list so Home / End / arrows
    // navigate annotations again (issue #5975).
    if (::GetFocus() == hwnd && (!vroot || !vroot->focused)) {
        FocusAnnotationsList(this);
    }
}

extern bool SaveAnnotationsToMaybeNewPdfFile(WindowTab*);

static void ButtonSaveToNewFileHandler(EditAnnotationsWindow* ew) {
    FlushContentsFromEdit(ew);
    WindowTab* tab = ew->tab;
    // Saving to a new path replaces the tab and closes this window; choosing
    // the current path keeps it. Do not touch ew after the call in either case.
    bool ok = SaveAnnotationsToMaybeNewPdfFile(tab);
    if (!ok) {
        return;
    }
    // A replacement window, if needed, is created inside Save*.
}

extern bool SaveAnnotationsToExistingFile(WindowTab* tab);

static void ButtonSaveToCurrentPDFHandler(EditAnnotationsWindow* ew) {
    FlushContentsFromEdit(ew);
    // The PDF engine is replaced, but the editor refreshes in place.
    SaveAnnotationsToExistingFile(ew->tab);
}

static bool IsAnnotationListNavKey(int vkey) {
    return vkey == VK_UP || vkey == VK_DOWN || vkey == VK_PRIOR || vkey == VK_NEXT || vkey == VK_HOME || vkey == VK_END;
}

static bool FilterEditFocused(EditAnnotationsWindow* ew, HWND focusedHwnd) {
    return ew && ew->editFilter && ew->editFilter->hwnd && focusedHwnd == ew->editFilter->hwnd;
}

// Home / End in the filter: move the list when the caret is already at the
// start/end of the text (command palette). Ctrl+Home / Ctrl+End always do.
static bool FilterHomeEndMovesList(EditAnnotationsWindow* ew, const KeyEvent* ev) {
    if (ev->vkey != VK_HOME && ev->vkey != VK_END) {
        return true;
    }
    if (ev->isCtrl) {
        return true;
    }
    int selStart = 0, selEnd = 0;
    ew->editFilter->GetSelection(selStart, selEnd);
    int textLen = ew->editFilter->GetTextLen();
    bool toEnd = (ev->vkey == VK_END);
    return (selStart == selEnd) && (toEnd ? selEnd == textLen : selStart == 0);
}

void EditAnnotationsWindow::OnKeyDown(KeyEvent* ev) {
    bool filterFocused = FilterEditFocused(this, ev->hwnd);
    if (ev->vkey == VK_TAB) {
        // Tab / Shift+Tab. The ring is the layout order and skips what is hidden,
        // HWND controls and virtual ones alike. Apply a pending list selection
        // first so Contents / combos show the highlighted annot.
        ApplyListSelectionNow(this);
        TabNavigate(ev->isShift);
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_RETURN && filterFocused) {
        // Single-line filter would otherwise click the default Save button.
        // Apply the highlighted row, like Enter in the command palette.
        ApplyListSelectionNow(this);
        ev->didHandle = true;
        return;
    }
    if (filterFocused && ev->vkey == VK_ESCAPE) {
        if (len(editFilter->GetTextTemp()) > 0) {
            editFilter->SetText({});
            ev->didHandle = true;
        }
        return;
    }
    // Virt list/buttons use this HWND. Child Edits/combos do not: '/' must
    // still type in Contents, and Ctrl+F there is document find.
    HWND focused = ::GetFocus();
    bool childHwnd = focused && focused != hwnd && ::IsChild(hwnd, focused);
    if (!filterFocused && !childHwnd && !ev->isAlt) {
        bool ctrlF = (ev->vkey == 'F' || ev->vkey == 'f') && ev->isCtrl && !ev->isShift;
        bool f3 = ev->vkey == VK_F3 && !ev->isCtrl;
        bool slash = (ev->vkey == VK_OEM_2 || ev->vkey == VK_DIVIDE) && !ev->isCtrl && !ev->isShift;
        if (ctrlF || f3 || slash) {
            FocusAnnotationsFilter(this);
            ev->didHandle = true;
            return;
        }
    }
    if (ev->vkey == VK_DELETE) {
        if (ev->isCtrl && !ev->isAlt) {
            // Ctrl+Delete always deletes the annotation under the cursor on
            // the page (same as the main-window accelerator), even while this
            // window has focus and a different list item is selected.
            if (tab && tab->win) {
                HwndPostCommand(tab->win->hwndFrame, CmdDeleteAnnotation);
            }
            ev->didHandle = true;
            return;
        }
        // When focus is in a text field, let the Edit control handle Delete
        // (issue #5815). Ctrl+Delete is handled above.
        TempStr cls = HwndGetClassName(focused);
        if (str::EqI(cls, StrL("Edit"))) {
            return;
        }
        DeleteSelectedAnnotation(this);
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == 'A' && ev->isCtrl && !ev->isAlt && !ev->isShift) {
        // Ctrl+A selects every annotation, like a win32 listbox. Leave it
        // to the Contents edit when that has focus (issue #5976).
        if (childHwnd) {
            return;
        }
        if (listBox && listBox->multiSelect && listBox->ItemsCount() > 0) {
            FocusAnnotationsList(this);
            listBox->SelectAll();
            ev->didHandle = true;
        }
        return;
    }
    if (ev->vkey == 'S' && ev->isShift && ev->isCtrl) {
        // TODO: delay by posting a message?
        // TODO: the keybinding could be changed so this should
        // be more sophisticated and match the shortcut
        ButtonSaveToCurrentPDFHandler(this);
        ev->didHandle = true;
        return;
    }
    // Home / End / PageUp / PageDown / arrows used to move the native ListBox
    // in 3.6.1. The list is a VirtListBox now, so those keys only reach it when
    // it has virtual focus — which is often missing (window just opened, or
    // WM_KILLFOCUS cleared it). Drive the list from here unless a child HWND
    // (Contents edit, drop-down, trackbar) has focus (issue #5975). The filter
    // is the exception: arrows move the list and leave the caret in the edit,
    // like the command palette / Advanced Settings.
    if (IsAnnotationListNavKey(ev->vkey)) {
        if (!filterFocused && childHwnd) {
            return;
        }
        if (!listBox || listBox->ItemsCount() == 0) {
            return;
        }
        if (filterFocused && !FilterHomeEndMovesList(this, ev)) {
            return;
        }
        if (!filterFocused) {
            FocusAnnotationsList(this);
        }
        VirtKeyEvent ke;
        ke.vkey = ev->vkey;
        ke.isCtrl = ev->isCtrl;
        ke.isShift = ev->isShift;
        ke.isAlt = ev->isAlt;
        listBox->OnKeyDown(&ke);
        ev->didHandle = ke.didHandle;
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

static void DoRect(EditAnnotationsWindow* ew, Annotation* annot) {
    Str value = {};
    if (annot && gShowRect) {
        RectF rect = GetBounds(annot);
        value = fmt("x=%d y=%d dx=%d dy=%d", (int)rect.x, (int)rect.y, (int)rect.dx, (int)rect.dy);
    }
    ew->staticRect->SetText(value);
}

static void DoAuthor(EditAnnotationsWindow* ew, Annotation* annot) {
    Str author = {};
    if (annot) {
        author = Author(annot);
    }
    ew->staticAuthor->SetText(author);
}

static void AppendPdfDate(str::Builder& s, time_t secs) {
    struct tm tm;
    gmtime_s(&tm, &secs);
    char buf[100];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", &tm);
    s.Append(Str(buf));
}

static void DoModificationDate(EditAnnotationsWindow* ew, Annotation* annot) {
    str::Builder s;
    if (annot && ModificationDate(annot) != 0) {
        AppendPdfDate(s, ModificationDate(annot));
    }
    ew->staticModificationDate->SetText(ToStr(s));
}

static void DoPopup(EditAnnotationsWindow* ew, Annotation* annot) {
    int popupId = annot ? PopupId(annot) : -1;
    bool vis = popupId >= 0;
    ew->staticPopupLabel->SetIsVisible(vis);
    ew->staticPopup->SetIsVisible(vis);
    if (vis) {
        ew->staticPopup->SetText(fmt("%d 0 R", popupId));
    }
}

static void KillContentsDebounce(EditAnnotationsWindow* ew) {
    if (!ew || ew->contentsDebounceTimer == 0) {
        return;
    }
    if (ew->hwnd) {
        KillTimer(ew->hwnd, ew->contentsDebounceTimer);
    }
    ew->contentsDebounceTimer = 0;
}

static void KillSelectionDebounce(EditAnnotationsWindow* ew) {
    if (!ew || ew->selectionDebounceTimer == 0) {
        return;
    }
    if (ew->hwnd) {
        KillTimer(ew->hwnd, ew->selectionDebounceTimer);
    }
    ew->selectionDebounceTimer = 0;
}

// pdf_update_annot + dropping the page display list is too slow for EN_CHANGE.
static bool ApplyContentsFromEdit(EditAnnotationsWindow* ew) {
    KillContentsDebounce(ew);
    if (!ew || !ew->editContents || !ew->tab || ew->updatingControls) {
        return false;
    }
    Annotation* a = ew->tab->selectedAnnotation;
    if (!AnnotationIsLive(a) || ew->annotations.Find(a) < 0) {
        return false;
    }
    auto txt = ew->editContents->GetTextTemp();
    txt = str::ReplaceTemp(txt, StrL("\r\n"), StrL("\n"));
    if (!SetContents(a, txt)) {
        return false;
    }
    EnableSaveIfAnnotationsChanged(ew);
    if (ew->listBox) {
        ew->listBox->Invalidate();
    }
    return true;
}

// Push the contents edit into the selected annotation. Called on switch/save/
// close so unsaved last edits stick (plus df1b2aab8).
static void FlushContentsFromEdit(EditAnnotationsWindow* ew) {
    ApplyContentsFromEdit(ew);
}

static void DoContents(EditAnnotationsWindow* ew, Annotation* annot) {
    Str s = {};
    if (annot) {
        s = Contents(annot);
        // don't replace if already is "\r\n"
        s = str::ReplaceTemp(s, StrL("\r\n"), StrL("\n"));
        s = str::ReplaceTemp(s, StrL("\n"), StrL("\r\n"));
    }
    TempStr cur = ew->editContents->GetTextTemp();
    if (!str::Eq(cur, s)) {
        ew->updatingControls = true;
        ew->editContents->SetText(s);
        ew->updatingControls = false;
    }
    ew->editContents->SetIsEnabled(annot != nullptr);
}

static void SetDropDownItemsOnce(DropDown* w, SeqStrings items) {
    if (w->IsVisible() && len(w->items) > 0) {
        return;
    }
    w->SetItemsSeqStrings(items);
}

static void DoTextAlignment(EditAnnotationsWindow* ew, Annotation* annot) {
    bool vis = annot && Type(annot) == AnnotationType::FreeText;
    ew->staticTextAlignment->SetIsVisible(vis);
    if (!vis) {
        ew->dropDownTextAlignment->SetIsVisible(false);
        return;
    }
    int itemNo = Quadding(annot);
    SetDropDownItemsOnce(ew->dropDownTextAlignment, gQuaddingNames);
    ew->dropDownTextAlignment->SetCurrentSelection(itemNo);
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
    int itemNo = -1;
    if (annot && Type(annot) == AnnotationType::FreeText) {
        Str fontName = DefaultAppearanceTextFont(annot);
        // TODO: might have other fonts, like "Symb" and "ZaDb"
        itemNo = SeqStrIndex(gFontNames, fontName);
    }
    bool vis = itemNo >= 0;
    ew->staticTextFont->SetIsVisible(vis);
    if (!vis) {
        ew->dropDownTextFont->SetIsVisible(false);
        return;
    }
    SetDropDownItemsOnce(ew->dropDownTextFont, gFontReadableNames);
    ew->dropDownTextFont->SetCurrentSelection(itemNo);
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
    bool vis = annot && Type(annot) == AnnotationType::FreeText;
    ew->staticTextSize->SetIsVisible(vis);
    if (!vis) {
        ew->trackbarTextSize->SetIsVisible(false);
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
    bool vis = annot && Type(annot) == AnnotationType::FreeText;
    ew->staticTextColor->SetIsVisible(vis);
    if (!vis) {
        ew->dropDownTextColor->SetIsVisible(false);
        return;
    }
    PdfColor col = DefaultAppearanceTextColor(annot);
    DropDownFillColors(ew->dropDownTextColor, col, ew->currTextColor);
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
    bool vis = annot && AnnotationSupportsBorder(annot->type);
    ew->staticBorder->SetIsVisible(vis);
    if (!vis) {
        ew->trackbarBorder->SetIsVisible(false);
        return;
    }
    int borderWidth = BorderWidth(annot);
    borderWidth = setMinMax(borderWidth, kBorderWidthMin, kBorderWidthMax);
    TempStr s = fmt(_TRA("Border: %d").s, borderWidth);
    ew->staticBorder->SetText(s);
    ew->trackbarBorder->SetValue(borderWidth);
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
    bool vis = annot && Type(annot) == AnnotationType::Line;
    ew->staticLineStart->SetIsVisible(vis);
    ew->staticLineEnd->SetIsVisible(vis);
    if (!vis) {
        ew->dropDownLineStart->SetIsVisible(false);
        ew->dropDownLineEnd->SetIsVisible(false);
        return;
    }
    int start = 0;
    int end = 0;
    GetLineEndingStyles(annot, &start, &end);
    SetDropDownItemsOnce(ew->dropDownLineStart, gLineEndingStyles);
    ew->dropDownLineStart->SetCurrentSelection(start);
    SetDropDownItemsOnce(ew->dropDownLineEnd, gLineEndingStyles);
    ew->dropDownLineEnd->SetCurrentSelection(end);
    ew->dropDownLineStart->SetIsVisible(true);
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

static SeqStrings AnnotationIconNames(Annotation* annot) {
    SeqStrings items = nullptr;
    if (annot) {
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
                break;
        }
    }
    return items;
}

static void DoIcon(EditAnnotationsWindow* ew, Annotation* annot) {
    SeqStrings items = AnnotationIconNames(annot);
    Str itemName = annot ? IconName(annot) : Str{};
    bool vis = items && len(itemName) > 0;
    ew->staticIcon->SetIsVisible(vis);
    if (!vis) {
        ew->dropDownIcon->SetIsVisible(false);
        return;
    }
    ew->dropDownIcon->SetItemsSeqStrings(items);
    int idx = FindStringInArray(items, itemName, 0);
    ew->dropDownIcon->SetCurrentSelection(idx);
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
    bool vis = annot && AnnotationSupportsColor(annot->type);
    ew->staticColor->SetIsVisible(vis);
    if (!vis) {
        ew->dropDownColor->SetIsVisible(false);
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
    bool vis = annot && AnnotationSupportsInteriorColor(annot->type);
    ew->staticInteriorColor->SetIsVisible(vis);
    if (!vis) {
        ew->dropDownInteriorColor->SetIsVisible(false);
        return;
    }
    PdfColor col = InteriorColor(annot);
    DropDownFillColors(ew->dropDownInteriorColor, col, ew->currCustomInteriorColor);
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
    bool vis = annot && Type(annot) == AnnotationType::Highlight;
    ew->staticOpacity->SetIsVisible(vis);
    if (!vis) {
        ew->trackbarOpacity->SetIsVisible(false);
        return;
    }
    int opacity = Opacity(annot);
    TempStr s = fmt(_TRA("Opacity: %d").s, opacity);
    ew->staticOpacity->SetText(s);
    ew->trackbarOpacity->SetValue(opacity);
    ew->trackbarOpacity->SetIsVisible(true);
}

static void DoSaveEmbed(EditAnnotationsWindow* ew, Annotation* annot) {
    bool vis = annot && Type(annot) == AnnotationType::FileAttachment;
    ew->buttonSaveAttachment->SetIsVisible(vis);
    ew->buttonEmbedAttachment->SetIsVisible(vis);
}

constexpr const WCHAR* kAnnotationHoverOverlayClassName = L"SumatraAnnotationHoverOverlay";

struct AnnotationHoverRows {
    StrVec keys;
    StrVec labels;
    StrVec values;

    void Add(Str key, Str label, Str value) {
        keys.Append(key);
        labels.Append(label);
        values.Append(value);
    }
};

struct AnnotationHoverOverlay {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    VirtHost* host = nullptr;
    PlatformFont* font = nullptr;
    Size size;
    Rect lastPlaced;
    Rect anchorRect;
    RectF annotBounds;
    Str rowsDump;
    int rowCount = 0;
    bool isAbove = false;
};

static TempStr AnnotationColorNameTemp(PdfColor color) {
    TempStr known = GetKnownColorNameTemp(color);
    if (known) {
        return known;
    }
    str::Builder value;
    SerializePdfColor(color, value);
    return ToStrTemp(value);
}

static TempStr ShortAnnotationHoverValueTemp(Str value, int maxRunes = 72) {
    if (len(value) == 0) {
        return StrL("");
    }
    TempStr oneLine = str::NormalizeWSTemp(value);
    return ShortenStringUtf8Temp(oneLine, maxRunes);
}

static TempStr ShortAnnotationContentsTemp(Str value) {
    constexpr int kMaxRunes = 32;
    TempStr oneLine = str::NormalizeWSTemp(value);
    int nRunes = utf8StrLen((const u8*)CStrTemp(oneLine));
    if (nRunes >= 0 && nRunes <= kMaxRunes) {
        return oneLine;
    }

    int bytesToKeep = std::min(kMaxRunes, len(oneLine));
    if (nRunes >= 0) {
        bytesToKeep = 0;
        for (int i = 0; i < kMaxRunes; i++) {
            int runeBytes = utf8RuneLen((const u8*)oneLine.s + bytesToKeep);
            ReportIf(runeBytes <= 0);
            if (runeBytes <= 0) {
                break;
            }
            bytesToKeep += runeBytes;
        }
    } else if (len(oneLine) <= kMaxRunes) {
        return oneLine;
    }
    return str::JoinTemp(Str(oneLine.s, bytesToKeep), StrL("..."));
}

// Keep the hover card's rows in lockstep with the per-annotation controls in
// UpdateUIForSelectedAnnotation(). Metadata is always present; type-specific
// properties use the same visibility predicates as the editor.
static void CollectAnnotationHoverRows(Annotation* annot, AnnotationHoverRows& rows) {
    Str contents = Contents(annot);
    if (len(contents) > 0) {
        rows.Add(StrL("contents"), _TRA("Contents:"), ShortAnnotationContentsTemp(contents));
    }

    AnnotationType type = Type(annot);
    if (type == AnnotationType::FreeText) {
        int quadding = Quadding(annot);
        rows.Add(StrL("textAlignment"), _TRA("Text Alignment:"), SeqStrByIndex(gQuaddingNames, quadding));

        int fontIdx = SeqStrIndex(gFontNames, DefaultAppearanceTextFont(annot));
        if (fontIdx >= 0) {
            rows.Add(StrL("textFont"), _TRA("Text Font:"), SeqStrByIndex(gFontReadableNames, fontIdx));
        }
        rows.Add(StrL("textSize"), _TRA("Text Size:"), fmt("%d", DefaultAppearanceTextSize(annot)));
        rows.Add(StrL("textColor"), _TRA("Text Color:"), AnnotationColorNameTemp(DefaultAppearanceTextColor(annot)));
    }

    if (type == AnnotationType::Line) {
        int start = 0;
        int end = 0;
        GetLineEndingStyles(annot, &start, &end);
        rows.Add(StrL("lineStart"), _TRA("Line Start:"), SeqStrByIndex(gLineEndingStyles, start));
        rows.Add(StrL("lineEnd"), _TRA("Line End:"), SeqStrByIndex(gLineEndingStyles, end));
    }

    Str icon = IconName(annot);
    if (AnnotationIconNames(annot) && icon) {
        rows.Add(StrL("icon"), _TRA("Icon:"), ShortAnnotationHoverValueTemp(icon));
    }
    if (AnnotationSupportsBorder(type)) {
        rows.Add(StrL("border"), _TRA("Border:"), fmt("%d", BorderWidth(annot)));
    }
    if (AnnotationSupportsColor(type)) {
        bool isBackground = IsAnnotationTypeInArray(gAnnotsIsColorBackground, dimofi(gAnnotsIsColorBackground), type);
        Str label = isBackground ? _TRA("Background Color:") : _TRA("Color:");
        rows.Add(StrL("color"), label, AnnotationColorNameTemp(GetColor(annot)));
    }
    if (AnnotationSupportsInteriorColor(type)) {
        rows.Add(StrL("interiorColor"), _TRA("Interior Color:"), AnnotationColorNameTemp(InteriorColor(annot)));
    }
    if (type == AnnotationType::Highlight) {
        rows.Add(StrL("opacity"), _TRA("Opacity:"), fmt("%d", Opacity(annot)));
    }

    rows.Add(StrL("author"), _TRA("Author:"), ShortAnnotationHoverValueTemp(Author(annot)));
    str::Builder date;
    if (ModificationDate(annot) != 0) {
        AppendPdfDate(date, ModificationDate(annot));
    }
    rows.Add(StrL("date"), _TRA("Date:"), ToStr(date));
    int popupId = PopupId(annot);
    if (popupId >= 0) {
        rows.Add(StrL("popup"), _TRA("Popup:"), fmt("%d 0 R", popupId));
    }
    if (gShowRect) {
        RectF rect = GetBounds(annot);
        rows.Add(StrL("rect"), _TRA("Rect:"), fmt("%d-%d@%d-%d", (int)rect.dx, (int)rect.dy, (int)rect.x, (int)rect.y));
    }
}

static Color AnnotationHoverBg() {
    return ThemeNotificationsBackgroundColor();
}

static Color AnnotationHoverText() {
    return ThemeNotificationsTextColor();
}

static void PaintAnnotationHoverOverlay(AnnotationHoverOverlay*, VirtHostPaintEvent* ev) {
    int radius = DpiScale(6);
    ev->gfx->FillRoundedRect(ev->clientRect, radius, AnnotationHoverBg(), ThemeEdgeColor());
}

static void AnnotationHoverNativeMsg(AnnotationHoverOverlay*, VirtHostNativeMsg* ev) {
    if (ev->msg == WM_NCHITTEST) {
        // This is an informational card, not a new interaction surface. Let
        // mouse input continue to reach the annotation and canvas beneath it.
        ev->didHandle = true;
        ev->res = HTTRANSPARENT;
    }
}

static AnnotationHoverOverlay* GetOrCreateAnnotationHoverOverlay(MainWindow* win) {
    if (win->annotationHoverOverlay) {
        return win->annotationHoverOverlay;
    }
    auto* overlay = new AnnotationHoverOverlay();
    overlay->win = win;
    overlay->font = GetAppFontForDpi(DpiGetForHwnd(win->hwndCanvas));

    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStr(kAnnotationHoverOverlayClassName);
    args.initialSize = {1, 1};
    args.bgColor = AnnotationHoverBg();
    args.isPopup = true;
    args.visible = false;
    args.noActivate = true;
    args.userData = overlay;
    overlay->host = VirtHost::Create(args);
    if (!overlay->host) {
        delete overlay;
        return nullptr;
    }
    overlay->host->onPaintBackground = MkFunc1(PaintAnnotationHoverOverlay, overlay);
    overlay->host->onNativeMsg = MkFunc1(AnnotationHoverNativeMsg, overlay);
    win->annotationHoverOverlay = overlay;
    return overlay;
}

static void BuildAnnotationHoverOverlay(AnnotationHoverOverlay* overlay, Annotation* annot) {
    AnnotationHoverRows rows;
    CollectAnnotationHoverRows(annot, rows);

    Color textColor = AnnotationHoverText();
    Color labelColor = ThemeWindowTextDisabledColor();
    auto* title = NewVirtText({
        .s = AnnotationReadableNameTemp(Type(annot)),
        .font = GetBoldPlatformFont(overlay->font),
        .textColor = textColor,
    });

    auto* table = new Table();
    table->SetSize(len(rows.labels), 2);
    table->colGap = DpiScale(12);
    table->rowGap = DpiScale(3);
    for (int row = 0; row < len(rows.labels); row++) {
        auto* label = NewVirtText({
            .s = rows.labels[row],
            .font = overlay->font,
            .textColor = labelColor,
        });
        auto* value = NewVirtText({
            .s = rows.values[row],
            .font = overlay->font,
            .textColor = textColor,
            .ellipsis = !str::Eq(rows.keys[row], StrL("date")) && !str::Eq(rows.keys[row], StrL("rect")),
        });
        TableCell& labelCell = table->SetCell(row, 0, label);
        labelCell.alignV = CrossAxisAlign::CrossCenter;
        TableCell& valueCell = table->SetCell(row, 1, value);
        valueCell.alignV = CrossAxisAlign::CrossCenter;
    }

    auto* column = new VBox();
    column->alignCross = CrossAxisAlign::Stretch;
    column->AddChild(title);
    column->AddChild(new Spacer(0, DpiScale(5)));
    column->AddChild(table);
    auto* content = new Padding(column, DpiScaledInsets(8, 10));
    overlay->size = overlay->host->SetLayoutSizedToContent(content);
    overlay->host->ClipToRoundedRect(6, overlay->size);

    str::Builder dump;
    for (int i = 0; i < len(rows.keys); i++) {
        dump.Append(fmt("row %s=%s\n", rows.keys[i], rows.values[i]));
    }
    str::ReplaceWithCopy(&overlay->rowsDump, ToStr(dump));
    overlay->rowCount = len(rows.keys);
    overlay->annot = annot;
    overlay->tab = overlay->win->CurrentTab();
    overlay->annotBounds = GetRect(annot);
}

static bool SameRectF(RectF a, RectF b) {
    return a.x == b.x && a.y == b.y && a.dx == b.dx && a.dy == b.dy;
}

static bool PositionAnnotationHoverOverlay(AnnotationHoverOverlay* overlay) {
    MainWindow* win = overlay ? overlay->win : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    Annotation* annot = overlay ? overlay->annot : nullptr;
    if (!dm || !annot || !dm->PageVisible(PageNo(annot))) {
        return false;
    }

    Rect canvas = HwndClientRect(win->hwndCanvas);
    Rect annotRect = dm->CvtToScreen(PageNo(annot), GetRect(annot));
    overlay->anchorRect = annotRect;
    if (canvas.Intersect(annotRect).IsEmpty()) {
        return false;
    }

    int gap = DpiScale(6);
    int width = overlay->size.dx;
    int height = std::min(overlay->size.dy, canvas.dy);
    int x = annotRect.x;
    int y = annotRect.y + annotRect.dy + gap;
    overlay->isAbove = y + height > canvas.y + canvas.dy;
    if (overlay->isAbove) {
        y = annotRect.y - gap - height;
    }

    int maxX = canvas.x + canvas.dx - width;
    x = std::max(canvas.x, std::min(x, maxX));
    int maxY = canvas.y + canvas.dy - height;
    y = std::max(canvas.y, std::min(y, maxY));

    Point screen = HwndClientToScreen(win->hwndCanvas, Point(x, y));
    Rect placed(screen.x, screen.y, width, height);
    if (placed != overlay->lastPlaced) {
        overlay->host->SetBounds(placed);
        overlay->lastPlaced = placed;
    }
    return true;
}

void HideAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay) {
        return;
    }
    overlay->host->Show(false);
    overlay->annot = nullptr;
    overlay->tab = nullptr;
    overlay->lastPlaced = {};
    overlay->anchorRect = {};
    overlay->annotBounds = {};
    overlay->rowCount = 0;
    str::Free(overlay->rowsDump);
    overlay->rowsDump = {};
}

void UpdateAnnotationHoverOverlay(MainWindow* win) {
    Annotation* annot = win ? win->annotationUnderCursor : nullptr;
    if (!win || !win->pdfAnnotationsToolbarEnabled || win->mouseAction != MouseAction::None ||
        !AnnotationIsLive(annot)) {
        HideAnnotationHoverOverlay(win);
        return;
    }
    AnnotationHoverOverlay* overlay = GetOrCreateAnnotationHoverOverlay(win);
    if (!overlay) {
        return;
    }
    RectF bounds = GetRect(annot);
    bool rebuild = overlay->annot != annot || overlay->tab != win->CurrentTab() ||
                   !SameRectF(bounds, overlay->annotBounds) || !overlay->host->IsVisible();
    if (rebuild) {
        BuildAnnotationHoverOverlay(overlay, annot);
    }
    if (!PositionAnnotationHoverOverlay(overlay)) {
        HideAnnotationHoverOverlay(win);
        return;
    }
    overlay->host->Show(true);
    if (rebuild) {
        overlay->host->Invalidate(false);
    }
}

void RepositionAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay || !overlay->host->IsVisible()) {
        return;
    }
    UpdateAnnotationHoverOverlay(win);
}

void RefreshAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay || !overlay->host->IsVisible()) {
        return;
    }
    overlay->annot = nullptr;
    UpdateAnnotationHoverOverlay(win);
}

void DeleteAnnotationHoverOverlay(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    if (!overlay) {
        return;
    }
    win->annotationHoverOverlay = nullptr;
    str::Free(overlay->rowsDump);
    delete overlay->host;
    delete overlay;
}

TempStr AnnotationHoverOverlayStateTemp(MainWindow* win) {
    AnnotationHoverOverlay* overlay = win ? win->annotationHoverOverlay : nullptr;
    bool visible = overlay && overlay->host && overlay->host->IsVisible();
    if (!visible) {
        return StrL("overlay visible=0\n");
    }
    Rect r = overlay->host->ScreenRect();
    Rect a = overlay->anchorRect;
    str::Builder out;
    out.Append(fmt("overlay visible=1 rows=%d above=%d rect=%d,%d,%d,%d anchor=%d,%d,%d,%d\n", overlay->rowCount,
                   overlay->isAbove ? 1 : 0, r.x, r.y, r.dx, r.dy, a.x, a.y, a.dx, a.dy));
    out.Append(overlay->rowsDump);
    return ToStrTemp(out);
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

static void RelayoutEditAnnotationsWindow(EditAnnotationsWindow* ew, int clientDx, int clientDy);
static void LayoutAnnotWindowInPlace(EditAnnotationsWindow* ew);

// TODO: maybe use ew->tab->selectedAnnotation instead of annot
static void UpdateUIForSelectedAnnotation(EditAnnotationsWindow* ew, Annotation* annot, bool isNew = false,
                                          EditAnnotFocus focus = EditAnnotFocus::Default) {
    // Set each control to its final visibility (no hide-all). Unchanged
    // dropdowns/trackbars stay put so navigating the list does not flash.
    // The window is WS_EX_COMPOSITED and the virt tree double-buffered;
    // native children are positioned with SWP_NOREDRAW and painted on
    // the next WM_PAINT with the parent.
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

    if (annot) {
        int itemNo = ew->visibleAnnots.Find(annot);
        if (itemNo < 0 && len(ew->filterWords) > 0 && ew->editFilter) {
            // clicked/created annot is hidden by the filter: drop it so the
            // row shows up
            ew->updatingControls = true;
            ew->editFilter->SetText({});
            ew->updatingControls = false;
            ew->filterWords.Reset();
            RebuildAnnotationsListBox(ew);
            itemNo = ew->visibleAnnots.Find(annot);
        }
        if (itemNo < 0) {
            // can happen if annotations list is out of sync (e.g. after reload)
            LayoutAnnotWindowInPlace(ew);
            return;
        }

        // Don't collapse a Shift/Ctrl multi-select when the caret is already
        // on this row (issue #5976).
        if (ew->listBox->GetCurrentSelection() != itemNo) {
            ew->listBox->SetCurrentSelection(itemNo);
        }
        ew->buttonDelete->SetIsEnabled(true);

        // NOLINTNEXTLINE(bugprone-branch-clone): branch order matters, an explicit focus wins over isNew
        HWND focused = ::GetFocus();
        bool childFocused = focused && focused != ew->hwnd && ::IsChild(ew->hwnd, focused);
        if (focus == EditAnnotFocus::Edit) {
            HwndSetFocus(ew->editContents->hwnd);
            ew->editContents->SelectAll();
        } else if (focus == EditAnnotFocus::List) { // NOLINT(bugprone-branch-clone)
            ew->SetFocusTo(ew->listBox);
        } else if (isNew && annot->type == AnnotationType::FreeText) {
            HwndSetFocus(ew->editContents->hwnd);
            // ew->editContents->SetCursorPositionAtEnd();
            ew->editContents->SelectAll();
        } else if (childFocused) {
            // Contents / combo already has focus (flushed after list nav)
        } else if (!ew->vroot || ew->vroot->focused != ew->listBox) {
            ew->SetFocusTo(ew->listBox);
        }
    } else {
        ew->buttonDelete->SetIsEnabled(false);
    }

    LayoutAnnotWindowInPlace(ew);

    if (ew->skipGoToPage || isNew) {
        ew->skipGoToPage = false;
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

// GoToPage / canvas scroll for the current selection. Posted so holding
// arrows in the annot list can keep moving the caret (issue #6009). Find
// uses ScheduleRepaint, not MainWindowRerender, for the same reason.
// GoToPage used to run before HwndShowWithoutActivate; posting it meant the
// frame (TOC select, overlay scrollbar HWND_TOP) could cover this window.
static void ShowSelectedAnnotationView(WindowTab* tab) {
    if (!tab) {
        return;
    }
    tab->pendingShowSelectedAnnotation = false;
    if (!IsMainWindowValidAndNotClosing(tab->win)) {
        return;
    }
    MainWindow* win = tab->win;
    bool tabOpen = false;
    for (WindowTab* t : win->Tabs()) {
        if (t == tab) {
            tabOpen = true;
            break;
        }
    }
    if (!tabOpen) {
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    HWND annotHwnd = ew ? ew->hwnd : nullptr;
    HWND prevFocus = ::GetFocus();
    bool annotHadFocus = annotHwnd && prevFocus && (prevFocus == annotHwnd || ::IsChild(annotHwnd, prevFocus));
    Annotation* annot = tab->selectedAnnotation;
    DisplayModel* dm = tab->AsFixed();
    if (AnnotationIsLive(annot) && dm) {
        int pageNo = annot->pageNo;
        int nPages = dm->PageCount();
        if (pageNo < 1 || pageNo > nPages) {
            logf("ShowSelectedAnnotationView: invalid pageNo=%d nPages=%d\n", pageNo, nPages);
        } else if (!dm->PageVisible(pageNo)) {
            dm->GoToPage(pageNo, true);
        }
    }
    ScheduleRepaint(win, 0);
    ToolbarUpdateStateForWindow(win, false);
    if (annotHwnd && ::IsWindow(annotHwnd)) {
        HwndShowWithoutActivate(annotHwnd);
        if (annotHadFocus && prevFocus && ::IsWindow(prevFocus) && ::GetFocus() != prevFocus) {
            ::SetFocus(prevFocus);
        }
    }
}

static void ScheduleShowSelectedAnnotationView(WindowTab* tab) {
    if (!tab || tab->pendingShowSelectedAnnotation) {
        return;
    }
    tab->pendingShowSelectedAnnotation = true;
    uitask::Post(MkFunc0(ShowSelectedAnnotationView, tab), "ShowSelectedAnnot");
}

void SetSelectedAnnotation(WindowTab* tab, Annotation* annot, bool isNew, EditAnnotFocus focus) {
    MainWindow* win = tab->win;
    auto* ew = tab->editAnnotsWindow;
    if (ew) {
        KillSelectionDebounce(ew);
    }
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
    if (ew) {
        UpdateUIForSelectedAnnotation(ew, annot, isNew, focus);
        HwndShowWithoutActivate(ew->hwnd);
    }
    ScheduleShowSelectedAnnotationView(tab);
}

static void AddAnnotPage(Vec<int>& pages, int pageNo, int pageCount) {
    if (pageNo < 1 || pageNo > pageCount) {
        return;
    }
    if (pages.Contains(pageNo)) {
        return;
    }
    pages.Append(pageNo);
}

// Load annot wrappers for one page (page + annot dicts, not stext).
static void LoadAnnotsForPage(EngineMupdf* e, int pageNo) {
    if (!e || pageNo < 1 || pageNo > e->pageCount) {
        return;
    }
    FzPageInfo* pi = e->GetFzPageInfo(pageNo, true);
    if (!pi) {
        logf("LoadAnnotsForPage: page %d GetFzPageInfo failed\n", pageNo);
        return;
    }
    logf("LoadAnnotsForPage: page %d n=%d loaded=%d\n", pageNo, len(pi->annotations), (int)pi->annotsLoaded);
}

// Pages the background loader should finish first: current page (toolbar page
// even when visibleRatio is still 0), every page overlapping the viewport, and
// a context-menu annotation's page.
static void CollectPriorityAnnotPages(EditAnnotationsWindow* ew, Annotation* extra, Vec<int>& pages) {
    pages.Reset();
    if (!ew || !ew->tab) {
        return;
    }
    DisplayModel* dm = ew->tab->AsFixed();
    if (!dm) {
        return;
    }
    int n = dm->PageCount();
    int curr = dm->CurrentPageNo();
    if (curr < 1 || curr > n) {
        curr = 1;
    }
    AddAnnotPage(pages, curr, n);
    AddAnnotPage(pages, dm->FirstVisiblePageNo(), n);
    for (int i = 1; i <= n; i++) {
        if (dm->PageVisible(i)) {
            AddAnnotPage(pages, i, n);
        }
    }
    if (extra) {
        AddAnnotPage(pages, extra->pageNo, n);
    }
}

static void OnAnnotsProgress(WindowTab* tab) {
    if (!tab || !tab->editAnnotsWindow) {
        return;
    }
    if (!IsMainWindowValidAndNotClosing(tab->win)) {
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    EngineMupdf* engine = GetEngineMupdf(ew);
    if (!engine) {
        return;
    }
    int nBefore = len(ew->annotations);
    EngineMupdfGetLoadedAnnotations(engine, ew->annotations);
    logf("OnAnnotsProgress: nAnnots=%d (was %d)\n", len(ew->annotations), nBefore);
    if (len(ew->annotations) == nBefore) {
        return;
    }
    RebuildAnnotationsListBox(ew);
    LayoutAnnotWindowInPlace(ew);
}

void UpdateAnnotationsList(EditAnnotationsWindow* ew) {
    if (!ew) {
        return;
    }
    auto* engine = GetEngineMupdf(ew);
    if (!engine) {
        return;
    }
    Annotation* extra = ew->tab ? ew->tab->selectedAnnotation : nullptr;
    Vec<int> firstPages;
    CollectPriorityAnnotPages(ew, extra, firstPages);
    if (ew->reloadSelectionValid) {
        AddAnnotPage(firstPages, ew->reloadSelectionPageNo, engine->pageCount);
    }
    // Load current/visible pages on this thread so the window never opens
    // empty. loadQuick skips stext; we still wait for pagesLock/renderLock.
    for (int pageNo : firstPages) {
        LoadAnnotsForPage(engine, pageNo);
    }
    EngineMupdfGetLoadedAnnotations(engine, ew->annotations);
    logf("UpdateAnnotationsList: nAnnots=%d firstPages=%d extra=%d\n", len(ew->annotations), len(firstPages),
         extra ? extra->pageNo : 0);
    RebuildAnnotationsListBox(ew);
    LayoutAnnotWindowInPlace(ew);
    EngineMupdfStartLoadAllAnnotations(engine, firstPages, MkFunc0(OnAnnotsProgress, ew->tab));
}

// Repopulate an existing editor from the replacement engine and restore its
// logical selection without recreating or activating the window.
void RefreshEditAnnotationsAfterEngineChange(WindowTab* tab) {
    if (!tab || !tab->editAnnotsWindow) {
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    UpdateAnnotationsList(ew);

    Annotation* match = nullptr;
    if (ew->reloadSelectionValid) {
        for (Annotation* annot : ew->annotations) {
            if (annot->pageNo == ew->reloadSelectionPageNo && annot->type == ew->reloadSelectionType &&
                annot->bounds == ew->reloadSelectionBounds) {
                match = annot;
                break;
            }
        }
    }
    ew->reloadSelectionValid = false;
    if (ew->listBox) {
        ew->listBox->ScrollTo(ew->reloadScrollY);
    }
    // This is the same logical selection, so refresh its controls and overlay
    // without scheduling GoToPage (the document reload already restored the
    // user's view).
    tab->selectedAnnotation = match;
    SetSelectedAnnotation(tab, match);
    if (ew->hwnd) {
        HwndInvalidate(ew->hwnd);
    }
}

static void ButtonDeleteHandler(EditAnnotationsWindow* ew) {
    ReportIf(!ew->tab->selectedAnnotation);
    DeleteSelectedAnnotation(ew);
}

static void ListBoxSelectionChanged(EditAnnotationsWindow* ew) {
    ew->ListBoxSelectionChanged();
}

// Apply the list caret to the details panel, canvas overlay, and GoToPage.
static void ApplyListSelectionNow(EditAnnotationsWindow* ew) {
    KillSelectionDebounce(ew);
    if (!ew || !ew->listBox || !ew->tab) {
        return;
    }
    int itemNo = ew->listBox->GetCurrentSelection();
    if (itemNo < 0) {
        return;
    }
    Annotation* annot = VisibleAnnotAt(ew, itemNo);
    if (!annot) {
        logf("ApplyListSelectionNow: invalid itemNo=%d, len(visibleAnnots)=%d\n", itemNo, len(ew->visibleAnnots));
        ReportDebugIf(true);
        return;
    }
    if (annot == ew->tab->selectedAnnotation) {
        return;
    }
    SetSelectedAnnotation(ew->tab, annot);
}

// Holding arrows only needs the list caret to move. Rebuild the form 300ms
// after the last change so intermediate annots are skipped.
static void ScheduleListSelectionUi(EditAnnotationsWindow* ew) {
    if (!ew || !ew->tab || !ew->listBox) {
        return;
    }
    int itemNo = ew->listBox->GetCurrentSelection();
    Annotation* annot = itemNo >= 0 ? VisibleAnnotAt(ew, itemNo) : nullptr;
    if (annot == ew->tab->selectedAnnotation) {
        KillSelectionDebounce(ew);
        return;
    }
    if (!ew->hwnd) {
        ApplyListSelectionNow(ew);
        return;
    }
    KillSelectionDebounce(ew);
    UINT_PTR id = SetTimer(ew->hwnd, kSelectionDebounceTimerId, kSelectionDebounceMs, nullptr);
    if (!id) {
        ApplyListSelectionNow(ew);
        return;
    }
    ew->selectionDebounceTimer = id;
}

void EditAnnotationsWindow::ListBoxSelectionChanged() {
    ScheduleListSelectionUi(this);
}

// Clicking Contents / a combo takes HWND focus off the list. Apply the
// highlighted row first so those controls belong to that annot.
static void OnAnnotWndProc(WindowBase::WndProcEvent* ev) {
    auto* ew = (EditAnnotationsWindow*)ev->w;
    if (ev->msg == WM_CHAR && ev->wparam == '/') {
        HWND focused = ::GetFocus();
        bool childHwnd = focused && focused != ew->hwnd && ::IsChild(ew->hwnd, focused);
        if (!childHwnd) {
            FocusAnnotationsFilter(ew);
            ev->didHandle = true;
            ev->result = 0;
        }
        return;
    }
    if (ev->msg != WM_KILLFOCUS) {
        return;
    }
    HWND newFocus = (HWND)ev->wparam;
    if (newFocus && ew->hwnd && ::IsChild(ew->hwnd, newFocus)) {
        ApplyListSelectionNow(ew);
    }
}

static UINT_PTR gMainWindowRerenderTimer = 0;
static MainWindow* gMainWindowForRender = nullptr;

static void ScheduleMainWindowRerender(EditAnnotationsWindow* ew) {
    if (!ew || !ew->tab) {
        return;
    }
    MainWindow* win = ew->tab->win;
    if (!win || !win->hwndCanvas) {
        return;
    }
    if (gMainWindowRerenderTimer != 0) {
        KillTimer(win->hwndCanvas, gMainWindowRerenderTimer);
        gMainWindowRerenderTimer = 0;
    }
    gMainWindowForRender = win;
    gMainWindowRerenderTimer = SetTimer(win->hwndCanvas, 1, 1000, [](HWND, UINT, UINT_PTR, DWORD) {
        if (IsMainWindowValidAndNotClosing(gMainWindowForRender)) {
            MainWindowRerender(gMainWindowForRender);
        }
        gMainWindowRerenderTimer = 0;
    });
}

// EN_CHANGE: do not call pdf_update_annot per keystroke.
static void ContentsChanged(EditAnnotationsWindow* ew) {
    if (!ew || !ew->tab || !ew->editContents || ew->updatingControls) {
        return;
    }
    if (!AnnotationIsLive(ew->tab->selectedAnnotation)) {
        return;
    }
    if (!ew->hwnd) {
        ApplyContentsFromEdit(ew);
        ScheduleMainWindowRerender(ew);
        return;
    }
    KillContentsDebounce(ew);
    UINT_PTR id = SetTimer(ew->hwnd, kContentsDebounceTimerId, kContentsDebounceMs, nullptr);
    if (!id) {
        ApplyContentsFromEdit(ew);
        ScheduleMainWindowRerender(ew);
        return;
    }
    ew->contentsDebounceTimer = id;
}

static void ContentsKillFocus(EditAnnotationsWindow* ew) {
    if (ApplyContentsFromEdit(ew)) {
        ScheduleMainWindowRerender(ew);
    }
}

void EditAnnotationsWindow::OnTimer(WindowBase::TimerEvent* ev) {
    if (!ev) {
        return;
    }
    if (ev->timerId == kSelectionDebounceTimerId) {
        ApplyListSelectionNow(this);
        return;
    }
    if (ev->timerId != kContentsDebounceTimerId) {
        return;
    }
    if (ApplyContentsFromEdit(this)) {
        ScheduleMainWindowRerender(this);
    }
}

// The list is as tall as the annotations (at least kMinAnnotListLines, capped
// at kMaxAnnotListLines). Contents stays kPreferredContentsLines so leftover
// height is a spacer below the per-annot buttons, and navigating does not
// resize the edit.
static void SetGrowingControlsToFit(EditAnnotationsWindow* ew, int targetClientDy) {
    if (!ew->listBox || !ew->mainLayout) {
        return;
    }
    if (ew->listBox->GetItemHeight() <= 0 || targetClientDy <= 0) {
        return;
    }
    ew->listBox->idealSizeLines = AnnotListIdealLines(ew->listBox->ItemsCount());
    if (ew->editContents) {
        ew->editContents->idealSizeLines = kPreferredContentsLines;
    }
}

// Relayout the current client size without resizing the HWND. Used when
// switching annotations: ResizeHwndToClientArea on every selection is what
// flashed the window, and skipping WM_PAINT left the virtual tree stale.
static void LayoutAnnotWindowInPlace(EditAnnotationsWindow* ew) {
    if (!ew || !ew->hwnd || !ew->mainLayout) {
        return;
    }
    Rect client = HwndClientRect(ew->hwnd);
    int dx = client.dx;
    int dy = client.dy;
    if (dx <= 0 || dy <= 0) {
        auto currBounds = ew->mainLayout->lastBounds;
        dx = currBounds.dx;
        dy = currBounds.dy;
    }
    if (dx <= 0 || dy <= 0) {
        return;
    }
    ew->DoLayout({dx, dy});
    // Native children are positioned with SWP_NOREDRAW. Newly shown
    // dropdowns (Icon on a Text annot) start at (0,0) over Contents;
    // invalidate the old parent rects (SetBounds) and paint once here.
    // No RDW_ERASE: a full-window erase flashed on every list selection
    // change. WM_PAINT already fills through a double-buffer.
    RedrawWindow(ew->hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
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
    // Save while the window is live: app shutdown writes settings before it
    // destroys tabs and their annotation editors.
    gGlobalPrefs->annotationsWindowSize = {dx, dy};
    HwndInvalidate(hwnd);
    if (false && mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    SetGrowingControlsToFit(this, dy);
    DoLayout({dx, dy});
}

static VirtText* CreateStatic(Str s = {}) {
    return NewVirtText({
        .s = s,
        .font = GetAppFont(),
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });
}

static VirtText* CreateAnnotOptLabel(Str s) {
    return NewVirtText({
        .s = s,
        .font = GetAppFont(),
        .align = VirtTextAlign::Right,
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });
}

static void AddAnnotOptRow(Table* t, int row, VirtText* label, ILayout* ctrl) {
    auto& lc = t->SetCell(row, 0, label);
    lc.alignH = CrossAxisAlign::Stretch;
    lc.alignV = CrossAxisAlign::CrossCenter;
    auto& rc = t->SetCell(row, 1, ctrl);
    rc.alignH = CrossAxisAlign::Stretch;
    rc.alignV = CrossAxisAlign::CrossCenter;
}

static void AddAnnotButton(HBox* row, VirtButton* b) {
    row->AddChild(b);
}

static HBox* NewAnnotButtonRow(MainAxisAlign align = MainAxisAlign::MainStart) {
    auto* hbox = new HBox();
    hbox->alignMain = align;
    hbox->alignCross = CrossAxisAlign::CrossCenter;
    hbox->gap = GetAppFont()->averageCharWidth;
    return hbox;
}

static VirtButton* NewAnnotButton(HWND hwnd, Str text, PlatformFont* font, bool isDefault) {
    auto* b = NewThemedButton(hwnd, text, font, isDefault);
    // tighter than Advanced Settings' 5pt vertical padding
    b->textPadding = DpiScaledInsets(2, 12);
    return b;
}

static void CreateMainLayout(EditAnnotationsWindow* ew) {
    HWND parent = ew->hwnd;
    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    PlatformFont* fnt = GetAppFont();
    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();

    {
        Edit::CreateArgs args;
        args.parent = parent;
        args.isMultiLine = false;
        args.withBorder = true;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto* c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkFunc0(FilterAnnotationsChanged, ew);
        ew->editFilter = c;
        vbox->AddChild(c);
    }

    {
        auto* w = new VirtListBox();
        w->dpi = ew->GetDpi();
        w->font = fnt;
        w->padding = DpiScaledInsets(4, 0);
        w->idealSizeLines = kMinAnnotListLines;
        w->multiSelect = true;
        auto* lbModel = new ListBoxModelStrings();
        w->SetModel(lbModel);
        w->onSelectionChanged = MkFunc0(ListBoxSelectionChanged, ew);
        w->onDrawItem = MkFunc1(DrawAnnotationListItem, ew);
        ew->listBox = w;
        vbox->AddChild(w);
        vbox->AddChild(new Spacer(0, DpiScale(4))); // 0.25rem between list and Delete
    }

    {
        auto* row = NewAnnotButtonRow(MainAxisAlign::MainEnd);
        auto* w = NewAnnotButton(parent, _TRA("Delete Annotation"), fnt, false);
        w->onClick = MkFunc0(ButtonDeleteHandler, ew);
        w->SetIsEnabled(false);
        ew->buttonDelete = w;
        AddAnnotButton(row, w);
        vbox->AddChild(row);
    }

    auto makeDropDown = [&]() -> DropDown* {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto* w = new DropDown();
        w->Create(args);
        return w;
    };
    auto makeTrackbar = [&](int rangeMin, int rangeMax) -> Trackbar* {
        Trackbar::CreateArgs args;
        args.parent = parent;
        args.rangeMin = rangeMin;
        args.rangeMax = rangeMax;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto* w = new Trackbar();
        w->Create(args);
        return w;
    };

    auto* meta = new Table();
    meta->SetSize(4, 2);
    meta->colGap = DpiScale(8);
    meta->rowGap = DpiScale(4);
    meta->padding = DpiScaledInsets(4, 0, 0, 0);
    ew->staticRect = CreateStatic();
    AddAnnotOptRow(meta, 0, CreateAnnotOptLabel(_TRA("Rect:")), ew->staticRect);
    ew->staticModificationDate = CreateStatic();
    AddAnnotOptRow(meta, 1, CreateAnnotOptLabel(_TRA("Date:")), ew->staticModificationDate);
    ew->staticAuthor = CreateStatic();
    AddAnnotOptRow(meta, 2, CreateAnnotOptLabel(_TRA("Author:")), ew->staticAuthor);
    ew->staticPopupLabel = CreateAnnotOptLabel(_TRA("Popup:"));
    ew->staticPopup = CreateStatic();
    AddAnnotOptRow(meta, 3, ew->staticPopupLabel, ew->staticPopup);
    vbox->AddChild(meta);

    {
        auto* w = CreateStatic(_TRA("Contents:"));
        ew->staticContents = w;
        w->padding = DpiScaledInsets(4, 0, 0, 0);
        vbox->AddChild(w);
        vbox->AddChild(new Spacer(0, DpiScale(4))); // 0.25rem between label and edit
    }

    {
        Edit::CreateArgs args;
        args.parent = parent;
        args.isMultiLine = true;
        args.idealSizeLines = kPreferredContentsLines;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto* w = new Edit();
        HWND hwnd = w->Create(args);
        ReportIf(!hwnd);
        w->onTextChanged = MkFunc0(ContentsChanged, ew);
        // flush Contents on blur (EN_KILLFOCUS); do not fold this into
        // onTextChanged — that must stay EN_CHANGE-only (Advanced Settings UAF)
        w->onKillFocus = MkFunc0(ContentsKillFocus, ew);
        ew->editContents = w;
        vbox->AddChild(w);
    }

    auto* opts = new Table();
    opts->SetSize(11, 2);
    opts->colGap = DpiScale(8);
    opts->rowGap = DpiScale(4);
    opts->padding = DpiScaledInsets(4, 0, 0, 0);
    int optRow = 0;

    ew->staticTextAlignment = CreateAnnotOptLabel(_TRA("Text Alignment:"));
    ew->dropDownTextAlignment = makeDropDown();
    ew->dropDownTextAlignment->SetItemsSeqStrings(gQuaddingNames);
    ew->dropDownTextAlignment->onSelectionChanged = MkFunc0(TextAlignmentSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticTextAlignment, ew->dropDownTextAlignment);

    ew->staticTextFont = CreateAnnotOptLabel(_TRA("Text Font:"));
    ew->dropDownTextFont = makeDropDown();
    ew->dropDownTextFont->SetItemsSeqStrings(gQuaddingNames);
    ew->dropDownTextFont->onSelectionChanged = MkFunc0(TextFontSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticTextFont, ew->dropDownTextFont);

    ew->staticTextSize = CreateAnnotOptLabel(_TRA("Text Size:"));
    ew->trackbarTextSize = makeTrackbar(8, 36);
    ew->trackbarTextSize->onPositionChanging = MkFunc1(TextFontSizeChanging, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticTextSize, ew->trackbarTextSize);

    ew->staticTextColor = CreateAnnotOptLabel(_TRA("Text Color:"));
    ew->dropDownTextColor = makeDropDown();
    ew->dropDownTextColor->SetItemsSeqStrings(gColors);
    ew->dropDownTextColor->onSelectionChanged = MkFunc0(TextColorSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticTextColor, ew->dropDownTextColor);

    ew->staticLineStart = CreateAnnotOptLabel(_TRA("Line Start:"));
    ew->dropDownLineStart = makeDropDown();
    ew->dropDownLineStart->onSelectionChanged = MkFunc0(LineStartSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticLineStart, ew->dropDownLineStart);

    ew->staticLineEnd = CreateAnnotOptLabel(_TRA("Line End:"));
    ew->dropDownLineEnd = makeDropDown();
    ew->dropDownLineEnd->onSelectionChanged = MkFunc0(LineEndSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticLineEnd, ew->dropDownLineEnd);

    ew->staticIcon = CreateAnnotOptLabel(_TRA("Icon:"));
    ew->dropDownIcon = makeDropDown();
    ew->dropDownIcon->onSelectionChanged = MkFunc0(IconSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticIcon, ew->dropDownIcon);

    ew->staticBorder = CreateAnnotOptLabel(_TRA("Border:"));
    ew->trackbarBorder = makeTrackbar(kBorderWidthMin, kBorderWidthMax);
    ew->trackbarBorder->onPositionChanging = MkFunc1(BorderWidthChanging, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticBorder, ew->trackbarBorder);

    ew->staticColor = CreateAnnotOptLabel(_TRA("Color:"));
    ew->dropDownColor = makeDropDown();
    ew->dropDownColor->SetItemsSeqStrings(gColors);
    ew->dropDownColor->onSelectionChanged = MkFunc0(ColorSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticColor, ew->dropDownColor);

    ew->staticInteriorColor = CreateAnnotOptLabel(_TRA("Interior Color:"));
    ew->dropDownInteriorColor = makeDropDown();
    ew->dropDownInteriorColor->SetItemsSeqStrings(gColors);
    ew->dropDownInteriorColor->onSelectionChanged = MkFunc0(InteriorColorSelectionChanged, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticInteriorColor, ew->dropDownInteriorColor);

    ew->staticOpacity = CreateAnnotOptLabel(_TRA("Opacity:"));
    ew->trackbarOpacity = makeTrackbar(0, 255);
    ew->trackbarOpacity->onPositionChanging = MkFunc1(OpacityChanging, ew);
    AddAnnotOptRow(opts, optRow++, ew->staticOpacity, ew->trackbarOpacity);

    ReportIf(optRow != 11);
    vbox->AddChild(opts);

    {
        auto* row = NewAnnotButtonRow();
        auto* saveAtt = NewAnnotButton(parent, _TRA("Save..."), fnt, false);
        saveAtt->onClick = MkFunc0(ButtonSaveAttachment, ew);
        ew->buttonSaveAttachment = saveAtt;
        AddAnnotButton(row, saveAtt);
        auto* embed = NewAnnotButton(parent, _TRA("Embed..."), fnt, false);
        embed->onClick = MkFunc0(ButtonEmbedAttachment, ew);
        ew->buttonEmbedAttachment = embed;
        AddAnnotButton(row, embed);
        vbox->AddChild(row);
    }

    {
        // leftover window height sits here so Contents and the list stay put
        auto* w = new Spacer(0, 0);
        vbox->AddChild(w, 1);
    }

    {
        auto* row = NewAnnotButtonRow();
        // text set by UpdateSaveButtonLabels once tab is attached
        auto* saveCur = NewAnnotButton(parent, _TRA("Save changes to existing PDF"), fnt, true);
        saveCur->SetIsEnabled(false); // only enabled if there are changes
        saveCur->onClick = MkFunc0(ButtonSaveToCurrentPDFHandler, ew);
        ew->buttonSaveToCurrentFile = saveCur;
        AddAnnotButton(row, saveCur);
        row->AddChild(new Spacer(0, 0), 1);
        auto* saveNew = NewAnnotButton(parent, _TRA("Save changes to a new PDF"), fnt, false);
        saveNew->SetIsEnabled(false); // only enabled if there are changes
        saveNew->onClick = MkFunc0(ButtonSaveToNewFileHandler, ew);
        ew->buttonSaveToNewFile = saveNew;
        AddAnnotButton(row, saveNew);
        vbox->AddChild(row);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    ew->mainLayout = padding;
    // WindowBase owns and lays out `layout`; mainLayout is the same tree, kept
    // as an ILayout* for its lastBounds
    ew->layout = padding;
    HidePerAnnotControls(ew);
}

static void LimitEditAnnotationsClientSizeToScreen(HWND hwnd, HWND hwndRelative, Size& size) {
    // nullptr hwnd so we use the nearest monitor, not the primary
    Rect work = GetWorkAreaRect(HwndWindowRect(hwndRelative), nullptr);
    WINDOWINFO wi{};
    wi.cbSize = sizeof(wi);
    int nonClientDx = 0;
    int nonClientDy = 0;
    if (GetWindowInfo(hwnd, &wi)) {
        nonClientDx = RectDx(wi.rcWindow) - RectDx(wi.rcClient);
        nonClientDy = RectDy(wi.rcWindow) - RectDy(wi.rcClient);
    }
    // a newly created hidden window can report rcWindow == rcClient
    if (nonClientDx <= 0 || nonClientDy <= 0) {
        RECT rc{0, 0, 100, 100};
        DWORD style = hwnd ? (DWORD)GetWindowLongW(hwnd, GWL_STYLE) : WS_OVERLAPPEDWINDOW;
        DWORD exStyle = hwnd ? (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE) : 0;
        AdjustWindowRectEx(&rc, style, FALSE, exStyle);
        nonClientDx = (rc.right - rc.left) - 100;
        nonClientDy = (rc.bottom - rc.top) - 100;
    }
    int maxClientDx = std::max(work.dx - nonClientDx, 1);
    int maxClientDy = std::max(work.dy - nonClientDy, 1);
    size.dx = std::min(size.dx, maxClientDx);
    size.dy = std::min(size.dy, maxClientDy);
}

static HWND AnnotEditorRelativeHwnd(EditAnnotationsWindow* ew) {
    if (ew && ew->tab && ew->tab->win && ew->tab->win->hwndFrame) {
        return ew->tab->win->hwndFrame;
    }
    return ew ? ew->hwnd : nullptr;
}

// Size the HWND to a client size that fits the work area. The list stays at
// clamp(n, kMinAnnotListLines, kMaxAnnotListLines) rows and Contents at
// kPreferredContentsLines.
static void RelayoutEditAnnotationsWindow(EditAnnotationsWindow* ew, int clientDx, int clientDy) {
    if (!ew || !ew->hwnd || !ew->mainLayout) {
        return;
    }
    Size size{clientDx, clientDy};
    if (size.dx <= 0 || size.dy <= 0) {
        Rect cr = HwndClientRect(ew->hwnd);
        size = {cr.dx, cr.dy};
    }
    HWND rel = AnnotEditorRelativeHwnd(ew);
    if (rel) {
        LimitEditAnnotationsClientSizeToScreen(ew->hwnd, rel, size);
    }
    if (size.dx <= 0 || size.dy <= 0) {
        return;
    }
    SetGrowingControlsToFit(ew, size.dy);
    ResizeHwndToClientArea(ew->hwnd, size.dx, size.dy, false);
    ew->DoLayout({size.dx, size.dy});
}

static void ClampEditAnnotationsWindowToWorkArea(HWND hwnd, HWND hwndRelative) {
    if (!hwnd) {
        return;
    }
    Rect r = HwndWindowRect(hwnd);
    Rect probe = hwndRelative ? HwndWindowRect(hwndRelative) : r;
    Rect work = GetWorkAreaRect(probe, nullptr);
    if (work.IsEmpty()) {
        return;
    }
    if (r.dx > work.dx) {
        r.dx = work.dx;
    }
    if (r.dy > work.dy) {
        r.dy = work.dy;
    }
    r = ShiftRectToWorkArea(r, nullptr, true);
    SetWindowPos(hwnd, nullptr, r.x, r.y, r.dx, r.dy, SWP_NOZORDER);
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
    // OnSize sizes the list / Contents to their fixed line counts before DoLayout
    ew->autoLayout = false;
    // Esc normally does not close because the user may be editing text
    // (issue #5934). EscToExit is the explicit opt-in to close anyway.
    ew->closeOnEsc = gGlobalPrefs->escToExit;
    ew->closeOnCtrlW = true;
    ew->onClose = MkFunc1Void(OnClose);
    ew->onDestroy = MkFunc1Void(OnDestroy);
    ew->onSize = MkMethod1<EditAnnotationsWindow, WindowBase::SizeEvent*, &EditAnnotationsWindow::OnSize>(ew);
    ew->onFocus = MkMethod1<EditAnnotationsWindow, WindowBase::FocusEvent*, &EditAnnotationsWindow::OnFocus>(ew);
    ew->onKeyDown = MkMethod1<EditAnnotationsWindow, KeyEvent*, &EditAnnotationsWindow::OnKeyDown>(ew);
    ew->onTimer = MkMethod1<EditAnnotationsWindow, WindowBase::TimerEvent*, &EditAnnotationsWindow::OnTimer>(ew);
    ew->onWndProc = MkFunc1Void(OnAnnotWndProc);
    CreateCustomArgs args;
    HMODULE h = GetModuleHandleW(nullptr);
    args.icon = LoadIconW(h, MAKEINTRESOURCEW(GetAppIconID()));
    // mainWindow->isDialog = true;
    args.bgColor = DarkModeDialogBgColor();

    args.title = str::JoinTemp(_TRA("Annotations"), StrL(": "), tab->GetTabTitle());
    args.visible = false;
    args.font = GetAppFont();
    // Parent + native children (combos, Contents) paint off-screen so
    // arrow-key selection changes don't flash through erase-then-paint.
    args.exStyle = WS_EX_COMPOSITED;

    // PositionCloseTo(w, args->hwndRelatedTo);
    // Size winSize = {w->initialSize.dx, w->initialSize.Height};
    // winSize = HwndLimitSizeToScreen(args->hwndRelatedTo, winSize);
    // w->initialSize = {winSize.dx, winSize.dy};
    ew->CreateCustom(args);

    CreateMainLayout(ew);
    ew->tab = tab;
    tab->editAnnotsWindow = ew;
    UpdateSaveButtonLabels(ew);

    // so CollectPriorityAnnotPages includes this annot's page (context menu)
    Annotation* prevSel = tab->selectedAnnotation;
    if (annot) {
        tab->selectedAnnotation = annot;
    }
    UpdateAnnotationsList(ew);
    tab->selectedAnnotation = prevSel;

    Rect lastPos = tab->lastEditAnnotsWindowPos;
    Size lastSize = gGlobalPrefs->annotationsWindowSize;
    if (lastSize.IsEmpty() && !lastPos.IsEmpty()) {
        lastSize = {lastPos.dx, lastPos.dy};
    }
    if (lastSize.IsEmpty()) {
        lastSize = {520, 720};
        // size our editor window to be the same height as main window
        // TODO: this is slightly less that wanted
        HWND hwnd = tab->win->hwndCanvas;
        auto rc = HwndClientRect(hwnd);
        if (rc.dy > 0) {
            lastSize.dy = rc.dy;
        }
    }

    if (lastPos.IsEmpty()) {
        RelayoutEditAnnotationsWindow(ew, lastSize.dx, lastSize.dy);
        HwndPositionToTheRightOf(ew->hwnd, tab->win->hwndFrame);
        ClampEditAnnotationsWindowToWorkArea(ew->hwnd, tab->win->hwndFrame);
    } else {
        RelayoutEditAnnotationsWindow(ew, lastSize.dx, lastSize.dy);
        // pass nullptr for hwnd so ShiftRectToWorkArea uses the saved rect
        // to find the correct monitor (not the monitor the hwnd is currently on)
        Rect r = HwndWindowRect(ew->hwnd);
        r.x = lastPos.x;
        r.y = lastPos.y;
        r = ShiftRectToWorkArea(r, nullptr, true);
        SetWindowPos(ew->hwnd, nullptr, r.x, r.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
        ClampEditAnnotationsWindowToWorkArea(ew->hwnd, tab->win->hwndFrame);
    }
    if (!annot) {
        annot = ew->tab->selectedAnnotation;
    }
    if (!annot && len(ew->visibleAnnots) > 0) {
        int curr = 0;
        DisplayModel* dm = tab->AsFixed();
        if (dm) {
            curr = dm->CurrentPageNo();
        }
        for (Annotation* a : ew->visibleAnnots) {
            if (curr < 1 || a->pageNo == curr) {
                annot = a;
                break;
            }
        }
        if (!annot) {
            annot = ew->visibleAnnots[0];
        }
    }
    ew->skipGoToPage = (annot != nullptr);
    bool isNew = false;
    if (annot) {
        isNew = annot != ew->tab->win->annotationUnderCursor;
        SetSelectedAnnotation(tab, annot, isNew, focus);
    }
    DarkModeApplyToNotifyWindowAndEraseBg(ew->hwnd);

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    ew->SetIsVisible(true);
    // Showing the top-level window can clear focus set while it was hidden.
    // Restore the same target UpdateUIForSelectedAnnotation chose so the first
    // Tab starts from the annotation list (issue #6036).
    if (annot && (focus == EditAnnotFocus::Edit ||
                  (focus == EditAnnotFocus::Default && isNew && annot->type == AnnotationType::FreeText))) {
        HwndSetFocus(ew->editContents->hwnd);
    } else {
        FocusAnnotationsList(ew);
    }
}

// Resize the annotation editor to clientDy and report list / Contents sizes
// for tests/issue-3769.ts and tests/issue-5834.ts. selectItem is
// 1-based; 0 leaves the selection alone; -1 selects every row. selectLast
// (1-based) with selectItem > 0 selects that inclusive range (issue #5976).
TempStr AnnotEditorLayoutResultTemp(int clientDy, int selectItem, int* exitCodeOut, int selectLast) {
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

    // Re-showing an existing editor steals focus while GUI tests query resize
    // state, which can disturb mouse capture and overwrite the drag result.
    if (!tab->editAnnotsWindow) {
        ShowEditAnnotationsWindow(tab, nullptr);
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    if (!ew || !ew->hwnd || !ew->listBox) {
        return finish(StrL("ERROR no-editor"), 1);
    }

    if (selectItem < 0) {
        if (len(ew->annotations) == 0) {
            return finish(StrL("ERROR no-annot"), 1);
        }
        ew->listBox->SelectAll();
        ApplyListSelectionNow(ew);
    } else if (selectItem > 0) {
        int idx = selectItem - 1;
        if (!ew->annotations.isValidIndex(idx)) {
            return finish(fmt("ERROR no-annot item=%d n=%d", selectItem, len(ew->annotations)), 1);
        }
        int last = selectLast > 0 ? selectLast - 1 : idx;
        if (selectLast > 0 && !ew->annotations.isValidIndex(last)) {
            return finish(fmt("ERROR no-annot item=%d n=%d", selectLast, len(ew->annotations)), 1);
        }
        if (last != idx) {
            ew->listBox->SelectRange(idx, last);
        } else {
            ew->listBox->SetCurrentSelection(idx);
        }
        ApplyListSelectionNow(ew);
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
    Rect contentsR{};
    if (ew->editContents && ew->editContents->IsVisible() && ew->editContents->hwnd) {
        contentsR = HwndMapRectToWindow(HwndClientRect(ew->editContents->hwnd), ew->editContents->hwnd, ew->hwnd);
        contentsDy = contentsR.dy;
    }
    Rect iconR{};
    int iconVis = 0;
    if (ew->dropDownIcon && ew->dropDownIcon->IsVisible() && ew->dropDownIcon->hwnd) {
        iconVis = 1;
        iconR = HwndMapRectToWindow(HwndClientRect(ew->dropDownIcon->hwnd), ew->dropDownIcon->hwnd, ew->hwnd);
    }
    int gapBelow = cr.dy - (listR.y + listR.dy);
    int sel = ew->listBox->GetCurrentSelection();
    int n = len(ew->annotations);
    int selCount = ew->listBox->SelectedCount();
    out.Append(
        fmt("OK windowDy=%d listDy=%d contentsDy=%d gapBelow=%d sel=%d n=%d selCount=%d contents=%d,%d,%d,%d "
            "iconVis=%d icon=%d,%d,%d,%d",
            cr.dy, listR.dy, contentsDy, gapBelow, sel, n, selCount, contentsR.x, contentsR.y, contentsR.dx,
            contentsR.dy, iconVis, iconR.x, iconR.y, iconR.dx, iconR.dy));
    out.Append(fmt(" ignoreReload=%d reloadOnFocus=%d resizeRerenderPending=%d", (int)tab->ignoreNextAutoReload,
                   (int)tab->reloadOnFocus, (int)(win->annotationResizeRerenderTimer != 0)));
    Annotation* annot = tab->selectedAnnotation;
    if (annot) {
        Rect annotRect = tab->AsFixed()->CvtToScreen(annot->pageNo, GetRect(annot));
        out.Append(fmt(" annotType=%d annotRect=%d,%d,%d,%d canResize=%d", (int)annot->type, annotRect.x, annotRect.y,
                       annotRect.dx, annotRect.dy, (int)AnnotationCanBeResized(annot->type)));
    }
    return finish({}, 0);
}
