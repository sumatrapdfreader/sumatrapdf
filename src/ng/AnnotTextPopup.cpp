/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AnnotTextPopup.cpp - the read-only card with an annotation's whole
// text, opened by clicking it outside Edit PDF mode. orig hosts a borderless
// multi-line edit in a layered popup window; here the card is an absolutely
// positioned gpui element in the canvas with the same header (author on the
// left, date on the right, a rule under them) and the same colors.

#include "gui/GpuiBridge.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
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
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "AnnotTextPopup.h"

constexpr int kMargin = 8;
constexpr int kRuleGap = 4;
constexpr int kCornerRadius = 6;
// the card is as wide as the comment's longest line; lines longer than this
// wrap, so a comment with no line breaks doesn't span the canvas
constexpr int kMaxLineChars = 80;
constexpr int kMinWidth = 120;
// how much of the canvas the card may cover before the text starts scrolling
constexpr int kMaxHeightPercent = 60;

struct AnnotTextPopup {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr;
    Annotation* annot = nullptr;
    Str author; // owned
    Str date;   // owned
    Str text;   // owned
    gp::Bounds measured;

    ~AnnotTextPopup() {
        str::Free(author);
        str::Free(date);
        str::Free(text);
    }
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

static TempStr AnnotDateTemp(Annotation* annot) {
    time_t secs = ModificationDate(annot);
    if (secs == 0) {
        return {};
    }
    struct tm tm;
#if OS_WIN
    gmtime_s(&tm, &secs);
#else
    gmtime_r(&secs, &tm);
#endif
    char buf[100];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", &tm);
    return str::DupTemp(Str(buf));
}

bool IsAnnotationTextPopupShown(MainWindow* win) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    return p && p->annot != nullptr;
}

bool IsAnnotationTextPopupShownFor(MainWindow* win, Annotation* annot) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    return p && annot && p->annot == annot;
}

void HideAnnotationTextPopup(MainWindow* win) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    if (!p || !p->annot) {
        return;
    }
    p->annot = nullptr;
    p->tab = nullptr;
    AppShellInvalidate(win);
}

void HideAnnotationTextPopupFor(MainWindow* win, Annotation* annot) {
    if (IsAnnotationTextPopupShownFor(win, annot)) {
        HideAnnotationTextPopup(win);
    }
}

void DeleteAnnotationTextPopup(MainWindow* win) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    if (!p) {
        return;
    }
    win->annotTextPopup = nullptr;
    delete p;
}

bool ShowAnnotationTextPopup(MainWindow* win, Annotation* annot) {
    if (!win || !win->AsFixed() || !AnnotationHasText(annot)) {
        return false;
    }
    if (!win->annotTextPopup) {
        auto* p = new AnnotTextPopup();
        p->win = win;
        win->annotTextPopup = p;
    }
    AnnotTextPopup* p = win->annotTextPopup;
    p->annot = annot;
    p->tab = win->CurrentTab();
    // who wrote the comment; the annotation's type name when it has no author, so
    // the header never comes up empty (orig's PopupAuthorTemp)
    Str author = Author(annot);
    if (len(author) == 0) {
        author = AnnotationReadableNameTemp(Type(annot));
    }
    str::ReplaceWithCopy(&p->author, author);
    str::ReplaceWithCopy(&p->date, AnnotDateTemp(annot));
    str::ReplaceWithCopy(&p->text, Contents(annot));
    AppShellInvalidate(win);
    return true;
}

struct AnnotTextPopupView {
    MainWindow* win = nullptr;

    static void OnClose(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<AnnotTextPopupView> gAnnotTextPopupView;

void AnnotTextPopupView::OnClose(AnnotTextPopupView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    HideAnnotationTextPopup(self->win);
    gp::Notify(cx);
}

gp::El* AnnotTextPopupBuild(MainWindow* win, gp::Ctx* cx) {
    AnnotTextPopup* p = win ? win->annotTextPopup : nullptr;
    if (!p || !p->annot) {
        return nullptr;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || win->CurrentTab() != p->tab || !AnnotationIsLive(p->annot) || !dm->PageVisible(PageNo(p->annot))) {
        HideAnnotationTextPopup(win);
        return nullptr;
    }
    if (!gAnnotTextPopupView.IsValid()) {
        gAnnotTextPopupView = gp::EntityNewState<AnnotTextPopupView>(cx->app);
    }
    auto* view = (AnnotTextPopupView*)gp::EntityGet(cx->app, gAnnotTextPopupView.id);
    view->win = win;

    Color bg = PopupBg();
    Color fg = PopupText();
    float margin = (float)DpiScale(kMargin);
    float maxW = (float)std::max(DpiScale(kMinWidth), std::min(win->canvasRc.dx - 32, kMaxLineChars * 8));
    float maxH = (float)(win->canvasRc.dy * kMaxHeightPercent / 100);

    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->Gap(4)
                       ->Pad(margin)
                       ->MaxW(maxW)
                       ->MaxH(maxH)
                       ->Radius((float)DpiScale(kCornerRadius))
                       ->Bg(ToGpui(bg))
                       ->Border(1, ToGpui(ThemeEdgeColor()));
    // author on the left, date on the right, with a rule underneath
    gp::El* header = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(12)->W(gp::kFill);
    Str author = p->author;
    header->Child(gp::TextEl(cx->a, GpuiDup(cx->a, author))->Font(12)->Bold()->Fg(ToGpui(fg))->Flex1()->Truncate());
    if (len(p->date) > 0) {
        header->Child(gp::TextEl(cx->a, GpuiDup(cx->a, p->date))->Font(12)->Fg(ToGpui(PopupMutedText())));
    }
    header->Child(gpc::Button::New(cx, GStrL("annot-popup-close"))
                      ->Icon(gp::IconName::Close)
                      ->Ghost()
                      ->Compact()
                      ->WithSize(gp::UiSize::XSmall)
                      ->Tooltip(ToGpui(Tr("Close")))
                      ->OnClick(gp::ListenTo(gAnnotTextPopupView, &AnnotTextPopupView::OnClose))
                      ->IntoEl());
    card->Child(header);
    card->Child(gp::Div(cx->a)->W(gp::kFill)->H(1)->MarginB((float)DpiScale(kRuleGap))->Bg(ToGpui(PopupRuleColor())));
    card->Child(gp::TextEl(cx->a, GpuiDup(cx->a, p->text))->Font(13)->Fg(ToGpui(fg))->Wrap()->W(gp::kFill));
    card->BoundsOut(&p->measured);

    // under the annotation, above it when there is no room
    float k = CanvasScale(win);
    Rect r = dm->CvtToScreen(PageNo(p->annot), GetRect(p->annot));
    float w = p->measured.w > 0 ? p->measured.w : maxW;
    float h = p->measured.h > 0 ? p->measured.h : 80;
    float gap = (float)DpiScale(6);
    float x = ((float)r.x + (float)r.dx / 2) * k - (w / 2);
    float y = (float)(r.y + r.dy) * k + gap;
    if (y + h > (float)win->canvasRc.dy) {
        y = (float)r.y * k - gap - h;
    }
    x = std::max(std::min(x, (float)win->canvasRc.dx - w), 0.f);
    y = std::max(std::min(y, (float)win->canvasRc.dy - h), 0.f);
    card->Absolute()->Left(x)->Top(y);
    return card;
}
