/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
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
#include "SvgIcons.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "Toolbar.h"
#include "FilterUtil.h"
#include "AnnotEditToolbar.h"
#include "DarkMode_win.h"

#include "AnnotFilterToolbar.h"

constexpr const WCHAR* kAnnotFilterListClassName = L"SumatraAnnotFilterList";
constexpr const WCHAR* kAnnotFilterFloatClassName = L"SUMATRA_ANNOT_FILTER_WND";
constexpr int kSelectionDebounceTimerId = 1;
constexpr int kSelectionDebounceMs = 300;
constexpr int kMaxListLines = 12;
constexpr int kFloatWinPadding = 8;
constexpr int kFloatWinGap = 6;

struct AnnotFilterToolbar;
struct AnnotFilterWindow;

static void RebuildList(AnnotFilterToolbar*);
static void UpdateCue(AnnotFilterToolbar*);
static void ApplySelectionNow(AnnotFilterToolbar*);
static void ScheduleSelection(AnnotFilterToolbar*);
static AnnotFilterToolbar* GetOrCreate(MainWindow*);
static void ShowAnnotFilterWindow(MainWindow*);
static void HideAnnotFilterWindow(MainWindow*);
static bool FilterHomeEndMovesList(AnnotFilterToolbar*, int vkey, bool isCtrl);
static bool IsListNavKey(int vkey);
static void KillSelectionTimer(AnnotFilterToolbar*);
static void UpdateFloatButtons(AnnotFilterToolbar*);

struct AnnotFilterWindow : WindowBase {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    VirtListBox* listBox = nullptr;
    VirtButton* btnDelete = nullptr;
    VirtButton* btnDiscard = nullptr;
    VirtButton* btnSave = nullptr;
    VirtButton* btnSaveNew = nullptr;
    Padding* rootPadding = nullptr;
    HBox* header = nullptr;
    Spacer* headerListGap = nullptr;
    Spacer* listButtonsGap = nullptr;
    VBox* buttonsBox = nullptr;
    int layoutDpi = 96;

    bool Create(MainWindow* mainWin);
    void SavePos();
    void UpdateTheme() override;
    void ApplyDarkMode() override;
    void UpdateDpi(int dpi);
    void BuildLayout();
    void OnClose(WindowBase::CloseEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void OnSize(WindowBase::SizeEvent* ev);
    void OnTimer(WindowBase::TimerEvent* ev);
    void OnDpiChanged(WindowBase::DpiChangedEvent* ev);
    void OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev);
};

