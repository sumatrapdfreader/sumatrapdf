/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

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
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Toolbar.h"
#include "FilterUtil.h"
#include "EditAnnotations.h"
#include "ToolbarInternal.h"

#include "AnnotFilterToolbar.h"

constexpr const WCHAR* kAnnotFilterListClassName = L"SumatraAnnotFilterList";
constexpr UINT_PTR kSelectionDebounceTimerId = 1;
constexpr int kSelectionDebounceMs = 300;
constexpr int kMaxListLines = 12;

struct AnnotFilterToolbar;

static void HideList(AnnotFilterToolbar*, bool dismissed, bool apply = true);
static void ShowList(AnnotFilterToolbar*);
static void RebuildList(AnnotFilterToolbar*);
static void UpdateCue(AnnotFilterToolbar*);
static void ApplySelectionNow(AnnotFilterToolbar*);
static void ScheduleSelection(AnnotFilterToolbar*);
static void EnsureListHost(AnnotFilterToolbar*);
static void PositionList(AnnotFilterToolbar*);
static AnnotFilterToolbar* GetOrCreate(MainWindow*);

struct AnnotFilterToolbar {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    VirtHost* listHost = nullptr;
    VirtListBox* listBox = nullptr;
    Vec<Annotation*> annotations;
    Vec<Annotation*> visibleAnnots;
    StrVec filterWords;
    Vec<u8> filterHlScratch;
    Func1List<MainWindow*> onWindowMoved;
    bool listDismissed = false;
    // bumped on each list caret change / cancel so a late uitask apply is dropped
    int selEpoch = 0;
    int applyEpoch = 0;
};

static bool IsListNavKey(int vkey) {
    return vkey == VK_UP || vkey == VK_DOWN || vkey == VK_PRIOR || vkey == VK_NEXT || vkey == VK_HOME || vkey == VK_END;
}

static WindowTab* FilterTab(AnnotFilterToolbar* f) {
    return f && f->win ? f->win->CurrentTab() : nullptr;
}

static void UpdateCue(AnnotFilterToolbar* f) {
    if (!f || !f->edit) {
        return;
    }
    f->edit->SetCue(fmt(_TRA("filter %d annotations").s, len(f->annotations)));
}

