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
#include "Annotation.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "AppSettings.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"

#include "AnnotTextPopup.h"

// A floating card with the annotation's whole text, opened by clicking the
// annotation. The text sits in a read-only multi-line edit, which wraps,
// scrolls and lets the text be selected and copied without any of it being
// editable.

constexpr const WCHAR* kAnnotTextPopupClassName = L"SumatraAnnotTextPopup";

constexpr int kMargin = 8;
constexpr int kGap = 5;
constexpr int kCornerRadius = 6;
// the card is anchored to the annotation, so it is sized to read comfortably
// rather than to the annotation's own width
constexpr int kIdealWidth = 380;
constexpr int kMinWidth = 200;
// how much of the canvas the card may cover before the text starts scrolling
constexpr int kMaxHeightPercent = 60;
// a card shorter than this looks like a glitch, however short the comment
constexpr int kMinLines = 3;

// Native read-only edit hosted in the VirtHost; the layout slot positions its
// HWND, the way the contents editor's does
struct AnnotTextSlot : VirtCustom {
    Edit* edit = nullptr;

    Size GetIdealSize() override { return idealSize; }

    void SetBounds(Rect r) override {
        VirtCtrl::SetBounds(r);
        if (edit) {
            edit->SetBounds(r);
        }
    }
};

struct AnnotTextPopup {
    MainWindow* win = nullptr;
    VirtHost* host = nullptr;
    Edit* edit = nullptr;
    PlatformFont* font = nullptr;
    // the annotation the card belongs to; it follows this one and closes when
    // the annotation, its tab or its page goes away
    Annotation* annot = nullptr;
    WindowTab* tab = nullptr;
    RectF annotBounds;
    Size size;
    Rect lastPlaced;
    bool closing = false;
    Func1List<MainWindow*> onWindowMoved;
};

bool AnnotationHasText(Annotation* annot) {
    if (!AnnotationIsLive(annot)) {
        return false;
    }
    return len(Contents(annot)) > 0;
}

static Color PopupBg() {
    return ThemeNotificationsBackgroundColor();
}

static Color PopupText() {
    return ThemeNotificationsTextColor();
}

static void PaintPopupBg(AnnotTextPopup*, VirtHostPaintEvent* ev) {
    int radius = DpiScale(kCornerRadius);
    ev->gfx->FillRoundedRect(ev->clientRect, radius, PopupBg(), ThemeEdgeColor());
}

static void PostedHidePopup(MainWindow* win) {
    AnnotTextPopup* popup = win ? win->annotTextPopup : nullptr;
    // clicking a second annotation kills the focus of the first card and then
    // opens a new one; without this the queued hide would close that new card
    if (!popup || !popup->closing) {
        return;
    }
    HideAnnotationTextPopup(win);
}

static void QueueHide(AnnotTextPopup* popup) {
    if (!popup || popup->closing || !popup->win) {
        return;
    }
    popup->closing = true;
    uitask::Post(MkFunc0(PostedHidePopup, popup->win), "HideAnnotTextPopup");
}

static void OnPopupEditWndProc(AnnotTextPopup* popup, ControlBase::WndProcEvent* ev) {
    if (!popup || !popup->edit) {
        return;
    }
    if (ev->msg == WM_CHAR && ev->wparam == VK_ESCAPE) {
        // eat it so the edit doesn't beep after KEYDOWN queued the close
        ev->didHandle = true;
        ev->result = 0;
        return;
    }
    if (ev->msg == WM_KEYDOWN && ev->wparam == VK_ESCAPE) {
        ev->didHandle = true;
        ev->result = 0;
        QueueHide(popup);
        return;
    }
    if (ev->msg == WM_KILLFOCUS) {
        // clicking anywhere else dismisses the card
        QueueHide(popup);
    }
    popup->edit->WndProc(ev);
}

static void OnPopupNativeMsg(AnnotTextPopup* popup, VirtHostNativeMsg* ev) {
    if (!popup || !popup->edit || !popup->edit->hwnd) {
        return;
    }
    if (ev->msg == WM_COMMAND && (HWND)ev->lp == popup->edit->hwnd) {
        popup->edit->DispatchCommand(ev->wp, ev->lp);
        ev->didHandle = true;
        ev->res = 0;
        return;
    }
    // A read-only edit asks its parent for its colors with CTLCOLORSTATIC.
    // Without reflecting it back the edit inherits the card's background and
    // the text field stops looking like one.
    if (ev->msg == WM_CTLCOLOREDIT || ev->msg == WM_CTLCOLORSTATIC) {
        if ((HWND)ev->lp == popup->edit->hwnd) {
            ev->res = popup->edit->DispatchMessageReflect(ev->msg, ev->wp, ev->lp);
            ev->didHandle = ev->res != 0;
        }
    }
}