struct AnnotFilterToolbar {
    MainWindow* win = nullptr;
    AnnotFilterWindow* floatWnd = nullptr;
    Vec<Annotation*> annotations;
    Vec<Annotation*> visibleAnnots;
    StrVec filterWords;
    Vec<u8> filterHlScratch;
    bool suppressFilterChanged = false;
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

static Edit* ActiveEdit(AnnotFilterToolbar* f) {
    return (f && f->floatWnd) ? f->floatWnd->edit : nullptr;
}

static VirtListBox* ActiveList(AnnotFilterToolbar* f) {
    return (f && f->floatWnd) ? f->floatWnd->listBox : nullptr;
}

static void UpdateCue(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    if (!f->floatWnd) {
        return;
    }
    EditSetCueText(f->floatWnd->edit, fmt(_TRA("filter %d annotations").s, len(f->annotations)));
}

static void LoadAnnotations(AnnotFilterToolbar* f) {
    if (!f || !f->win) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    VecReset(f->annotations);
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
    if (!f || !VecIsValidIndex(f->visibleAnnots, idx)) {
        return nullptr;
    }
    return f->visibleAnnots[idx];
}

static void ApplyListColorsTo(VirtListBox* lb) {
    if (!lb) {
        return;
    }
    lb->SetColor(kColListBg, ThemeWindowControlBackgroundColor());
    lb->SetColor(kColListText, ThemeWindowTextColor());
}

static void ApplyListColors(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    if (f->floatWnd) {
        ApplyListColorsTo(f->floatWnd->listBox);
    }
}

static ListBoxModelStrings* NewAnnotListModel(AnnotFilterToolbar* f) {
    auto* model = new ListBoxModelStrings();
    for (Annotation* annot : f->visibleAnnots) {
        model->strings.Append(AnnotationReadableNameTemp(annot->type));
    }
    return model;
}

static void ApplyVisibleToList(AnnotFilterToolbar* f, VirtListBox* lb, Annotation* caret,
                               const Vec<Annotation*>& keepSel, int prevScrollY) {
    if (!lb) {
        return;
    }
    lb->SetModel(NewAnnotListModel(f));
    lb->ScrollTo(prevScrollY);
    int caretIdx = caret ? VecFind(f->visibleAnnots, caret) : -1;
    if (caretIdx < 0) {
        Annotation* keep = FilterTab(f) ? FilterTab(f)->selectedAnnotation : nullptr;
        caretIdx = keep ? VecFind(f->visibleAnnots, keep) : -1;
    }
    if (lb->multiSelect && len(keepSel) > 0) {
        if (caretIdx < 0) {
            caretIdx = VecFind(f->visibleAnnots, keepSel[0]);
        }
        if (caretIdx >= 0) {
            lb->SetCurrentSelection(caretIdx);
        }
        for (Annotation* a : keepSel) {
            int i = VecFind(f->visibleAnnots, a);
            if (i >= 0 && !lb->IsSelected(i)) {
                lb->ToggleSelected(i);
            }
        }
        lb->Invalidate();
        return;
    }
    if (caretIdx >= 0) {
        lb->SetCurrentSelection(caretIdx);
    }
}

static void RebuildList(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    Annotation* caret = nullptr;
    int prevScrollY = 0;
    Vec<Annotation*> keepSel;
    VirtListBox* active = ActiveList(f);
    if (active) {
        caret = VisibleAnnotAt(f, active->GetCurrentSelection());
        prevScrollY = active->scrollY;
        if (active->multiSelect) {
            Vec<int> idxs;
            active->GetSelectedIndices(idxs);
            for (int i : idxs) {
                if (Annotation* a = VisibleAnnotAt(f, i)) {
                    VecAppend(keepSel, a);
                }
            }
        }
    }
    VecReset(f->visibleAnnots);
    for (Annotation* annot : f->annotations) {
        if (AnnotMatchesFilter(annot, f->filterWords)) {
            VecAppend(f->visibleAnnots, annot);
        }
    }
    if (f->floatWnd) {
        ApplyVisibleToList(f, f->floatWnd->listBox, caret, keepSel, prevScrollY);
    }
    UpdateCue(f);
    if (f->floatWnd && f->floatWnd->listBox) {
        f->floatWnd->listBox->Invalidate();
    }
    UpdateFloatButtons(f);
}

static void KillSelectionTimer(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    if (f->floatWnd && f->floatWnd->hwnd) {
        KillTimer(f->floatWnd->hwnd, kSelectionDebounceTimerId);
    }
}

static void CancelPendingSelection(AnnotFilterToolbar* f) {
    if (!f) {
        return;
    }
    f->selEpoch++;
    KillSelectionTimer(f);
}

static void ApplySelectionNow(AnnotFilterToolbar* f) {
    CancelPendingSelection(f);
    VirtListBox* lb = ActiveList(f);
    if (!f || !lb) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    if (!tab) {
        return;
    }
    int itemNo = lb->GetCurrentSelection();
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
    if (!f) {
        return;
    }
    VirtListBox* lb = ActiveList(f);
    if (!lb) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    if (!tab) {
        return;
    }
    int itemNo = lb->GetCurrentSelection();
    Annotation* annot = itemNo >= 0 ? VisibleAnnotAt(f, itemNo) : nullptr;
    if (annot == tab->selectedAnnotation) {
        CancelPendingSelection(f);
        return;
    }
    CancelPendingSelection(f);
    if (f->floatWnd && f->floatWnd->hwnd) {
        SetTimer(f->floatWnd->hwnd, kSelectionDebounceTimerId, kSelectionDebounceMs, nullptr);
        return;
    }
    f->applyEpoch = f->selEpoch;
    if (f->win) {
        uitask::Post(MkFunc0(PostedApplySelection, f->win), "ApplyAnnotFilterSelection");
    }
}

static void OnListTimer(AnnotFilterToolbar* f, int timerId) {
    if (!f || !f->win || timerId != (int)kSelectionDebounceTimerId) {
        return;
    }
    KillSelectionTimer(f);
    f->applyEpoch = f->selEpoch;
    uitask::Post(MkFunc0(PostedApplySelection, f->win), "ApplyAnnotFilterSelection");
}

static void OnListSelectionChanged(AnnotFilterToolbar* f) {
    UpdateFloatButtons(f);
    ScheduleSelection(f);
}

static void OnListDoubleClick(AnnotFilterToolbar* f) {
    ApplySelectionNow(f);
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

static void WireListBox(AnnotFilterToolbar* f, VirtListBox* lb, int dpi) {
    lb->dpi = dpi;
    lb->font = GetAppFont();
    lb->padding = DpiScaledInsets(1, 1);
    lb->idealSizeLines = 1;
    lb->SetModel(new ListBoxModelStrings());
    lb->onSelectionChanged = MkFunc0(OnListSelectionChanged, f);
    lb->onDoubleClick = MkFunc0(OnListDoubleClick, f);
    lb->onDrawItem = MkFunc1(OnListDrawItem, f);
    lb->SetFlag(vwfFocusable, false);
    ApplyListColorsTo(lb);
}

static bool FilterHomeEndMovesList(AnnotFilterToolbar* f, int vkey, bool isCtrl) {
    if (vkey != VK_HOME && vkey != VK_END) {
        return true;
    }
    if (isCtrl) {
        return true;
    }
    Edit* e = ActiveEdit(f);
    if (!e) {
        return true;
    }
    int selStart = 0;
    int selEnd = 0;
    EditGetSelection(e, selStart, selEnd);
    int textLen = EditGetTextLen(e);
    bool toEnd = (vkey == VK_END);
    return (selStart == selEnd) && (toEnd ? selEnd == textLen : selStart == 0);
}

static void HandleEscape(AnnotFilterToolbar* f) {
    Edit* e = ActiveEdit(f);
    if (!f || !e) {
        return;
    }
    if (len(e->GetTextTemp()) > 0) {
        e->SetText({});
        return;
    }
    if (f->win && f->win->hwndCanvas) {
        HwndSetFocus(f->win->hwndCanvas);
    }
}

static void OnFilterTextChanged(AnnotFilterToolbar* f) {
    if (!f || f->suppressFilterChanged) {
        return;
    }
    Edit* e = ActiveEdit(f);
    if (!e) {
        return;
    }
    WindowTab* tab = FilterTab(f);
    Annotation* keep = tab ? tab->selectedAnnotation : nullptr;
    f->filterWords.Reset();
    SplitFilterToWords(e->GetTextTemp(), f->filterWords);
    LoadAnnotations(f);
    RebuildList(f);
    VirtListBox* lb = ActiveList(f);
    if (!lb) {
        return;
    }
    int idx = keep ? VecFind(f->visibleAnnots, keep) : -1;
    if (idx >= 0) {
        lb->SetCurrentSelection(idx);
        return;
    }
    if (len(f->visibleAnnots) > 0) {
        lb->SetCurrentSelection(0);
        ScheduleSelection(f);
        return;
    }
    if (tab) {
        SetSelectedAnnotation(tab, nullptr);
    }
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
            VirtListBox* lb = ActiveList(f);
            if (!lb || lb->ItemsCount() == 0) {
                ev->didHandle = true;
                ev->result = 0;
                return;
            }
            VirtKeyEvent ke;
            ke.vkey = vkey;
            ke.isCtrl = isCtrl;
            ke.isShift = isShift;
            ke.isAlt = isAlt;
            lb->OnKeyDown(&ke);
            ev->didHandle = ke.didHandle;
            ev->result = 0;
            return;
        }
    }
    // Create() installs Edit::WndProc as onWndProc; we replaced it, so forward
    // NC paint / frame / Ctrl+Backspace to the edit itself.
    auto* e = (Edit*)ev->w;
    if (e) {
        e->WndProc(ev);
    }
}