static void LoadAnnotations(AnnotFilterToolbar* f) {
    if (!f || !f->win) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    f->annotations.Reset();
    if (!tab) {
        return;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    EngineMupdfGetLoadedAnnotations(engine, f->annotations);
}

static Annotation* VisibleAnnotAt(AnnotFilterToolbar* f, int idx) {
    if (!f || !f->visibleAnnots.isValidIndex(idx)) {
        return nullptr;
    }
    return f->visibleAnnots[idx];
}

static void ApplyListColors(AnnotFilterToolbar* f) {
    if (!f || !f->listBox) {
        return;
    }
    f->listBox->SetColor(kColListBg, ThemeWindowControlBackgroundColor());
    f->listBox->SetColor(kColListText, ThemeWindowTextColor());
}

static void RebuildList(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    Annotation* caret = nullptr;
    int prevScrollY = 0;
    if (f->listBox) {
        caret = VisibleAnnotAt(f, f->listBox->GetCurrentSelection());
        prevScrollY = f->listBox->scrollY;
    }
    f->visibleAnnots.Reset();
    auto* model = new ListBoxModelStrings();
    for (Annotation* annot : f->annotations) {
        if (!AnnotMatchesFilter(annot, f->filterWords)) {
            continue;
        }
        f->visibleAnnots.Append(annot);
        model->strings.Append(AnnotationReadableNameTemp(annot->type));
    }
    if (!f->listBox) {
        delete model;
        UpdateCue(f);
        return;
    }
    f->listBox->SetModel(model);
    f->listBox->ScrollTo(prevScrollY);
    int idx = caret ? f->visibleAnnots.Find(caret) : -1;
    if (idx < 0) {
        Annotation* keep = FilterTab(f) ? FilterTab(f)->selectedAnnotation : nullptr;
        idx = keep ? f->visibleAnnots.Find(keep) : -1;
    }
    if (idx >= 0) {
        f->listBox->SetCurrentSelection(idx);
    }
    UpdateCue(f);
    if (f->listHost && f->listHost->IsVisible()) {
        PositionList(f);
        f->listHost->Invalidate(false);
    }
}

static void CancelPendingSelection(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    f->selEpoch++;
    if (f->listHost) {
        f->listHost->KillTimer((int)kSelectionDebounceTimerId);
    }
}

static void ApplySelectionNow(AnnotFilterToolbar* f) {
    CancelPendingSelection(f);
    if (!f || !f->listBox) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    if (!tab) {
        return;
    }
    int itemNo = f->listBox->GetCurrentSelection();
    if (itemNo < 0) {
        return;
    }
    Annotation* annot = VisibleAnnotAt(f, itemNo);
    if (!annot || annot == tab->selectedAnnotation) {
        return;
    }
    SetSelectedAnnotation(tab, annot);
}

static void PostedApplySelection(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    AnnotFilterToolbar* f = win->annotFilterToolbar;
    if (!f || f->selEpoch != f->applyEpoch) {
        return;
    }
    ApplySelectionNow(f);
}

static void ScheduleSelection(AnnotFilterToolbar* f) {
    if (!f || !f->listBox) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    if (!tab) {
        return;
    }
    int itemNo = f->listBox->GetCurrentSelection();
    Annotation* annot = itemNo >= 0 ? VisibleAnnotAt(f, itemNo) : nullptr;
    if (annot == tab->selectedAnnotation) {
        CancelPendingSelection(f);
        return;
    }
    CancelPendingSelection(f);
    if (!f->listHost || !f->win) {
        f->applyEpoch = f->selEpoch;
        if (f->win) {
            uitask::Post(MkFunc0(PostedApplySelection, f->win), "ApplyAnnotFilterSelection");
        }
        return;
    }
    f->listHost->SetTimer((int)kSelectionDebounceTimerId, kSelectionDebounceMs);
}

static void OnListTimer(AnnotFilterToolbar* f, int timerId) {
    if (!f || !f->win || timerId != (int)kSelectionDebounceTimerId) {
        return;
    }
    if (f->listHost) {
        f->listHost->KillTimer((int)kSelectionDebounceTimerId);
    }
    f->applyEpoch = f->selEpoch;
    uitask::Post(MkFunc0(PostedApplySelection, f->win), "ApplyAnnotFilterSelection");
}

static void OnListSelectionChanged(AnnotFilterToolbar* f) {
    ScheduleSelection(f);
}

static void OnListDoubleClick(AnnotFilterToolbar* f) {
    ApplySelectionNow(f);
    HideList(f, true, false);
}

static void OnListDrawItem(AnnotFilterToolbar* f, VirtListBox::DrawItemEvent* ev) {
    if (!f || !ev) {
        return;
    }
    Annotation* annot = VisibleAnnotAt(f, ev->itemIndex);
    if (!annot || !ev->listBox) {
        return;
    }
    DrawAnnotationListRow(ev->gfx, ev->listBox->font, ev->itemRect, annot, f->filterWords, f->filterHlScratch,
                          ev->listBox->GetColor(kColListBg), ev->listBox->GetColor(kColListText), ev->selected);
}

static void PaintListBg(AnnotFilterToolbar*, VirtHostPaintEvent* ev) {
    ev->gfx->FillRect(ev->clientRect, ThemeWindowControlBackgroundColor());
    ev->gfx->DrawRect(ev->clientRect, ThemeEdgeColor(), 1);
}

static void EnsureListHost(AnnotFilterToolbar* f) {
    if (!f || !f->win || f->listHost) {
        return;
    }
    VirtHost::CreateArgs args;
    args.parent = f->win->hwndFrame;
    args.className = WStr(kAnnotFilterListClassName);
    args.isPopup = true;
    args.visible = false;
    args.noActivate = true;
    args.userData = f;
    args.bgColor = ThemeWindowControlBackgroundColor();
    args.initialSize = {100, 100};

    f->listHost = VirtHost::Create(args);
    if (!f->listHost) {
        return;
    }
    f->listHost->onPaintBackground = MkFunc1(PaintListBg, f);
    f->listHost->onTimer = MkFunc1(OnListTimer, f);

    auto* lb = new VirtListBox();
    lb->dpi = DpiGetForHwnd(f->win->hwndFrame);
    lb->font = GetAppFont();
    lb->padding = DpiScaledInsets(1, 1);
    lb->idealSizeLines = 1;
    lb->SetModel(new ListBoxModelStrings());
    lb->onSelectionChanged = MkFunc0(OnListSelectionChanged, f);
    lb->onDoubleClick = MkFunc0(OnListDoubleClick, f);
    lb->onDrawItem = MkFunc1(OnListDrawItem, f);
    lb->SetFlag(vwfFocusable, false);
    f->listBox = lb;
    ApplyListColors(f);
    f->listHost->SetLayout(lb);
}

static void PositionList(AnnotFilterToolbar* f) {
    if (!f || !f->edit || !f->listHost || !f->listBox) {
        return;
    }
    Rect editScreen = HwndWindowRect(f->edit->hwnd);
    if (editScreen.IsEmpty()) {
        return;
    }
    int n = f->listBox->ItemsCount();
    int lines = n;
    if (lines < 1) {
        lines = 1;
    }
    if (lines > kMaxListLines) {
        lines = kMaxListLines;
    }
    f->listBox->idealSizeLines = lines;
    int minDx = DpiScale(280);
    f->listBox->idealSizeDx = std::max(editScreen.dx, minDx);
    Size sz = f->listBox->GetIdealSize();
    int w = std::max(sz.dx, f->listBox->idealSizeDx);
    int h = sz.dy;
    if (w < 1 || h < 1) {
        return;
    }
    int x = editScreen.x;
    if (IsUIRtl()) {
        x = editScreen.x + editScreen.dx - w;
    }
    int y = editScreen.y + editScreen.dy;
    Rect work = GetWorkAreaRect(editScreen, f->win->hwndFrame);
    bool above = ToolbarAtBottom();
    if (!work.IsEmpty()) {
        if (!above && y + h > work.y + work.dy) {
            above = true;
        }
        if (above) {
            y = editScreen.y - h;
            if (y < work.y) {
                y = work.y;
            }
        }
        if (x + w > work.x + work.dx) {
            x = work.x + work.dx - w;
        }
        if (x < work.x) {
            x = work.x;
        }
    } else if (above) {
        y = editScreen.y - h;
    }
    f->listHost->SetPos({x, y, w, h}, true);
}

static void HideList(AnnotFilterToolbar* f, bool dismissed, bool apply) {
    if (!f) {
        return;
    }
    f->listDismissed = dismissed;
    if (apply && f->listHost && f->listHost->IsVisible()) {
        ApplySelectionNow(f);
    } else {
        CancelPendingSelection(f);
    }
    if (f->listHost && f->listHost->IsVisible()) {
        f->listHost->Show(false);
    }
}

static void ShowList(AnnotFilterToolbar* f) {
    if (!f || !f->edit || !f->edit->IsFocused()) {
        return;
    }
    f->listDismissed = false;
    EnsureListHost(f);
    if (!f->listHost) {
        return;
    }
    LoadAnnotations(f);
    RebuildList(f);
    PositionList(f);
}

static bool ListIsShown(AnnotFilterToolbar* f) {
    return f && f->listHost && f->listHost->IsVisible() && !f->listDismissed;
}

static void EnsureListShown(AnnotFilterToolbar* f) {
    if (ListIsShown(f)) {
        return;
    }
    ShowList(f);
}

static bool FilterHomeEndMovesList(AnnotFilterToolbar* f, int vkey, bool isCtrl) {
    if (vkey != VK_HOME && vkey != VK_END) {
        return true;
    }
    if (isCtrl) {
        return true;
    }
    if (!f->edit) {
        return true;
    }
    int selStart = 0, selEnd = 0;
    f->edit->GetSelection(selStart, selEnd);
    int textLen = f->edit->GetTextLen();
    bool toEnd = (vkey == VK_END);
    return (selStart == selEnd) && (toEnd ? selEnd == textLen : selStart == 0);
}

static void HandleEscape(AnnotFilterToolbar* f) {
    if (!f || !f->edit) {
        return;
    }
    if (len(f->edit->GetTextTemp()) > 0) {
        f->edit->SetText({});
        return;
    }
    HideList(f, false);
    if (f->win && f->win->hwndCanvas) {
        HwndSetFocus(f->win->hwndCanvas);
    }
}

static void OnFilterTextChanged(AnnotFilterToolbar* f) {
    if (!f || !f->edit) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    Annotation* keep = tab ? tab->selectedAnnotation : nullptr;
    f->filterWords.Reset();
    SplitFilterToWords(f->edit->GetTextTemp(), f->filterWords);
    if (f->edit->IsFocused()) {
        ShowList(f);
    } else {
        LoadAnnotations(f);
        RebuildList(f);
    }
    if (!f->listBox) {
        return;
    }
    int idx = keep ? f->visibleAnnots.Find(keep) : -1;
    if (idx >= 0) {
        f->listBox->SetCurrentSelection(idx);
        return;
    }
    if (len(f->visibleAnnots) > 0) {
        f->listBox->SetCurrentSelection(0);
        ScheduleSelection(f);
        return;
    }
    if (tab) {
        SetSelectedAnnotation(tab, nullptr);
    }
}

static void OnFilterFocus(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    StartLoadingAnnotationsForUi(tab);
    ShowList(f);
}

static void PostedHideIfUnfocused(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    AnnotFilterToolbar* f = win->annotFilterToolbar;
    if (!f || !f->edit) {
        return;
    }
    if (f->edit->IsFocused()) {
        return;
    }
    HideList(f, false);
}

static void OnFilterKillFocus(AnnotFilterToolbar* f) {
    if (!f || !f->win) {
        return;
    }
    uitask::Post(MkFunc0(PostedHideIfUnfocused, f->win), "HideAnnotFilterList");
}

static void OnFilterWndProc(AnnotFilterToolbar* f, ControlBase::WndProcEvent* ev) {
    if (!f || !ev) {
        return;
    }
    if (ev->msg == WM_CHAR &&
        (ev->wparam == VK_ESCAPE || ev->wparam == VK_RETURN || ev->wparam == '\r' || ev->wparam == '\n')) {
        ev->didHandle = true;
        ev->result = 0;
        return;
    }
    if (ev->msg == WM_KEYDOWN) {
        int vkey = (int)ev->wparam;
        bool isCtrl = IsCtrlPressed();
        bool isShift = IsShiftPressed();
        bool isAlt = IsAltPressed();
        if (vkey == VK_ESCAPE) {
            HandleEscape(f);
            ev->didHandle = true;
            ev->result = 0;
            return;
        }
        if (vkey == VK_RETURN) {
            ApplySelectionNow(f);
            ev->didHandle = true;
            ev->result = 0;
            return;
        }
        if (IsListNavKey(vkey) && ((vkey != VK_HOME && vkey != VK_END) || FilterHomeEndMovesList(f, vkey, isCtrl))) {
            EnsureListShown(f);
            if (!f->listBox || f->listBox->ItemsCount() == 0) {
                ev->didHandle = true;
                ev->result = 0;
                return;
            }
            VirtKeyEvent ke;
            ke.vkey = vkey;
            ke.isCtrl = isCtrl;
            ke.isShift = isShift;
            ke.isAlt = isAlt;
            f->listBox->OnKeyDown(&ke);
            ev->didHandle = ke.didHandle;
            ev->result = 0;
            return;
        }
    }
    // Create() installs Edit::WndProc as onWndProc; we replaced it, so forward
    // NC paint / frame / Ctrl+Backspace to the edit itself.
    if (f->edit) {
        f->edit->WndProc(ev);
    }
}

static AnnotFilterToolbar* GetOrCreate(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    if (win->annotFilterToolbar) {
        return win->annotFilterToolbar;
    }
    auto* f = new AnnotFilterToolbar();
    f->win = win;
    f->onWindowMoved = MkFunc1Void(RepositionAnnotFilterList);
    win->RegisterOnWindowMoved(&f->onWindowMoved);
    win->annotFilterToolbar = f;
    return f;
}

static int FilterEditPadL() {
    return UiEdgeDx();
}

static int FilterEditPadR() {
    return FilterEditPadL() + DpiScale(4);
}

Edit* CreateAnnotFilterEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    AnnotFilterToolbar* f = GetOrCreate(win);
    Edit::CreateArgs args;
    args.parent = win->hwndToolbar;
    args.font = font;
    args.isRtl = IsUIRtl();
    args.withFrame = true;
    args.noTheme = true;
    args.selectAllOnFocus = true;
    args.centerTextVert = true;
    args.marginLeft = FilterEditPadL();
    args.marginRight = FilterEditPadR();
    auto* e = new Edit();
    e->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    e->Create(args);
    e->mapRtlX = true;
    e->SetIdealWidthFromText(fmt(_TRA("filter %d annotations").s, 999), DpiScale(8));
    e->idealDy = iconDy;
    e->SetCue(fmt(_TRA("filter %d annotations").s, 0));
    if (f) {
        f->edit = e;
        e->onTextChanged = MkFunc0(OnFilterTextChanged, f);
        e->onFocus = MkFunc0(OnFilterFocus, f);
        e->onKillFocus = MkFunc0(OnFilterKillFocus, f);
        e->onWndProc = MkFunc1(OnFilterWndProc, f);
        UpdateCue(f);
    }
    return e;
}

