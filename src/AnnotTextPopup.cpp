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
// between the author and the date on the header line
constexpr int kHeaderGap = 12;
// between the header line and the rule under it
constexpr int kRuleGap = 4;
constexpr int kCornerRadius = 6;
// the card is as wide as the comment's longest line; lines longer than this
// wrap, so a comment with no line breaks doesn't span the canvas
constexpr int kMaxLineChars = 80;
constexpr int kMinWidth = 120;
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

// the date is secondary information: same hue, less contrast
static Color PopupMutedText() {
    float units = IsLightColor(PopupBg()) ? 55.0f : -55.0f;
    return AdjustLightness2(PopupText(), units);
}

// the rule under the header: a mid-tone that reads on both a light and a dark
// card (the window edge color is nearly invisible on white)
static Color PopupRuleColor() {
    float units = IsLightColor(PopupBg()) ? 190.0f : -190.0f;
    return AdjustLightness2(PopupText(), units);
}

// author on the left, date on the right, with a rule underneath. It's a single
// control rather than an HBox of two labels so that a long author name
// ellipsizes instead of making the card wider than the comment needs
struct AnnotPopupHeader : VirtCustom {
    Str author;
    Str date;
    PlatformFont* authorFont = nullptr;
    PlatformFont* dateFont = nullptr;

    ~AnnotPopupHeader() override {
        str::Free(author);
        str::Free(date);
    }
};

static void PaintPopupHeader(AnnotPopupHeader* h, VirtPaintCtx* ctx) {
    Rect r = ctx->bounds;
    int ruleDy = DpiScale(1);
    int textDy = r.dy - DpiScale(kRuleGap) - ruleDy;
    if (textDy <= 0) {
        return;
    }
    int dateDx = 0;
    if (len(h->date) > 0) {
        dateDx = ctx->gfx->MeasureText(h->date, h->dateFont).dx;
        Rect dr{r.x + r.dx - dateDx, r.y, dateDx, textDy};
        ctx->gfx->DrawText(h->date, dr, gfxTextRight | gfxTextVCenter, h->dateFont, PopupMutedText());
    }
    int authorDx = r.dx - dateDx - (dateDx > 0 ? DpiScale(kHeaderGap) : 0);
    if (authorDx > 0 && len(h->author) > 0) {
        Rect ar{r.x, r.y, authorDx, textDy};
        ctx->gfx->DrawText(h->author, ar, gfxTextEllipsis | gfxTextVCenter, h->authorFont, PopupText());
    }
    ctx->gfx->FillRect({r.x, r.y + r.dy - ruleDy, r.dx, ruleDy}, PopupRuleColor());
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

// the widest card the canvas can hold
static int PopupMaxWidth(MainWindow* win) {
    Rect canvas = HwndClientRect(win->hwndCanvas);
    return std::max(canvas.dx - DpiScale(24), DpiScale(kMinWidth));
}

// width of the longest of the LF-separated lines
static int LongestLineDx(PlatformFont* font, Str s) {
    int dx = 0;
    while (len(s) > 0) {
        int n = str::IndexOfChar(s, '\n');
        Str line = n < 0 ? s : Str(s.s, n);
        dx = std::max(dx, PlatformFontMeasureText(font, line).dx);
        if (n < 0) {
            break;
        }
        s = Str(s.s + n + 1, len(s) - n - 1);
    }
    return dx;
}

// who wrote the comment; the annotation's type name when it has no author, so
// the header never comes up empty
static TempStr PopupAuthorTemp(Annotation* annot) {
    Str author = Author(annot);
    if (len(author) == 0) {
        return AnnotationReadableNameTemp(Type(annot));
    }
    return str::DupTemp(author);
}

// local time, like other apps show comment timestamps
static TempStr PopupDateTemp(Annotation* annot) {
    time_t secs = ModificationDate(annot);
    if (secs == 0) {
        return {};
    }
    struct tm tm;
    if (localtime_s(&tm, &secs) != 0) {
        return {};
    }
    char buf[64];
    size_t n = strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &tm);
    return str::DupTemp(Str(buf, (int)n));
}