static void WireFilterEdit(AnnotFilterToolbar* f, Edit* e) {
    if (!f || !e) {
        return;
    }
    e->onTextChanged = MkFunc0(OnFilterTextChanged, f);
    e->onWndProc = MkFunc1(OnFilterWndProc, f);
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
    win->annotFilterToolbar = f;
    return f;
}

static int FilterEditPadL() {
    return UiEdgeDx();
}

static int FilterEditPadR() {
    return FilterEditPadL() + DpiScale(4);
}

static bool FrameIsMaxOrFullscreen(MainWindow* win) {
    if (!win || !win->hwndFrame) {
        return false;
    }
    return win->isFullScreen || win->presentation || IsZoomed(win->hwndFrame);
}

static Rect AnnotFilterDefaultRect(MainWindow* win, int dx, int dy) {
    HWND hwnd = win->hwndFrame;
    Rect fr = HwndWindowRect(hwnd);
    int dpi = DpiGetForHwnd(hwnd);
    int gap = DpiScaleByDpi(dpi, kFloatWinGap);
    Rect area = (win->isFullScreen || win->presentation) ? HwndGetFullscreenRect(hwnd) : GetWorkAreaRect(fr, nullptr);
    dx = std::min(dx, std::max(area.dx, 1));
    dy = std::min(dy, std::max(area.dy, 1));

    if (FrameIsMaxOrFullscreen(win)) {
        int x = area.x + area.dx - dx;
        int y = area.y + (area.dy - dy) / 2;
        return {x, y, dx, dy};
    }

    int spaceRight = area.x + area.dx - (fr.x + fr.dx);
    int spaceLeft = fr.x - area.x;
    int x = spaceRight >= spaceLeft ? fr.x + fr.dx + gap : fr.x - gap - dx;
    return ShiftRectToWorkArea({x, fr.y, dx, dy}, nullptr, true);
}