static AnnotTextPopup* GetOrCreatePopup(MainWindow* win) {
    if (win->annotTextPopup) {
        return win->annotTextPopup;
    }
    auto* popup = new AnnotTextPopup();
    popup->win = win;
    popup->font = GetAppFontForDpi(DpiGetForHwnd(win->hwndCanvas));

    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStr(kAnnotTextPopupClassName);
    args.initialSize = {1, 1};
    args.bgColor = PopupBg();
    args.isPopup = true;
    args.visible = false;
    args.userData = popup;
    popup->host = VirtHost::Create(args);
    if (!popup->host) {
        delete popup;
        return nullptr;
    }
    HWND hwnd = popup->host->native;
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_CLIPCHILDREN);
    popup->host->onPaintBackground = MkFunc1(PaintPopupBg, popup);
    popup->host->onNativeMsg = MkFunc1(OnPopupNativeMsg, popup);
    popup->onWindowMoved = MkFunc1Void(RepositionAnnotationTextPopup);
    win->RegisterOnWindowMoved(&popup->onWindowMoved);
    win->annotTextPopup = popup;
    return popup;
}

// the card's own width, clamped to what the canvas can hold
static int PopupWidth(MainWindow* win) {
    Rect canvas = HwndClientRect(win->hwndCanvas);
    int wantDx = DpiScale(kIdealWidth);
    int maxDx = std::max(canvas.dx - DpiScale(24), DpiScale(kMinWidth));
    return std::min(wantDx, maxDx);
}

// "Note" or "Note by Jane Doe", so it is clear whose comment this is
static TempStr PopupTitleTemp(Annotation* annot) {
    TempStr type = AnnotationReadableNameTemp(Type(annot));
    Str author = Author(annot);
    if (len(author) == 0) {
        return type;
    }
    return fmt("%s — %s", type, author);
}

static void BuildPopup(AnnotTextPopup* popup, Annotation* annot) {
    int margin = DpiScale(kMargin);
    int width = PopupWidth(popup->win);

    auto* title = NewVirtText({
        .s = PopupTitleTemp(annot),
        .font = GetBoldPlatformFont(popup->font),
        .textColor = PopupText(),
        .ellipsis = true,
    });

    // CRLF is what a win32 edit expects; annotation text uses bare LF
    Str s = Contents(annot);
    s = str::ReplaceTemp(s, StrL("\r\n"), StrL("\n"));
    s = str::ReplaceTemp(s, StrL("\n"), StrL("\r\n"));
    popup->edit->SetText(s);

    int textDx = width - (2 * margin);
    popup->edit->idealDx = textDx;
    auto* slot = new AnnotTextSlot();
    slot->edit = popup->edit;

    // The card grows to the comment instead of showing a fixed number of
    // lines: give the edit its final width so the text wraps the way it will
    // be read, then ask it how many lines that came to.
    Rect canvas = HwndClientRect(popup->win->hwndCanvas);
    int maxTextDy = std::max(canvas.dy * kMaxHeightPercent / 100, DpiScale(80));
    popup->edit->SetBounds(Rect(0, 0, textDx, maxTextDy));
    popup->edit->idealSizeLines = 1;
    int lineDy = popup->edit->GetIdealSize().dy;
    int nLines = (int)SendMessageW(popup->edit->hwnd, EM_GETLINECOUNT, 0, 0);
    int maxLines = lineDy > 0 ? std::max(maxTextDy / lineDy, kMinLines) : kMinLines;
    popup->edit->idealSizeLines = std::max(kMinLines, std::min(nLines, maxLines));
    int textDy = std::min(popup->edit->GetIdealSize().dy, maxTextDy);
    slot->idealSize = {textDx, textDy};

    auto* column = new VBox();
    column->alignMain = MainAxisAlign::MainStart;
    column->alignCross = CrossAxisAlign::Stretch;
    column->gap = DpiScale(kGap);
    column->AddChild(title);
    column->AddChild(slot);

    auto* content = new Padding(column, Insets{margin, margin, margin, margin});
    popup->size = popup->host->SetLayoutSizedToContent(content);
    popup->host->ClipToRoundedRect(kCornerRadius, popup->size);
    popup->annot = annot;
    popup->tab = popup->win->CurrentTab();
    popup->annotBounds = GetRect(annot);
}

