/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's SumatraDialogs.cpp is only the advanced-print property sheet
// (step 14). This file is the two things every ported dialog needs: the
// portable message box (orig calls win32 MessageBoxW through base's MsgBox())
// as a gpui AlertDialog, and the one place the shell asks "is a dialog up?".

#include "gui/GpuiBridge.h"
#include "base/UITask.h"
#include "VirtKeys.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "Version.h"
#include "SumatraPDF.h"
#include "Commands.h"
#include "Translations.h"
#include "KeyboardHelp.h"
#include "HomePage.h"
#include "DocumentProperties.h"
#include "TabGroupsManage.h"
#include "Menu.h"
#include "ImageSaveCropResize.h"
#include "Screenshot.h"
#include "PdfTools.h"
#include "TextViewWnd.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "gui/PlatformFont.h"
#include "AIChatCommon.h"
#include "SelectionTranslate.h"
#include "SimpleBrowserWindow.h"
#include "gui/DialogWidgets.h"
#include "gui/NativeMsgBox.h"
#include "SumatraDialogs.h"

#include "SumatraLog.h"

struct MsgBoxDlg {
    MainWindow* win = nullptr;
    // ng: the tool window that was the active one when the box was raised: it
    // is drawn there, as orig's message box is owned by the window that asks
    ToolWindow* host = nullptr;
    bool visible = false;
    Str text;
    Str caption;
    uint flags = 0;
    Func1<int> onResult;
};

static MsgBoxDlg gMsgBox;