static Rect AnnotFilterWindowPlacementRect(MainWindow* win) {
    if (!win || !win->hwndFrame) {
        return {};
    }
    int dpi = DpiGetForHwnd(win->hwndFrame);
    int dx = DpiScaleByDpi(dpi, 360);
    int dy = DpiScaleByDpi(dpi, 540);
    Rect saved = win->annotListFloatPos;
    if (saved.dx > 0) {
        dx = saved.dx;
    }
    if (saved.dy > 0) {
        dy = saved.dy;
    }
    if (win->annotListFloatPosUserSet && !saved.IsEmpty()) {
        return ShiftRectToWorkArea(saved, nullptr, true);
    }
    if (FrameIsMaxOrFullscreen(win) || saved.IsEmpty()) {
        return AnnotFilterDefaultRect(win, dx, dy);
    }
    return ShiftRectToWorkArea(saved, nullptr, true);
}

static void PositionAnnotFilterWindow(AnnotFilterWindow* w) {
    if (!w || !w->hwnd || !w->win) {
        return;
    }
    Rect r = AnnotFilterWindowPlacementRect(w->win);
    SetWindowPos(w->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_NOACTIVATE);
}

static void DeleteFloatSelected(AnnotFilterWindow* w) {
    if (!w || !w->win || !w->listBox) {
        return;
    }
    WindowTab* tab = w->win->CurrentTab();
    AnnotFilterToolbar* f = w->win->annotFilterToolbar;
    if (!tab || !f) {
        return;
    }
    Vec<int> idxs;
    w->listBox->GetSelectedIndices(idxs);
    if (len(idxs) == 0) {
        int idx = w->listBox->GetCurrentSelection();
        if (idx >= 0) {
            VecAppend(idxs, idx);
        }
    }
    Vec<Annotation*> toDelete;
    for (int idx : idxs) {
        if (Annotation* a = VisibleAnnotAt(f, idx)) {
            VecAppend(toDelete, a);
        }
    }
    if (len(toDelete) == 0) {
        if (tab->selectedAnnotation) {
            VecAppend(toDelete, tab->selectedAnnotation);
        }
    }
    if (len(toDelete) == 0) {
        return;
    }
    Annotation* keepSelected = tab->selectedAnnotation;
    if (VecContains(toDelete, keepSelected)) {
        keepSelected = nullptr;
    }
    for (Annotation* annot : toDelete) {
        DetachAnnotationFromUI(annot);
        DeleteAnnotation(annot);
    }
    RefreshAnnotationLists(tab);
    SetSelectedAnnotation(tab, keepSelected);
    if (IsMainWindowValidAndNotClosing(w->win)) {
        MainWindowRerender(w->win);
    }
}

static void OnFloatDelete(AnnotFilterWindow* w, VirtMouseEvent*) {
    DeleteFloatSelected(w);
}