void UnbindAnnotFilterEdit(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    HideList(f, false, false);
    f->edit = nullptr;
    ToolbarVirt* tb = win->toolbarVirt;
    if (tb) {
        tb->annotFilterEdit = nullptr;
    }
}

void HideAnnotFilterList(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (f) {
        HideList(f, false);
    }
}

void RepositionAnnotFilterList(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->listHost || !f->listHost->IsVisible()) {
        return;
    }
    if (!f->edit || !f->edit->IsFocused()) {
        HideList(f, false);
        return;
    }
    PositionList(f);
}

void SetAnnotFilterEditVisible(MainWindow* win, bool visible) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    Edit* edit = tb ? tb->annotFilterEdit : nullptr;
    if (edit) {
        edit->SetVisibility(visible ? Visibility::Visible : Visibility::Collapse);
    }
    if (!visible) {
        HideAnnotFilterList(win);
    }
}

void UpdateAnnotFilterToolbar(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->edit) {
        return;
    }
    ApplyListColors(f);
    f->edit->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    if (!win->pdfAnnotationsToolbarEnabled) {
        HideList(f, false);
        return;
    }
    bool filterFocused = f->edit && f->edit->IsFocused();
    if (f->listBox && !filterFocused) {
        WindowTab* tab = FilterTab(f);
        Annotation* keep = tab ? tab->selectedAnnotation : nullptr;
        int idx = keep ? f->visibleAnnots.Find(keep) : -1;
        if (idx >= 0 && f->listBox->GetCurrentSelection() != idx) {
            f->listBox->SetCurrentSelection(idx);
        }
    }
    UpdateCue(f);
    if (f->listHost && f->listHost->IsVisible()) {
        PositionList(f);
        f->listHost->Invalidate(false);
    }
}