struct MsgBoxView {
    static void OnOk(MsgBoxView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(MsgBoxView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<MsgBoxView> gMsgBoxView;

static void FreeMsgBox() {
    str::Free(gMsgBox.text);
    gMsgBox.text = {};
    str::Free(gMsgBox.caption);
    gMsgBox.caption = {};
    gMsgBox.onResult = {};
}

// in a tool window that is still there
static bool MsgBoxInToolWindow() {
    return gMsgBox.visible && gMsgBox.host && ToolWindowIsLive(gMsgBox.host);
}

// the frame's message box; one in a tool window is that window's business
bool IsMsgBoxVisible() {
    return gMsgBox.visible && !MsgBoxInToolWindow();
}

// hands the caller the button that was pressed, exactly once
static void FinishMsgBox(int res) {
    if (!gMsgBox.visible) {
        return;
    }
    gMsgBox.visible = false;
    gMsgBox.host = nullptr;
    MainWindow* win = gMsgBox.win;
    Func1<int> onResult = gMsgBox.onResult;
    logf("MsgBox: result %d\n", res);
    FreeMsgBox();
    onResult.Call(res);
    AppShellInvalidate(win);
}

static int MsgBoxButtonKind() {
    return (int)(gMsgBox.flags & 0xf);
}

static void AcceptMsgBox() {
    int kind = MsgBoxButtonKind();
    bool isYesNo = kind == (int)MbYesNo || kind == (int)MbYesNoCancel;
    FinishMsgBox(isYesNo ? MbRetYes : MbRetOk);
}

void MsgBoxView::OnOk(MsgBoxView*, gp::Ctx* cx, const gp::ClickEvent*) {
    AcceptMsgBox();
    gp::Notify(cx);
}

static void CancelMsgBox() {
    int kind = MsgBoxButtonKind();
    bool isYesNo = kind == (int)MbYesNo || kind == (int)MbYesNoCancel;
    FinishMsgBox(isYesNo ? MbRetNo : MbRetCancel);
}

void MsgBoxView::OnCancel(MsgBoxView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CancelMsgBox();
    gp::Notify(cx);
}

void MsgBox(MainWindow* win, Str text, Str caption, uint flags, Func1<int> onResult) {
    // a second message box would drop the first one's answer on the floor
    if (gMsgBox.visible) {
        FinishMsgBox(MbRetCancel);
    }
    if (!win) {
        win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    }
    if (!IsMainWindowValidAndNotClosing(win)) {
        logf("MsgBox: no window for '%s'\n", text);
        onResult.Call(MbRetCancel);
        return;
    }
#if OS_WIN
    // orig's MessageBoxW, where the system's box can be shown
    if (NativeMsgBoxEnabled()) {
        logf("MsgBox (system): '%s' / '%s' flags 0x%x\n", caption, text, flags);
        NativeMsgBox(win, text, len(caption) > 0 ? caption : Str(kAppName), flags, onResult);
        return;
    }
#endif
    gMsgBox.win = win;
    gMsgBox.host = ToolWindowActiveOf(win);
    gMsgBox.text = str::Dup(text);
    gMsgBox.caption = str::Dup(len(caption) > 0 ? caption : Str(kAppName));
    gMsgBox.flags = flags;
    gMsgBox.onResult = onResult;
    gMsgBox.visible = true;
    logf("MsgBox: '%s' / '%s' flags 0x%x\n", caption, text, flags);
    AppShellInvalidate(win);
}

// orig's MessageBoxWarning (SumatraPDF.cpp): a warning box with just OK
void MessageBoxWarning(MainWindow* win, Str msg, Str title) {
    if (len(title) == 0) {
        title = Tr("Warning");
    }
    MsgBox(win, msg, title, MbOk | MbIconWarning);
}

static gp::El* MsgBoxEl(gp::Ctx* cx);

static gp::El* MsgBoxBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gMsgBox.visible || gMsgBox.win != win || MsgBoxInToolWindow()) {
        return nullptr;
    }
    return MsgBoxEl(cx);
}

gp::El* MsgBoxBuildInToolWindow(ToolWindow* tw, gp::Ctx* cx) {
    if (!MsgBoxInToolWindow() || gMsgBox.host != tw) {
        return nullptr;
    }
    return MsgBoxEl(cx);
}

// win32's message box keys: Enter and Space take the default button, Esc
// cancels, &Yes / &No answer to their letters; nothing else gets through
bool MsgBoxOnKeyInToolWindow(ToolWindow* tw, int vk, bool ctrl) {
    if (!MsgBoxInToolWindow() || gMsgBox.host != tw) {
        return false;
    }
    int kind = MsgBoxButtonKind();
    bool isYesNo = kind == (int)MbYesNo || kind == (int)MbYesNoCancel;
    if (vk == VK_RETURN || vk == VK_SPACE) {
        AcceptMsgBox();
    } else if (vk == VK_ESCAPE) {
        CancelMsgBox();
    } else if (isYesNo && !ctrl && (vk == 'Y' || vk == 'N')) {
        FinishMsgBox(vk == 'Y' ? MbRetYes : MbRetNo);
    }
    return true;
}

static gp::El* MsgBoxEl(gp::Ctx* cx) {
    if (!gMsgBoxView.IsValid()) {
        gMsgBoxView = gp::EntityNewState<MsgBoxView>(cx->app);
    }
    int kind = MsgBoxButtonKind();
    bool isYesNo = kind == (int)MbYesNo || kind == (int)MbYesNoCancel;
    bool showCancel = kind != (int)MbOk;
    Str okText = isYesNo ? Tr("Yes") : Tr("OK");
    Str cancelText = isYesNo ? Tr("No") : Tr("Cancel");

    gpc::AlertDialog* dlg = gpc::AlertDialog::New(cx)
                                ->Open(true)
                                ->Title(ToGpui(gMsgBox.caption))
                                ->Description(GpuiDup(cx->a, gMsgBox.text))
                                ->W(420)
                                ->OkText(ToGpui(okText))
                                ->CancelText(ToGpui(cancelText))
                                ->ShowCancel(showCancel)
                                ->OnOk(gp::ListenTo(gMsgBoxView, &MsgBoxView::OnOk))
                                ->OnCancel(gp::ListenTo(gMsgBoxView, &MsgBoxView::OnCancel))
                                ->OnClose(gp::ListenTo(gMsgBoxView, &MsgBoxView::OnCancel));
    const gp::Theme& th = gp::ThemeNow(cx->app);
    uint icon = gMsgBox.flags & 0xf0;
    if (icon == MbIconError) {
        dlg->Icon(gp::IconName::CircleX, th.danger);
    } else if (icon == MbIconWarning) {
        dlg->Icon(gp::IconName::TriangleAlert, th.warning);
    } else if (icon == MbIconQuestion) {
        dlg->Icon(gp::IconName::Info, th.primary);
    } else if (icon == MbIconInformation) {
        dlg->Icon(gp::IconName::Info, th.info);
    }
    return dlg->IntoEl(gp::WindowSize(cx->win));
}

// --- the shell's one entry point --------------------------------------------

// ng: orig's dialogs are windows the OS stacks; here each one is an element the
// shell adds to the frame, so the order below is the stacking order and a
// message box (which another dialog can raise) comes last.
// --- dialogs in windows of their own ----------------------------------------

struct DlgWindowEntry {
    ToolWindow* tw = nullptr;
    DlgWindowSpec spec;
    MainWindow* win = nullptr;
    // the content as laid out last frame
    gp::Bounds content{};
    // where a combo box's dropped list ends, 0 without one: the window is
    // at least that tall, since the list cannot hang over its edge
    float comboBottom = 0;
    float width = 0;
    // sized to its content and put where it belongs
    bool placed = false;
    bool sizePosted = false;
};

static Vec<DlgWindowEntry*> gDlgWindows;

constexpr float kDlgWinPad = 12;
constexpr float kDlgWinGap = 12;
// until the first frame measured the content
constexpr int kDlgWinGuessDx = 448;
constexpr int kDlgWinGuessDy = 200;
// where the window is made, before its content was measured
constexpr int kDlgWinOffScreen = -32000;

static DlgWindowEntry* DlgWindowEntryOf(gp::Window* gw) {
    for (DlgWindowEntry* e : gDlgWindows) {
        if (gw && ToolWindowGpui(e->tw) == gw) {
            return e;
        }
    }
    return nullptr;
}

bool DlgWindowIsHost(gp::Ctx* cx) {
    return DlgWindowEntryOf(cx->win) != nullptr;
}

gp::Window* DlgWindowInputTarget(MainWindow* win) {
    DlgWindowEntry* active = nullptr;
    for (int i = len(gDlgWindows) - 1; i >= 0; i--) {
        DlgWindowEntry* e = gDlgWindows[i];
        if (e->win != win || !ToolWindowIsLive(e->tw) || !ToolWindowGpui(e->tw) || !ToolWindowIsVisible(e->tw)) {
            continue;
        }
        if (e->spec.modal) {
            return ToolWindowGpui(e->tw);
        }
        if (!active && ToolWindowIsActive(e->tw)) {
            active = e;
        }
    }
    return active ? ToolWindowGpui(active->tw) : nullptr;
}

// the window whose spec callback is running: a dialog with several windows
// (the PDF tools) tells from it which of them is meant
static ToolWindow* gDlgWindowCurrent = nullptr;

struct DlgWindowCurrentScope {
    ToolWindow* prev;