static void OnFloatDiscard(AnnotFilterWindow* w, VirtMouseEvent*) {
    if (w && w->win) {
        HwndPostCommand(w->win->hwndFrame, CmdDiscardChanges);
    }
}

static void OnFloatSave(AnnotFilterWindow* w, VirtMouseEvent*) {
    if (w && w->win) {
        HwndPostCommand(w->win->hwndFrame, CmdSaveAnnotations);
    }
}

static void OnFloatSaveNew(AnnotFilterWindow* w, VirtMouseEvent*) {
    if (w && w->win) {
        HwndPostCommand(w->win->hwndFrame, CmdSaveAnnotationsNewFile);
    }
}

static VirtButton* NewFloatActionButton(HWND hwnd, Str text, bool enabled) {
    auto* b = NewThemedButton(hwnd, text, GetAppFont(), false);
    b->textPadding = DpiScaledInsets(2, 12);
    b->SetIsEnabled(enabled);
    return b;
}

static void UpdateFloatButtons(AnnotFilterToolbar* f) {
    if (!f || !f->floatWnd) {
        return;
    }
    AnnotFilterWindow* w = f->floatWnd;
    WindowTab* tab = FilterTab(f);
    int nSel = 0;
    if (w->listBox) {
        nSel = w->listBox->SelectedCount();
        if (nSel == 0 && w->listBox->GetCurrentSelection() >= 0) {
            nSel = 1;
        }
    }
    if (nSel == 0 && tab && tab->selectedAnnotation) {
        nSel = 1;
    }
    if (w->btnDelete) {
        w->btnDelete->SetIsEnabled(nSel > 0);
        if (nSel > 1) {
            w->btnDelete->SetText(fmt(_TRA("Delete %d annotations").s, nSel));
        } else {
            w->btnDelete->SetText(_TRA("Delete Annotation"));
        }
        w->btnDelete->RequestLayout();
    }
    bool dirty = false;
    if (tab && tab->AsFixed()) {
        dirty = EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
    }
    if (w->btnDiscard) {
        w->btnDiscard->SetIsEnabled(dirty);
    }
    if (w->btnSave) {
        w->btnSave->SetIsEnabled(dirty);
        TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
        if (len(base) > 0) {
            w->btnSave->SetText(fmt(_TRA("Save changes to %s").s, base));
        } else {
            w->btnSave->SetText(_TRA("Save changes to existing PDF"));
        }
        w->btnSave->RequestLayout();
    }
    if (w->btnSaveNew) {
        w->btnSaveNew->SetIsEnabled(dirty);
    }
}

void AnnotFilterWindow::BuildLayout() {
    int pad = DpiScale(kFloatWinPadding);
    int gap = DpiScale(kFloatWinGap);
    header = new HBox();
    header->alignCross = CrossAxisAlign::CrossCenter;
    header->gap = gap;
    header->rtl = IsUIRtl();
    header->AddChild(edit, 1);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(header);
    headerListGap = new Spacer(0, gap);
    vbox->AddChild(headerListGap);
    vbox->AddChild(listBox, 1);
    listButtonsGap = new Spacer(0, gap);
    vbox->AddChild(listButtonsGap);
    buttonsBox = new VBox();
    buttonsBox->alignCross = CrossAxisAlign::Stretch;
    buttonsBox->gap = gap;
    buttonsBox->AddChild(btnDelete);
    buttonsBox->AddChild(btnDiscard);
    buttonsBox->AddChild(btnSave);
    buttonsBox->AddChild(btnSaveNew);
    vbox->AddChild(buttonsBox);

    rootPadding = new Padding(vbox, Insets{pad, pad, pad, pad});
    layout = rootPadding;
    layoutDpi = GetDpi();
}