// the header's ideal width fits author and date without eliding
static AnnotPopupHeader* MakePopupHeader(AnnotTextPopup* popup, Annotation* annot) {
    auto* h = new AnnotPopupHeader();
    h->author = str::Dup(PopupAuthorTemp(annot));
    h->date = str::Dup(PopupDateTemp(annot));
    h->authorFont = GetBoldPlatformFont(popup->font);
    h->dateFont = popup->font;
    int dx = PlatformFontMeasureText(h->authorFont, h->author).dx;
    if (len(h->date) > 0) {
        dx += DpiScale(kHeaderGap) + PlatformFontMeasureText(h->dateFont, h->date).dx;
    }
    int lineDy = PlatformFontLineHeight(popup->font);
    h->idealSize = {dx, lineDy + DpiScale(kRuleGap) + DpiScale(1)};
    h->onPaint = MkFunc1(PaintPopupHeader, h);
    h->SetFlag(vwfNoHitTest, true);
    return h;
}

static void BuildPopup(AnnotTextPopup* popup, Annotation* annot) {
    int margin = DpiScale(kMargin);
    auto* header = MakePopupHeader(popup, annot);

    // CRLF is what a win32 edit expects; annotation text uses bare LF
    TempStr s = str::DupTemp(Contents(annot));
    str::NormalizeNewlinesToLFInPlace(s);
    int lineDx = LongestLineDx(popup->font, s);
    s = str::LFToCRLFTemp(s);
    popup->edit->SetText(s);

    // as wide as the longest line or the header, capped at kMaxLineChars; the
    // extra px keeps the edit from wrapping a line that measured exactly
    int maxLineDx = popup->font->averageCharWidth * kMaxLineChars;
    LRESULT margins = SendMessageW(popup->edit->hwnd, EM_GETMARGINS, 0, 0);
    int textDx = std::max(lineDx, header->idealSize.dx);
    textDx = std::min(textDx, maxLineDx) + LOWORD(margins) + HIWORD(margins) + DpiScale(2);
    textDx = std::max(textDx, DpiScale(kMinWidth));
    textDx = std::min(textDx, PopupMaxWidth(popup->win) - (2 * margin));
    header->idealSize.dx = textDx;

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
    // a multi-line edit always has WS_VSCROLL; show the bar only when the
    // comment really is taller than the card
    bool scrolls = nLines > maxLines;
    ShowScrollBar(popup->edit->hwnd, SB_VERT, scrolls);
    // the bar takes its width from the text: widen so lines wrap as counted
    int slotDx = scrolls ? textDx + DpiGetSystemMetrics(SM_CXVSCROLL) : textDx;
    slot->idealSize = {slotDx, textDy};

    auto* column = new VBox();
    column->alignMain = MainAxisAlign::MainStart;
    column->alignCross = CrossAxisAlign::Stretch;
    column->gap = DpiScale(kGap);
    column->AddChild(header);
    column->AddChild(slot);

    auto* content = new Padding(column, Insets{margin, margin, margin, margin});
    popup->size = popup->host->SetLayoutSizedToContent(content);
    popup->host->ClipToRoundedRect(kCornerRadius, popup->size);
    popup->annot = annot;
    popup->tab = popup->win->CurrentTab();
    popup->annotBounds = GetRect(annot);
}

// right of the annotation when it fits, else below it, flipping above it when
// there is no room, and always inside the canvas
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
    int x = annotRect.x + annotRect.dx + gap;
    int y = annotRect.y;
    if (x + width > canvas.x + canvas.dx) {
        x = annotRect.x;
        y = annotRect.y + annotRect.dy + gap;
        if (y + height > canvas.y + canvas.dy) {
            y = annotRect.y - gap - height;
        }
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
    // no frame and the card's own background: this is a comment to read, not
    // a text field to type into
    args.withFrame = false;
    // the themed edit draws its own border; we want bare text
    args.noTheme = true;
    args.idealSizeLines = 6;
    args.textPadding = 0;
    args.font = popup->font;
    args.isRtl = IsUIRtl();
    auto* edit = new Edit();
    edit->SetColors(PopupText(), PopupBg());
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
    // the card has the whole text; the one-line tooltip would cover it
    win->DeleteToolTip();
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

bool IsAnnotationTextPopupShownFor(MainWindow* win, Annotation* annot) {
    return annot && IsAnnotationTextPopupShown(win) && win->annotTextPopup->annot == annot;
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
