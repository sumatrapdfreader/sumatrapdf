/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "EngineBase.h"
#include "DocController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SumatraDialogs.h"
#include "shared/PdfToolsCommon.h"
#include "PagePosition.h"

//   Go to page:   12                   (of 300)
//   ---------------------------------------------- hairline
//                          [Cancel] [Go to page]
// A chaptered document gets a Chapter line over the Page line.
constexpr int kGoToDlgDx = 340;
constexpr int kGoToEditChars = 6;
struct GoToPageWnd : WindowBase {
    ~GoToPageWnd() override = default;

    MainWindow* win = nullptr;
    int pageCount = 0;
    bool onlyNumeric = true;
    bool hasChapters = false;
    Edit* editPage = nullptr;
    VirtText* labelOf = nullptr;
    // chapter row; only built when hasChapters was true at Create() time
    Edit* editChapter = nullptr;
    VirtText* chapterLabelOf = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnGo = nullptr;
    PlatformFont* fontText = nullptr;
    PlatformFont* fontQuery = nullptr;
    PlatformFont* fontMeta = nullptr;
    Color colTxt = kColorUnset;
    Color colBg = kColorUnset;
    Color colMuted = kColorUnset;
    Vec<VirtText*> labels;

    bool Create(MainWindow* win);
    Edit* AddLine(VBox* vbox, Str label, Str text, bool numbersOnly, VirtText** ofOut, int ofCount);
    void SetTarget(MainWindow* win);

    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnOk(VirtMouseEvent* ev = nullptr);
};

static GoToPageWnd* gGoToPageWnd = nullptr;

static void ClearGoToPageWnd() {
    gGoToPageWnd = nullptr;
}

// caller (ShowGoToPageDialog) only reuses this window when hasChapters still
// matches, so the row layout built in Create() stays valid here
void GoToPageWnd::SetTarget(MainWindow* mainWin) {
    win = mainWin;
    pageCount = 0;
    onlyNumeric = true;
    Str pageLabel;
    int chapterCount = 0;
    int chapterCur = 0;
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded() && win->ctrl) {
        DocController* ctrl = win->ctrl;
        if (hasChapters) {
            Location cur = ctrl->CurrentLocation();
            chapterCount = ctrl->ChapterCount();
            chapterCur = cur.chapter;
            pageCount = ctrl->ChapterPageCount(cur.chapter);
            pageLabel = fmt("%d", cur.page);
        } else {
            pageCount = ctrl->PageCount();
            onlyNumeric = !ctrl->HasPageLabels();
            pageLabel = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        }
    }
    if (labelOf) {
        labelOf->SetText(fmt(Tr("(of %d)").s, pageCount));
    }
    if (editPage) {
        EditSetNumbersOnly(editPage, onlyNumeric);
        editPage->SetText(pageLabel);
        EditSelectAll(editPage);
    }
    if (chapterLabelOf) {
        chapterLabelOf->SetText(fmt(Tr("(of %d)").s, chapterCount));
    }
    if (editChapter) {
        editChapter->SetText(fmt("%d", chapterCur));
    }
}

void GoToPageWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void GoToPageWnd::OnOk(VirtMouseEvent*) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded() || !win->ctrl) {
        ScheduleDelete();
        return;
    }
    if (hasChapters) {
        int chapter = editChapter ? ParseInt(editChapter->GetTextTemp()) : 1;
        int page = editPage ? ParseInt(editPage->GetTextTemp()) : 1;
        Location loc = win->ctrl->ClampLocation({chapter, page});
        win->ctrl->GoToLocation(loc, true);
        ScheduleDelete();
        return;
    }
    TempStr pageLabel = editPage ? editPage->GetTextTemp() : Str{};
    int newPageNo = win->ctrl->GetPageByLabel(pageLabel);
    if (win->ctrl->ValidPageNo(newPageNo)) {
        win->ctrl->GoToPage(newPageNo, true);
    }
    ScheduleDelete();
}

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gGoToPageWnd) {
        gGoToPageWnd->OnCancel();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gGoToPageWnd) {
        gGoToPageWnd->ScheduleDelete();
    }
}

// a line: the label muted, the field in the larger font, "(of N)" at the end
Edit* GoToPageWnd::AddLine(VBox* vbox, Str label, Str text, bool numbersOnly, VirtText** ofOut, int ofCount) {
    HBox* row = NewDlgRow();
    VirtText* t = NewDlgLabel(label, fontText, colMuted, kToolDlgLineDy, true);
    VecAppend(labels, t);
    row->AddChild(t);

    Edit::CreateArgs args;
    args.parent = hwnd;
    args.font = fontQuery;
    args.withBorder = false;
    args.numbersOnly = numbersOnly;
    args.selectAllOnFocus = true;
    args.isRtl = IsUIRtl();
    args.text = text;
    args.idealWidthChars = kGoToEditChars;
    auto* e = new Edit();
    e->SetColors(colTxt, colBg);
    e->Create(args);
    row->AddChild(e, 1);

    *ofOut =
        NewVirtText({.s = fmt(Tr("(of %d)").s, ofCount), .font = fontMeta, .textColor = colMuted, .isRtl = IsUIRtl()});
    row->AddChild(*ofOut);
    vbox->AddChild(new Padding(row, DpiScaledInsets(0, kToolDlgPadDx)));
    return e;
}