bool AnnotFilterWindow::Create(MainWindow* mainWin) {
    win = mainWin;
    AnnotFilterToolbar* f = GetOrCreate(win);
    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();

    {
        CreateCustomArgs args;
        args.visible = false;
        args.title = _TRA("Annotations");
        args.className = WStr(kAnnotFilterFloatClassName);
        args.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN;
        args.exStyle = WS_EX_TOOLWINDOW;
        args.isRtl = IsUIRtl();
        args.pos = AnnotFilterWindowPlacementRect(win);
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        args.bgColor = colBg;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)win->hwndFrame);
    SetColors(colTxt, colBg);
    DarkModeApplyToTitleBar(hwnd);

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.font = GetAppFont();
        args.isRtl = IsUIRtl();
        args.withFrame = true;
        args.noTheme = true;
        args.selectAllOnFocus = true;
        args.centerTextVert = true;
        args.marginLeft = FilterEditPadL();
        args.marginRight = FilterEditPadR();
        edit = new Edit();
        edit->SetColors(colTxt, colBg);
        edit->Create(args);
        edit->SetIdealWidthFromText(fmt(_TRA("filter %d annotations").s, 999), DpiScale(8));
        EditSetCueText(edit, fmt(_TRA("filter %d annotations").s, 0));
        WireFilterEdit(f, edit);
    }

    {
        listBox = new VirtListBox();
        WireListBox(f, listBox, GetDpi());
        listBox->multiSelect = true;
        listBox->SetFlag(vwfFocusable, true);
    }

    btnDelete = NewFloatActionButton(hwnd, _TRA("Delete Annotation"), false);
    btnDelete->onClick = MkFunc1(OnFloatDelete, this);
    btnDiscard = NewFloatActionButton(hwnd, _TRA("Discard changes"), false);
    btnDiscard->onClick = MkFunc1(OnFloatDiscard, this);
    btnSave = NewFloatActionButton(hwnd, _TRA("Save changes to existing PDF"), false);
    btnSave->onClick = MkFunc1(OnFloatSave, this);
    btnSaveNew = NewFloatActionButton(hwnd, _TRA("Save changes to a new PDF"), false);
    btnSaveNew->onClick = MkFunc1(OnFloatSaveNew, this);

    BuildLayout();
    DarkModeApplyToPopupWindow(hwnd);
    return true;
}

void AnnotFilterWindow::SavePos() {
    if (!win || !hwnd || !HwndIsVisible(hwnd)) {
        return;
    }
    win->annotListFloatPos = HwndWindowRect(hwnd);
}

void AnnotFilterWindow::ApplyDarkMode() {
    DarkModeApplyToTitleBar(hwnd);
}

void AnnotFilterWindow::UpdateTheme() {
    WindowBase::UpdateTheme();
    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (edit) {
        edit->SetColors(colTxt, colBg);
    }
    ApplyListColorsTo(listBox);
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    UpdateFloatButtons(f);
}

void AnnotFilterWindow::UpdateDpi(int dpi) {
    if (dpi <= 0 || dpi == layoutDpi) {
        return;
    }
    if (!layout || !edit || !listBox) {
        return;
    }
    PlatformFont* appFont = GetAppFontForDpi(dpi);
    edit->SetFont(appFont);
    listBox->font = appFont;
    listBox->dpi = dpi;
    int pad = DpiScaleByDpi(dpi, kFloatWinPadding);
    int gap = DpiScaleByDpi(dpi, kFloatWinGap);
    if (header) {
        header->gap = gap;
    }
    if (headerListGap) {
        headerListGap->dy = gap;
    }
    if (rootPadding) {
        rootPadding->insets = Insets{pad, pad, pad, pad};
    }
    if (listButtonsGap) {
        listButtonsGap->dy = gap;
    }
    if (buttonsBox) {
        buttonsBox->gap = gap;
    }
    Insets btnPad{DpiScaleByDpi(dpi, 2), DpiScaleByDpi(dpi, 12), DpiScaleByDpi(dpi, 2), DpiScaleByDpi(dpi, 12)};
    VirtButton* btns[] = {btnDelete, btnDiscard, btnSave, btnSaveNew};
    for (VirtButton* b : btns) {
        if (b) {
            b->font = appFont;
            b->textPadding = btnPad;
        }
    }
    layoutDpi = dpi;
    DoLayout();
}

void AnnotFilterWindow::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg == WM_SIZE) {
        HwndInvalidate(hwnd, true);
        return;
    }
    if (ev->msg == WM_EXITSIZEMOVE) {
        HwndInvalidate(hwnd, true);
        SavePos();
        if (win) {
            win->annotListFloatPosUserSet = true;
        }
    }
}

void AnnotFilterWindow::OnTimer(WindowBase::TimerEvent* ev) {
    if (!win || ev->timerId != kSelectionDebounceTimerId) {
        return;
    }
    KillTimer(hwnd, ev->timerId);
    OnListTimer(win->annotFilterToolbar, (int)ev->timerId);
}