// below the annotation, flipping above it when there is no room, and always
// inside the canvas
static bool PositionPopup(AnnotTextPopup* popup) {
    MainWindow* win = popup ? popup->win : nullptr;
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    Annotation* annot = popup ? popup->annot : nullptr;
    if (!dm || !AnnotationIsLive(annot) || !dm->PageVisible(PageNo(annot))) {
        return false;
    }

    Rect canvas = HwndClientRect(win->hwndCanvas);
    Rect annotRect = dm->CvtToScreen(PageNo(annot), GetRect(annot));
    if (canvas.Intersect(annotRect).IsEmpty()) {
        return false;
    }

    int gap = DpiScale(6);
    int width = popup->size.dx;
    int height = std::min(popup->size.dy, canvas.dy);
    int x = annotRect.x;
    int y = annotRect.y + annotRect.dy + gap;
    if (y + height > canvas.y + canvas.dy) {
        y = annotRect.y - gap - height;
    }

    int maxX = canvas.x + canvas.dx - width;
    x = std::max(canvas.x, std::min(x, maxX));
    int maxY = canvas.y + canvas.dy - height;
    y = std::max(canvas.y, std::min(y, maxY));

    Point screen = HwndClientToScreen(win->hwndCanvas, Point(x, y));
    Rect placed(screen.x, screen.y, width, height);
    if (placed != popup->lastPlaced) {
        popup->host->SetPos(placed, true);
        popup->lastPlaced = placed;
    }
    return true;
}

bool ShowAnnotationTextPopup(MainWindow* win, Annotation* annot) {
    if (!win || !win->hwndCanvas || !win->AsFixed() || !AnnotationHasText(annot)) {
        return false;
    }
    AnnotTextPopup* popup = GetOrCreatePopup(win);
    if (!popup) {
        return false;
    }
    HideAnnotationTextPopup(win); // start from a clean card, keep the host

    Edit::CreateArgs args;
    args.parent = popup->host->native;
    args.isMultiLine = true;
    args.withFrame = true;
    args.idealSizeLines = 6;
    args.textPadding = 3;
    args.font = popup->font;
    args.isRtl = IsUIRtl();
    auto* edit = new Edit();
    bool isDark = !IsLightColor(ThemeWindowBackgroundColor());
    Color bg = isDark ? ThemeWindowControlBackgroundColor() : MkRgb(255, 255, 255);
    edit->SetColors(PopupText(), bg);
    if (!edit->Create(args)) {
        delete edit;
        return false;
    }
    // read-only, so the card can never edit the document by accident
    SendMessageW(edit->hwnd, EM_SETREADONLY, TRUE, 0);
    edit->onWndProc = MkFunc1(OnPopupEditWndProc, popup);
    popup->edit = edit;
    popup->closing = false;

    BuildPopup(popup, annot);
    if (!PositionPopup(popup)) {
        HideAnnotationTextPopup(win);
        return false;
    }
    popup->host->Show(true);
    popup->host->Invalidate(false);
    SetActiveWindow(popup->host->native);
    edit->SetFocus();
    // the caret belongs at the start: this is text to read, not to replace
    SendMessageW(edit->hwnd, EM_SETSEL, 0, 0);
    return true;
}

void HideAnnotationTextPopup(MainWindow* win) {
    AnnotTextPopup* popup = win ? win->annotTextPopup : nullptr;
    if (!popup) {
        return;
    }
    popup->host->Show(false);
    // the layout owns the slot, which only borrows the edit's HWND
    popup->host->SetLayout(nullptr);
    delete popup->edit;
    popup->edit = nullptr;
    popup->annot = nullptr;
    popup->tab = nullptr;
    popup->annotBounds = {};
    popup->lastPlaced = {};
    popup->closing = false;
}

bool IsAnnotationTextPopupShown(MainWindow* win) {
    AnnotTextPopup* popup = win ? win->annotTextPopup : nullptr;
    return popup && popup->host->IsVisible();
}

void RepositionAnnotationTextPopup(MainWindow* win) {
    AnnotTextPopup* popup = win ? win->annotTextPopup : nullptr;
    if (!popup || !popup->host->IsVisible()) {
        return;
    }
    bool sameTab = popup->tab == win->CurrentTab();
    if (!sameTab || !PositionPopup(popup)) {
        HideAnnotationTextPopup(win);
    }
}

void DeleteAnnotationTextPopup(MainWindow* win) {
    AnnotTextPopup* popup = win ? win->annotTextPopup : nullptr;
    if (!popup) {
        return;
    }
    win->annotTextPopup = nullptr;
    win->UnregisterOnWindowMoved(&popup->onWindowMoved);
    popup->host->SetLayout(nullptr);
    delete popup->edit;
    delete popup->host;
    delete popup;
}