    explicit DlgWindowCurrentScope(ToolWindow* tw) : prev(gDlgWindowCurrent) { gDlgWindowCurrent = tw; }
    ~DlgWindowCurrentScope() { gDlgWindowCurrent = prev; }
    DlgWindowCurrentScope(const DlgWindowCurrentScope&) = delete;
    DlgWindowCurrentScope& operator=(const DlgWindowCurrentScope&) = delete;
};

ToolWindow* DlgWindowCurrent() {
    return gDlgWindowCurrent;
}

static void DlgWindowForget(DlgWindowEntry* e) {
    VecRemove(gDlgWindows, e);
    delete e;
}

static gp::El* DlgWindowBuild(MainWindow* owner, gp::Ctx* cx) {
    DlgWindowEntry* e = DlgWindowEntryOf(cx->win);
    if (!e || !e->spec.build) {
        return nullptr;
    }
    DlgWindowCurrentScope current(e->tw);
    return e->spec.build(owner, cx);
}

static bool DlgWindowOnKey(MainWindow*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    DlgWindowEntry* e = DlgWindowEntryOf(cx->win);
    if (!e) {
        return false;
    }
    DlgWindowCurrentScope current(e->tw);
    // with nothing focused gpui has no capture phase: the key comes here
    if (e->spec.onKey && e->spec.onKey(e->win, (int)ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        return true;
    }
    if (ev->vk == VK_ESCAPE && !e->spec.closeOnEsc) {
        return true;
    }
    if (ToolWindowDialogKey(cx, ev, e->spec.close)) {
        return true;
    }
    bool editFocused = cx->win->input && cx->win->input->focused;
    if (ev->vk == VK_RETURN && !ev->ctrl && !ev->alt && !editFocused && e->spec.onEnter) {
        e->spec.onEnter();
        return true;
    }
    return false;
}

// Up / Down drive a dialog's list before the focused edit sees them
static bool DlgWindowOnCaptureKey(MainWindow*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    DlgWindowEntry* e = DlgWindowEntryOf(cx->win);
    DlgWindowCurrentScope current(e ? e->tw : nullptr);
    if (e && e->spec.onKey && e->spec.onKey(e->win, (int)ev->vk, ev->ctrl, ev->shift, ev->alt)) {
        return true;
    }
    if (!e || !e->spec.onArrow || ev->ctrl || ev->alt || ev->shift || IsTrackedPopupOpen(cx)) {
        return false;
    }
    if (ev->vk != VK_UP && ev->vk != VK_DOWN) {
        return false;
    }
    bool editFocused = cx->win->input && cx->win->input->focused;
    return e->spec.onArrow(ev->vk == VK_UP ? -1 : 1, editFocused);
}

static void DlgWindowOnKeyUp(MainWindow*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    DlgWindowEntry* e = DlgWindowEntryOf(cx->win);
    DlgWindowCurrentScope current(e ? e->tw : nullptr);
    if (e && e->spec.onKeyUp) {
        e->spec.onKeyUp(e->win, (int)ev->vk, ev->ctrl, ev->shift, ev->alt);
    }
}

// the main window of an unowned dialog closed and the layer gave the window
// to another one
static void DlgWindowOnOwnerClosed(MainWindow* newOwner) {
    for (DlgWindowEntry* e : gDlgWindows) {
        if (e->win == newOwner || ToolWindowOwner(e->tw) != newOwner) {
            continue;
        }
        e->win = newOwner;
        if (e->spec.onOwnerClosed) {
            e->spec.onOwnerClosed(newOwner);
        }
    }
}

// the close box, or the main window going
static void DlgWindowOnClosed(MainWindow*) {
    Vec<DlgWindowEntry*> entries = gDlgWindows;
    for (DlgWindowEntry* e : entries) {
        if (ToolWindowIsLive(e->tw)) {
            continue;
        }
        void (*close)() = e->spec.close;
        // the window is gone; its address still says which dialog it was
        DlgWindowCurrentScope current(e->tw);
        DlgWindowForget(e);
        if (close) {
            close();
        }
    }
}

static ToolWindowDesc DlgWindowDesc(const DlgWindowSpec& spec) {
    ToolWindowDesc desc = ToolWindowModalDesc(spec.name, spec.title);
    desc.modal = spec.modal ? ToolWinModal::Yes : ToolWinModal::No;
    desc.owner = (spec.modal || spec.owned) ? ToolWinOwner::Owned : ToolWinOwner::TopLevel;
    desc.build = DlgWindowBuild;
    desc.onKey = DlgWindowOnKey;
    desc.onCaptureKey = DlgWindowOnCaptureKey;
    if (spec.onKeyUp) {
        desc.onKeyUp = DlgWindowOnKeyUp;
    }
    if (spec.onOwnerClosed && !spec.modal && !spec.owned) {
        desc.onOwnerClosed = DlgWindowOnOwnerClosed;
    }
    desc.onClosed = DlgWindowOnClosed;
    return desc;
}

ToolWindow* DlgWindowOpen(const DlgWindowSpec& spec, MainWindow* win) {
    if (!ToolWindowsAvailable() || !IsMainWindowValidAndNotClosing(win)) {
        return nullptr;
    }
    // ng: gpui measures a layout only while it paints, so the window is made
    // off screen, and sized and centered once its first frame is known
    ToolWindowDesc desc = DlgWindowDesc(spec);
    int guessDx = spec.clientDx > 0 ? (int)spec.clientDx : kDlgWinGuessDx;
    Size outer = ToolWindowOuterSize(desc, win, Size(guessDx, kDlgWinGuessDy));
    ToolWindow* tw = ToolWindowOpen(desc, win, Rect(kDlgWinOffScreen, kDlgWinOffScreen, outer.dx, outer.dy));
    if (!tw) {
        return nullptr;
    }
    auto* e = new DlgWindowEntry();
    e->tw = tw;
    e->spec = spec;
    e->win = win;
    VecAppend(gDlgWindows, e);
    return tw;
}

void DlgWindowClose(ToolWindow** tw) {
    if (!tw || !*tw) {
        return;
    }
    for (DlgWindowEntry* e : gDlgWindows) {
        if (e->tw == *tw) {
            DlgWindowForget(e);
            break;
        }
    }
    ToolWindowClose(*tw);
    *tw = nullptr;
}

// from the ui task queue: sizing a window renders it
static void DlgWindowFit(DlgWindowEntry* e) {
    if (!VecContains(gDlgWindows, e)) {
        return;
    }
    e->sizePosted = false;
    if (!ToolWindowIsLive(e->tw) || e->content.h <= 0 || !IsMainWindowValid(e->win)) {
        return;
    }
    Size client((int)(e->width + 0.5f), (int)(std::max(e->content.h, e->comboBottom) + 0.5f));
    if (e->placed) {
        ToolWindowSetClientSize(e->tw, client);
        return;
    }
    e->placed = true;
    ToolWindowMove(e->tw, ToolWindowCenteredRect(DlgWindowDesc(e->spec), e->win, client));
}

static gp::El* DlgWindowFitTo(DlgWindowEntry* e, gp::Ctx* cx, gp::El* col, float width);

gp::El* DlgIntoEl(gp::Ctx* cx, gpc::Dialog* dlg) {
    DlgWindowEntry* e = DlgWindowEntryOf(cx->win);
    if (!e) {
        return dlg->IntoEl(gp::WindowSize(cx->win));
    }
    gp::El* col =
        gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0()->Pad(kDlgWinPad)->Gap(kDlgWinGap)->BoundsOut(&e->content);
    if (dlg->body) {
        col->Child(dlg->body);
    }
    gp::El* footer = dlg->footer;
    if (!footer) {
        // what a Dialog draws without a footer of its own
        const gp::component::DialogButtonProps& bp = dlg->buttonProps;
        footer = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->JustifyEnd()->Gap(8);
        if (bp.showCancel) {
            Str cancelText = bp.cancelText.len > 0 ? FromGpui(bp.cancelText) : Tr("Cancel");
            gp::Listener onCancel = bp.onCancel.IsValid() ? bp.onCancel : bp.onClose;
            footer->Child(DlgAccelEl(cx, gpc::Button::New(cx, GStrL("dlg-cancel"))->WithSize(gp::UiSize::Small),
                                     cancelText, onCancel));
        }
        Str okText = bp.okText.len > 0 ? FromGpui(bp.okText) : Tr("OK");
        footer->Child(DlgAccelEl(cx, gpc::Button::New(cx, GStrL("dlg-ok"))->Primary()->WithSize(gp::UiSize::Small),
                                 okText, bp.onOk));
    }
    col->Child(footer);
    return DlgWindowFitTo(e, cx, col, dlg->width);
}

// asks for the window to be `width` wide and as tall as `col` came out
static gp::El* DlgWindowFitTo(DlgWindowEntry* e, gp::Ctx* cx, gp::El* col, float width) {
    gp::WinSize ws = gp::WindowSize(cx->win);
    e->width = width;
    constexpr float kComboListMargin = 4;
    float comboBottom = DialogComboListBottom(cx->win);
    e->comboBottom = comboBottom > 0 ? comboBottom + kComboListMargin : 0;
    float fitDy = std::max(e->content.h, e->comboBottom);
    bool fits = e->placed && std::abs(ws.dipH - fitDy) < 1 && std::abs(ws.dipW - e->width) < 1;
    if (!fits && !e->sizePosted) {
        e->sizePosted = true;
        if (e->content.h <= 0) {
            // the bounds are this frame's; come back when it was laid out
            ToolWindowInvalidate(e->tw);
        }
        uitask::Post(MkFunc0(DlgWindowFit, e), "DlgWindowFit");
    }
    return gp::Div(cx->a)->FlexCol()->SizeFull()->Child(col);
}

float DlgWinFont(gp::Ctx* cx) {
    return kDlgWinFontPx * ToolWindowSetUiFontPx(cx, kDlgWinFontPx);
}

float DlgWinTextDx(gp::Ctx* cx, Str label) {
    DlgWindowEntry* e = DlgWindowEntryOf(cx->win);
    int dpi = std::max(e ? AppShellWindowDpi(e->win) : 96, 96);
    TempStr noAmp = str::ReplaceTemp(label, StrL("&"), StrL(""));
    int dx = PlatformFontMeasureText(GetDefaultGuiFont(), noAmp).dx;
    return (float)dx * 96.f / (float)dpi;
}

gp::El* DlgWinButton(gp::Ctx* cx, gp::Str id, Str label, gp::Listener onClick, bool isDefault, bool disabled) {
    gpc::Button* b = gpc::Button::New(cx, id)->Disabled(disabled);
    if (isDefault) {
        b->Primary();
    }
    float dx = DlgWinTextDx(cx, label) + 2 * kDlgWinBtnPadDx;
    gp::El* el = DlgAccelEl(cx, b, label, onClick, disabled)->H(kDlgWinBtnDy)->W(dx)->PadX(0)->Shrink0();
    if (isDefault) {
        // orig's gColsBtnDefault
        el->Border(1, ToGpui(ThemeHotEdgeColor()));
    }
    return el;
}

// orig's kColListSel / kColListSelFocused
gp::Rgba DlgWinListSelBg(bool listFocused) {
    constexpr int kListSelPct = 25;
    constexpr int kListSelFocusedPct = 45;
    return ToGpui(AccentColor(ThemeWindowControlBackgroundColor(), listFocused ? kListSelFocusedPct : kListSelPct));
}

gp::El* DlgWinLabel(gp::Ctx* cx, gp::Str text, float font, float padB) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    return gp::Div(cx->a)
        ->FlexRow()
        ->ItemsCenter()
        ->H(kDlgWinLineDy + padB)
        ->PadB(padB)
        ->Shrink0()
        ->Child(DlgAccelText(cx, text)->Font(font)->Fg(th.foreground));
}

gp::El* DlgWinEdit(gp::Ctx* cx, gp::Str id, gp::InputState* edit) {
    return gp::Div(cx->a)
        ->FlexRow()
        ->ItemsCenter()
        ->W(gp::kFill)
        ->H(kDlgWinEditDy)
        ->Shrink0()
        ->Child(gpc::Input::New(cx, id, edit)->WithSize(gp::UiSize::Small)->W(gp::kFill)->IntoEl()->H(kDlgWinEditDy));
}

gp::El* DlgWinCheck(gp::Ctx* cx, gp::El* box, float padT) {
    constexpr float kCheckLabelGap = 2;
    return gp::Div(cx->a)
        ->FlexRow()
        ->ItemsCenter()
        ->W(gp::kFill)
        ->H(kDlgWinCheckDy + padT)
        ->PadT(padT)
        ->Shrink0()
        ->Child(box->Gap(kCheckLabelGap));
}

gp::El* DlgWinButtonRow(gp::Ctx* cx, float padY) {
    return gp::Div(cx->a)
        ->FlexRow()
        ->JustifyEnd()
        ->ItemsCenter()
        ->W(gp::kFill)
        ->H(kDlgWinBtnDy + 2 * padY)
        ->Gap(kDlgWinBtnGap)
        ->Shrink0();
}

gp::El* DlgWinContent(gp::Ctx* cx, gp::El* col) {
    DlgWindowEntry* e = DlgWindowEntryOf(cx->win);
    if (!e) {
        return col;
    }
    gp::El* box = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0()->BoundsOut(&e->content)->Child(col);
    return DlgWindowFitTo(e, cx, box, e->spec.clientDx);
}

gp::El* DialogsBuild(MainWindow* win, gp::Ctx* cx) {
    if (gp::El* e = UnsavedAnnotationsDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = SavePathDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = GoToPageDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = AddFavoriteDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = PropertiesDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = AboutDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = TabGroupsDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = GetPasswordDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = CustomZoomDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = ChangeScrollbarDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = ChangeLanguageDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = ChangeThemeDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = ChangeColorDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = InverseSearchDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = EbookSettingsDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = SettingsDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = AdvancedSettingsDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = PageGridDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = SignDocumentDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = PdfToolDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = ImageEditWindowBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = SetScreenshotHotkeyDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = KeyboardHelpBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = AIChatNotInstalledDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = SelectionTranslateDialogBuild(win, cx)) {
        return e;
    }
    if (gp::El* e = SimpleBrowserWindowBuild(win, cx)) {
        return e;
    }
    return MsgBoxBuild(win, cx);
}

// orig's dialogs with a list let Up / Down drive it from anywhere in the dialog
bool DialogsOnArrowKey(MainWindow*, int dir, bool editFocused) {
    if (CustomZoomMoveSelection(dir)) {
        return true;
    }
    if (ChangeScrollbarMoveSelection(dir)) {
        return true;
    }
    if (ChangeThemeMoveSelection(dir)) {
        return true;
    }
    // Up/Down from the search box move the list, like the command palette
    // (ChangeLanguageWnd::OnKeyDown, AdvancedSettingsWnd::HandleUpDownKey);
    // from anywhere else they are the focused control's
    if (!editFocused) {
        return false;
    }
    if (ChangeLanguageMoveSelection(dir)) {
        return true;
    }
    return AdvancedSettingsMoveSelection(dir);
}

DialogAccels DialogsAccelTable(MainWindow* win) {
    // Only the properties window and its children use the edit table
    if (IsPropertiesDialogVisible()) {
        return DialogAccels::Edit;
    }
    bool any = IsMsgBoxVisible() || IsSimpleBrowserWindowVisible() || IsAIChatNotInstalledDialogVisible() ||
               IsSelectionTranslateDialogVisible() || IsAboutWindowVisible() || IsAddFavoriteDialogVisible() ||
               IsGoToPageDialogVisible() || IsTabGroupsDialogVisible() || IsGetPasswordDialogVisible() ||
               IsCustomZoomDialogVisible() || IsChangeScrollbarDialogVisible() || IsChangeLanguageDialogVisible() ||
               IsChangeThemeDialogVisible() || IsChangeColorDialogVisible() || IsInverseSearchDialogVisible() ||
               IsSignDocumentDialogVisible() || IsPdfToolDialogVisible() || IsImageEditWindowVisible() ||
               IsSetScreenshotHotkeyDialogVisible() || IsEbookSettingsDialogVisible() || IsSettingsDialogVisible() ||
               IsAdvancedSettingsDialogVisible() || IsPageGridDialogVisible() || IsKeyboardHelpVisible() ||
               IsUnsavedAnnotationsDialogVisible() || IsSavePathDialogVisible();
    (void)win;
    return any ? DialogAccels::None : DialogAccels::All;
}

bool DialogsOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) {
    bool noMods = !ctrl && !shift && !alt;
    // win32's message box: &Yes / &No answer to their letters
    if (IsMsgBoxVisible()) {
        int kind = MsgBoxButtonKind();
        bool isYesNo = kind == (int)MbYesNo || kind == (int)MbYesNoCancel;
        if (isYesNo && !ctrl && (vk == 'Y' || vk == 'N')) {
            FinishMsgBox(vk == 'Y' ? MbRetYes : MbRetNo);
            return true;
        }
        return false;
    }
    // the manual window: closeOnCtrlW and closeOnF1, and no closeOnEsc
    if (IsSimpleBrowserWindowVisible()) {
        bool isCtrlW = vk == 'W' && ctrl && !alt;
        if (isCtrlW || (vk == VK_F1 && noMods)) {
            SimpleBrowserWindowClose();
            return true;
        }
        return false;
    }
    if (IsSelectionTranslateDialogVisible()) {
        if (vk == 'W' && ctrl && !alt) {
            CloseSelectionTranslateDialog();
            return true;
        }
        return false;
    }
    // orig's WndProcAbout: Ctrl + C copies the about info
    if (IsAboutWindowVisible()) {
        if (vk == 'C' && ctrl) {
            CopyAboutInfoToClipboard(win);
            return true;
        }
        return false;
    }
    if (IsImageEditWindowVisible()) {
        return ImageEditOnKeyDown(win, vk, ctrl);
    }
    if (IsKeyboardHelpVisible()) {
        return KeyboardHelpOnKeyDown(vk);
    }
    // Page Up / Page Down skip the edit accelerator table; send next/prev page
    // (PdfDeletePageDialog::PreTranslate)
    if (IsPdfToolDialogVisible() && PdfToolDialogIsPagesKind() && noMods && (vk == VK_NEXT || vk == VK_PRIOR)) {
        ExecuteCmd(win, vk == VK_NEXT ? (int)CmdGoToNextPage : (int)CmdGoToPrevPage);
        return true;
    }
    return false;
}