void AnnotFilterWindow::OnDpiChanged(WindowBase::DpiChangedEvent* ev) {
    if (!layout) {
        ev->didHandle = true;
        return;
    }
    RECT* r = ev->suggested;
    if (r) {
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    UpdateDpi((int)ev->dpiX);
    ev->didHandle = true;
}

void AnnotFilterWindow::OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev) {
    auto* mmi = ev->mmi;
    if (layout) {
        int clientMinDx = layout->MinIntrinsicWidth(0);
        int clientMinDy = layout->MinIntrinsicHeight(clientMinDx);
        Rect wr = HwndWindowRect(hwnd);
        Rect cr = HwndClientRect(hwnd);
        mmi->ptMinTrackSize.x = clientMinDx + (wr.dx - cr.dx);
        mmi->ptMinTrackSize.y = clientMinDy + (wr.dy - cr.dy);
        return;
    }
    int pad = DpiScale(kFloatWinPadding);
    mmi->ptMinTrackSize.x = (2 * pad) + DpiScale(240);
    mmi->ptMinTrackSize.y = (2 * pad) + DpiScale(160);
}

void AnnotFilterWindow::OnClose(WindowBase::CloseEvent* /*ev*/) {
    // the caption close only hides: Find Annotation opens the same window again
    if (win) {
        HideAnnotFilterWindow(win);
    }
}

void AnnotFilterWindow::OnKeyDown(KeyEvent* ev) {
    bool filterFocused = edit && ev->hwnd == edit->hwnd;
    if (ev->vkey == VK_DELETE) {
        if (filterFocused) {
            return;
        }
        DeleteFloatSelected(this);
        ev->didHandle = true;
        return;
    }
    if (filterFocused || !listBox) {
        return;
    }
    if ((ev->vkey == 'A' && ev->isCtrl && !ev->isAlt) || IsListNavKey(ev->vkey)) {
        VirtKeyEvent ke;
        ke.vkey = ev->vkey;
        ke.isCtrl = ev->isCtrl;
        ke.isShift = ev->isShift;
        ke.isAlt = ev->isAlt;
        listBox->OnKeyDown(&ke);
        ev->didHandle = ke.didHandle;
    }
}

static AnnotFilterWindow* CreateAnnotFilterWindow(MainWindow* win) {
    auto* w = new AnnotFilterWindow();
    w->onSize = MkMethod1<AnnotFilterWindow, WindowBase::SizeEvent*, &AnnotFilterWindow::OnSize>(w);
    w->onDpiChanged = MkMethod1<AnnotFilterWindow, WindowBase::DpiChangedEvent*, &AnnotFilterWindow::OnDpiChanged>(w);
    w->onGetMinMaxInfo =
        MkMethod1<AnnotFilterWindow, WindowBase::GetMinMaxInfoEvent*, &AnnotFilterWindow::OnGetMinMaxInfo>(w);
    w->onClose = MkMethod1<AnnotFilterWindow, WindowBase::CloseEvent*, &AnnotFilterWindow::OnClose>(w);
    w->onKeyDown = MkMethod1<AnnotFilterWindow, KeyEvent*, &AnnotFilterWindow::OnKeyDown>(w);
    w->onTimer = MkMethod1<AnnotFilterWindow, WindowBase::TimerEvent*, &AnnotFilterWindow::OnTimer>(w);
    if (!w->Create(win)) {
        delete w;
        return nullptr;
    }
    return w;
}

static void ShowAnnotFilterWindow(MainWindow* win) {
    AnnotFilterToolbar* f = GetOrCreate(win);
    if (!f) {
        return;
    }
    if (!f->floatWnd) {
        f->floatWnd = CreateAnnotFilterWindow(win);
        if (!f->floatWnd) {
            return;
        }
    }
    LoadAnnotations(f);
    RebuildList(f);
    UpdateFloatButtons(f);
    PositionAnnotFilterWindow(f->floatWnd);
    Rect wr = HwndWindowRect(f->floatWnd->hwnd);
    f->floatWnd->UpdateDpi(DpiGetForPoint(wr.x + wr.dx / 2, wr.y + wr.dy / 2));
    f->floatWnd->DoLayout();
    ShowWindow(f->floatWnd->hwnd, SW_SHOW);
    if (f->floatWnd->edit) {
        f->floatWnd->edit->SetFocus();
    }
}