void RefreshAnnotFilterAnnotations(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->edit) {
        return;
    }
    LoadAnnotations(f);
    RebuildList(f);
}

void DeleteAnnotFilterToolbar(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    HideList(f, false, false);
    win->UnregisterOnWindowMoved(&f->onWindowMoved);
    f->edit = nullptr;
    f->listBox = nullptr;
    delete f->listHost;
    f->listHost = nullptr;
    win->annotFilterToolbar = nullptr;
    if (win->toolbarVirt) {
        win->toolbarVirt->annotFilterEdit = nullptr;
    }
    delete f;
}

bool AnnotFilterListContainsScreenPoint(MainWindow* win, Point pt) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->listHost || !f->listHost->IsVisible()) {
        return false;
    }
    return f->listHost->ContainsScreenPoint(pt);
}

TempStr AnnotFilterToolbarStateTemp(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    Edit* edit = tb ? tb->annotFilterEdit : nullptr;
    bool editVisible = edit && edit->GetVisibility() == Visibility::Visible;
    bool listVisible = f && f->listHost && f->listHost->IsVisible();
    int nAll = f ? len(f->annotations) : 0;
    int nVisible = f ? len(f->visibleAnnots) : 0;
    int sel = (f && f->listBox) ? f->listBox->GetCurrentSelection() : -1;
    Rect r = edit ? edit->lastBounds : Rect{};
    Rect lr = listVisible ? f->listHost->ScreenRect() : Rect{};
    return fmt(
        "annotFilter hidden=%d rect=%d,%d,%d,%d listVisible=%d nAll=%d nVisible=%d sel=%d listRect=%d,%d,%d,%d\n",
        editVisible ? 0 : 1, r.x, r.y, r.dx, r.dy, listVisible ? 1 : 0, nAll, nVisible, sel, lr.x, lr.y, lr.dx, lr.dy);
}