bool GoToPageWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    Str pageLabel;
    int chapterCount = 0;
    int chapterCur = 0;
    if (IsMainWindowValidAndNotClosing(win) && win->IsDocLoaded() && win->ctrl) {
        DocController* ctrl = win->ctrl;
        hasChapters = ShowChapterUi(ctrl);
        if (hasChapters) {
            Location cur = ctrl->CurrentLocation();
            chapterCount = ctrl->ChapterCount();
            chapterCur = cur.chapter;
            pageCount = ctrl->ChapterPageCount(cur.chapter);
            pageLabel = fmt("%d", cur.page);
        } else {
            pageCount = ctrl->PageCount();
            onlyNumeric = !ctrl->HasPageLabels();
            pageLabel = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        }
    }

    colTxt = ThemeWindowTextColor();
    colBg = ThemeWindowControlBackgroundColor();
    colMuted = ThemeWindowTextDisabledColor();
    fontText = DlgUiFont(kToolDlgTextFontPx);
    fontQuery = DlgUiFont(kToolDlgQueryFontPx);
    fontMeta = DlgUiFont(kToolDlgMetaFontPx);
    SetFont(fontText);
    {
        CreateCustomArgs args;
        args.owner = win ? win->hwndFrame : nullptr;
        args.title = Tr("Go to page");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION;
        args.font = fontText;
        args.isRtl = IsUIRtl();
        args.bgColor = colBg;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    if (hasChapters) {
        editChapter = AddLine(vbox, Tr("&Chapter:"), fmt("%d", chapterCur), true, &chapterLabelOf, chapterCount);
        vbox->AddChild(new VirtLine());
    }
    Str label = hasChapters ? Tr("&Page:") : Tr("&Go to page:");
    editPage = AddLine(vbox, label, pageLabel, onlyNumeric, &labelOf, pageCount);

    // the fields start in one column
    int labelDx = 0;
    for (VirtText* t : labels) {
        labelDx = std::max(labelDx, t->GetIdealSize().dx);
    }
    for (VirtText* t : labels) {
        t->padding.right += labelDx - t->GetIdealSize().dx;
    }

    btnCancel = NewDlgButton(hwnd, Tr("Cancel"), fontText, DlgButtonStyle::Outline, false);
    btnCancel->onClick = MkMethod1<GoToPageWnd, VirtMouseEvent*, &GoToPageWnd::OnCancel>(this);
    btnGo = NewDlgButton(hwnd, Tr("Go to page"), fontText, DlgButtonStyle::Filled, true);
    btnGo->onClick = MkMethod1<GoToPageWnd, VirtMouseEvent*, &GoToPageWnd::OnOk>(this);
    vbox->AddChild(NewDlgFooter(btnCancel, btnGo));
    layout = vbox;

    LayoutAndSizeToContent(layout, DpiScale(kGoToDlgDx), 0, hwnd);
    DoLayout(HwndClientRect(hwnd).Size());
    HwndCenterDialog(hwnd, win ? win->hwndFrame : nullptr);
    UpdateTheme();

    SetIsVisible(true);
    Edit* focusTarget = hasChapters && editChapter ? editChapter : editPage;
    EditSelectAll(focusTarget);
    EditSetFocus(focusTarget);
    return true;
}

void ShowGoToPageDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return;
    }
    // the chapter row is only built in Create(); rebuild if the doc kind changed
    bool hasChapters = ShowChapterUi(win->ctrl);
    if (gGoToPageWnd && gGoToPageWnd->hasChapters != hasChapters) {
        gGoToPageWnd->ScheduleDelete();
        gGoToPageWnd = nullptr;
    }
    if (gGoToPageWnd) {
        gGoToPageWnd->SetTarget(win);
        if (gGoToPageWnd->hwnd) {
            gGoToPageWnd->DoLayout(HwndClientRect(gGoToPageWnd->hwnd).Size());
            HwndInvalidate(gGoToPageWnd->hwnd);
        }
        HwndSetFocus(gGoToPageWnd->hwnd);
        Edit* focusTarget =
            gGoToPageWnd->hasChapters && gGoToPageWnd->editChapter ? gGoToPageWnd->editChapter : gGoToPageWnd->editPage;
        EditSetFocus(focusTarget);
        return;
    }
    auto* wnd = new GoToPageWnd();
    wnd->closeOnEsc = true;
    wnd->onBeforeDelete = MkFunc0Void(ClearGoToPageWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gGoToPageWnd = wnd;
    RunModalWindow(wnd->hwnd, win ? win->hwndFrame : nullptr);
}