static void HideAnnotFilterWindow(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->floatWnd || !f->floatWnd->hwnd) {
        return;
    }
    f->floatWnd->SavePos();
    if (HwndIsVisible(f->floatWnd->hwnd)) {
        ShowWindow(f->floatWnd->hwnd, SW_HIDE);
    }
}

bool IsFloatingAnnotListVisible(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    return f && f->floatWnd && f->floatWnd->hwnd && HwndIsVisible(f->floatWnd->hwnd);
}

void ToggleFloatingAnnotList(MainWindow* win) {
    if (!win) {
        return;
    }
    if (IsFloatingAnnotListVisible(win)) {
        HideAnnotFilterWindow(win);
        return;
    }
    ShowAnnotFilterWindow(win);
}

void UpdateAnnotFilterToolbar(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    ApplyListColors(f);
    if (f->floatWnd) {
        f->floatWnd->UpdateTheme();
    }
    if (!win->pdfAnnotationsToolbarEnabled) {
        HideAnnotFilterWindow(win);
        return;
    }
    Edit* e = ActiveEdit(f);
    bool filterFocused = e && e->IsFocused();
    VirtListBox* lb = ActiveList(f);
    if (lb && !filterFocused) {
        WindowTab* tab = FilterTab(f);
        Annotation* keep = tab ? tab->selectedAnnotation : nullptr;
        int idx = keep ? VecFind(f->visibleAnnots, keep) : -1;
        if (idx >= 0 && lb->GetCurrentSelection() != idx) {
            lb->SetCurrentSelection(idx);
        }
    }
    UpdateCue(f);
    UpdateFloatButtons(f);
}

void RefreshAnnotFilterAnnotations(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    LoadAnnotations(f);
    RebuildList(f);
    UpdateFloatButtons(f);
}

void DeleteAnnotFilterToolbar(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    if (f->floatWnd) {
        f->floatWnd->SavePos();
        AnnotFilterWindow* w = f->floatWnd;
        f->floatWnd = nullptr;
        w->win = nullptr;
        delete w;
    }
    win->annotFilterToolbar = nullptr;
    delete f;
}

TempStr AnnotFilterToolbarStateTemp(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    bool floatVisible = f && f->floatWnd && f->floatWnd->hwnd && HwndIsVisible(f->floatWnd->hwnd);
    int nAll = f ? len(f->annotations) : 0;
    int nVisible = f ? len(f->visibleAnnots) : 0;
    VirtListBox* lb = ActiveList(f);
    int sel = lb ? lb->GetCurrentSelection() : -1;
    Rect wr = floatVisible ? HwndWindowRect(f->floatWnd->hwnd) : Rect{};
    str::Builder out;
    out.Append(fmt("annotFilter floatVisible=%d floatRect=%d,%d,%d,%d nAll=%d nVisible=%d sel=%d\n",
                   floatVisible ? 1 : 0, wr.x, wr.y, wr.dx, wr.dy, nAll, nVisible, sel));
    int deleteOn = 0;
    int discardOn = 0;
    int saveOn = 0;
    int nSel = 0;
    int itemDy = 0;
    int listY = 0;
    if (f && f->floatWnd) {
        deleteOn = f->floatWnd->btnDelete && f->floatWnd->btnDelete->IsEnabled() ? 1 : 0;
        discardOn = f->floatWnd->btnDiscard && f->floatWnd->btnDiscard->IsEnabled() ? 1 : 0;
        saveOn = f->floatWnd->btnSave && f->floatWnd->btnSave->IsEnabled() ? 1 : 0;
        if (f->floatWnd->listBox) {
            nSel = f->floatWnd->listBox->SelectedCount();
            itemDy = f->floatWnd->listBox->GetItemHeight();
            listY = f->floatWnd->listBox->BoundsInWindow().y;
        }
    }
    out.Append(fmt("deleteEnabled=%d discardEnabled=%d saveEnabled=%d nSel=%d itemDy=%d listY=%d\n", deleteOn,
                   discardOn, saveOn, nSel, itemDy, listY));
    return ToStrTemp(out);
}