// Enter outside a text field: orig's default button
bool DialogsOnEnter(MainWindow*) {
    if (IsMsgBoxVisible()) {
        AcceptMsgBox();
        return true;
    }
    if (SelectionTranslateOnEnter()) {
        return true;
    }
    if (IsCustomZoomDialogVisible()) {
        CustomZoomOk();
        return true;
    }
    if (IsChangeScrollbarDialogVisible()) {
        ChangeScrollbarOk();
        return true;
    }
    if (IsChangeLanguageDialogVisible()) {
        ChangeLanguageOk();
        return true;
    }
    if (IsChangeThemeDialogVisible()) {
        ChangeThemeOk();
        return true;
    }
    if (PdfToolDialogOnEnter()) {
        return true;
    }
    return AdvancedSettingsOnEnter();
}

// --- the "Unsaved changes" dialog -------------------------------------------

// ng: orig asks with a win32 TASKDIALOG whose four buttons are the four
// choices. A gpui AlertDialog has two, so this is a component::Dialog with a
// footer of its own; same buttons, same order, Cancel the default.
struct UnsavedDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Str fileName; // owned
    Func1<int> onChoice;
};

static UnsavedDlg gUnsaved;

struct UnsavedView {
    static void OnSaveExisting(UnsavedView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnSaveNew(UnsavedView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnDiscard(UnsavedView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(UnsavedView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<UnsavedView> gUnsavedView;

bool IsUnsavedAnnotationsDialogVisible() {
    return gUnsaved.visible;
}

static void FinishUnsaved(SaveChoice choice) {
    if (!gUnsaved.visible) {
        return;
    }
    gUnsaved.visible = false;
    Func1<int> onChoice = gUnsaved.onChoice;
    gUnsaved.onChoice = {};
    str::Free(gUnsaved.fileName);
    gUnsaved.fileName = {};
    AppShellInvalidate(gUnsaved.win);
    onChoice.Call((int)choice);
}

void UnsavedView::OnSaveExisting(UnsavedView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishUnsaved(SaveChoice::SaveExisting);
    gp::Notify(cx);
}

void UnsavedView::OnSaveNew(UnsavedView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishUnsaved(SaveChoice::SaveNew);
    gp::Notify(cx);
}

void UnsavedView::OnDiscard(UnsavedView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishUnsaved(SaveChoice::Discard);
    gp::Notify(cx);
}

void UnsavedView::OnCancel(UnsavedView*, gp::Ctx* cx, const gp::ClickEvent*) {
    FinishUnsaved(SaveChoice::Cancel);
    gp::Notify(cx);
}

void ShowUnsavedAnnotationsDialog(MainWindow* win, Str fileName, Func1<int> onChoice) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        onChoice.Call((int)SaveChoice::Cancel);
        return;
    }
    if (gUnsaved.visible) {
        FinishUnsaved(SaveChoice::Cancel);
    }
#if OS_WIN
    // orig's task dialog, where the system's can be shown
    if (NativeMsgBoxEnabled()) {
        logf("ShowUnsavedAnnotationsDialog (system): '%s'\n", fileName);
        NativeUnsavedDialog(win, fileName, onChoice);
        return;
    }
#endif
    gUnsaved.win = win;
    gUnsaved.fileName = str::Dup(fileName);
    gUnsaved.onChoice = onChoice;
    gUnsaved.visible = true;
    logf("ShowUnsavedAnnotationsDialog: '%s'\n", fileName);
    AppShellInvalidate(win);
}

gp::El* UnsavedAnnotationsDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gUnsaved.visible || gUnsaved.win != win) {
        return nullptr;
    }
    if (!gUnsavedView.IsValid()) {
        gUnsavedView = gp::EntityNewState<UnsavedView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(6)->W(gp::kFill);
    TempStr instr = fmt(Tr("Unsaved changes in '%s'").s, gUnsaved.fileName);
    body->Child(gp::TextEl(cx->a, GpuiDup(cx->a, instr))->Font(14)->Bold()->Fg(th.foreground)->Wrap());
    body->Child(gp::TextEl(cx->a, ToGpui(Tr("Save changes?")))->Font(13)->Fg(th.mutedFg));

    auto button = [&](Str id, Str label, bool primary, gp::Listener onClick) {
        gpc::Button* b = gpc::Button::New(cx, GpuiDup(cx->a, id))->WithSize(gp::UiSize::Small);
        if (primary) {
            b->Primary();
        }
        return DlgAccelEl(cx, b, label, onClick);
    };
    // one button per row: the four labels do not fit side by side in a dialog
    gp::El* footer = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Gap(6);
    footer->Child(button(StrL("unsaved-existing"), Tr("&Save to existing PDF"), false,
                         gp::ListenTo(gUnsavedView, &UnsavedView::OnSaveExisting)));
    footer->Child(button(StrL("unsaved-new"), Tr("Save to &new PDF"), false,
                         gp::ListenTo(gUnsavedView, &UnsavedView::OnSaveNew)));
    footer->Child(button(StrL("unsaved-discard"), Tr("&Discard changes"), false,
                         gp::ListenTo(gUnsavedView, &UnsavedView::OnDiscard)));
    footer->Child(
        button(StrL("unsaved-cancel"), Tr("&Cancel"), true, gp::ListenTo(gUnsavedView, &UnsavedView::OnCancel)));

    return gpc::Dialog::New(cx)
        ->Open(true)
        ->Title(ToGpui(Tr("Unsaved changes")))
        ->Body(body)
        ->W(420)
        ->Footer(footer)
        ->OnClose(gp::ListenTo(gUnsavedView, &UnsavedView::OnCancel))
        ->IntoEl(gp::WindowSize(cx->win));
}

// orig gives every dialog closeOnEsc; the message box answers Cancel
bool DialogsOnEscape(MainWindow* win) {
    if (IsMsgBoxVisible()) {
        CancelMsgBox();
        return true;
    }
    // orig's task dialog: TDF_ALLOW_DIALOG_CANCELLATION, Esc is Cancel
    if (IsUnsavedAnnotationsDialogVisible()) {
        FinishUnsaved(SaveChoice::Cancel);
        return true;
    }
    // orig's manual window has no closeOnEsc: only Ctrl + W and F1 close it
    if (IsSimpleBrowserWindowVisible()) {
        return true;
    }
    if (IsAIChatNotInstalledDialogVisible()) {
        CloseAIChatNotInstalledDialog();
        return true;
    }
    if (IsSelectionTranslateDialogVisible()) {
        CloseSelectionTranslateDialog();
        return true;
    }
    if (IsAboutWindowVisible()) {
        CloseAboutWindow();
        return true;
    }
    if (IsPropertiesDialogVisible()) {
        DeletePropertiesWindow(win);
        return true;
    }
    if (IsAddFavoriteDialogVisible()) {
        CloseAddFavoriteDialog();
        return true;
    }
    if (IsGoToPageDialogVisible()) {
        CloseGoToPageDialog();
        return true;
    }
    if (IsTabGroupsDialogVisible()) {
        CloseTabGroupsDialog();
        return true;
    }
    if (IsGetPasswordDialogVisible()) {
        CloseGetPasswordDialog();
        return true;
    }
    if (IsCustomZoomDialogVisible()) {
        CloseCustomZoomDialog();
        return true;
    }
    if (IsChangeScrollbarDialogVisible()) {
        CloseChangeScrollbarDialog();
        return true;
    }
    if (IsChangeLanguageDialogVisible()) {
        CloseChangeLanguageDialog();
        return true;
    }
    if (IsChangeThemeDialogVisible()) {
        CloseChangeThemeDialog();
        return true;
    }
    if (IsChangeColorDialogVisible()) {
        CloseChangeColorDialog();
        return true;
    }
    if (IsInverseSearchDialogVisible()) {
        CloseInverseSearchDialog();
        return true;
    }
    if (IsSignDocumentDialogVisible()) {
        CloseSignDocumentDialog(win);
        return true;
    }
    if (IsPdfToolDialogVisible()) {
        // Esc ends a drag rather than closing the dialog (MergePdfWnd::OnKey)
        if (!PdfToolDialogEndDrag()) {
            ClosePdfToolDialog();
        }
        return true;
    }
    if (IsImageEditWindowVisible()) {
        ImageEditOnEscape();
        return true;
    }
    if (IsSetScreenshotHotkeyDialogVisible()) {
        CloseSetScreenshotHotkeyDialog();
        return true;
    }
    if (IsEbookSettingsDialogVisible()) {
        // orig: closeOnEsc = gSettings->escToExit
        if (gSettings->escToExit) {
            CloseEbookSettingsDialog();
        }
        return true;
    }
    if (IsSettingsDialogVisible()) {
        CloseSettingsDialog();
        return true;
    }
    if (IsAdvancedSettingsDialogVisible()) {
        AdvancedSettingsOnEscape();
        return true;
    }
    if (IsPageGridDialogVisible()) {
        ClosePageGridDialog();
        return true;
    }
    if (IsKeyboardHelpVisible()) {
        CloseKeyboardHelp();
        return true;
    }
    return false;
}
