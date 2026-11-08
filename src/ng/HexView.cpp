/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#include "base/File.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "Translations.h"
#include "Theme.h"
#include "Notifications.h"
#include "gui/AppShell.h"
#include "gui/GpuiTheme.h"
#include "HexViewCommon.h"
#include "HexView.h"

#include "SumatraLog.h"

// ng: the hex view tab, ported from fm's hex_view.cpp. As many bytes per row
// and rows as fit the canvas; it scrolls by byte offset with its own scroll
// bar, because a float scroll offset can't tell rows apart in a huge file.

constexpr float kHexFontSize = 13;
constexpr float kHexGap = 16;
constexpr float kHexPadX = 10;
constexpr float kHexRowH = 20;
constexpr float kHexScrollW = 12;
constexpr float kHexMinThumbH = 24;
constexpr float kHexStatusDy = 22;

// where the last frame put the rows and the scroll track, per window
struct HexGeom {
    MainWindow* win = nullptr;
    gp::Bounds view;
    gp::Bounds track;
};

static Vec<HexGeom*> gHexGeoms;

static HexGeom* GeomFor(MainWindow* win) {
    for (HexGeom* g : gHexGeoms) {
        if (g->win == win) {
            return g;
        }
    }
    auto* g = new HexGeom();
    g->win = win;
    VecAppend(gHexGeoms, g);
    return g;
}

static void HexRelayout(MainWindow* win) {
    AppShellInvalidate(win);
}

static HexView* HexOfWindow(MainWindow* win) {
    if (!IsMainWindowValid(win)) {
        return nullptr;
    }
    WindowTab* tab = win->CurrentTab();
    return tab && tab->IsHexTab() ? tab->hex : nullptr;
}

struct HexEnt {
    static void OnWheel(HexEnt*, gp::Ctx* cx, const gp::ScrollWheelEvent* ev, int64_t arg);
    static void OnTrackDown(HexEnt*, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t arg);
    static void OnTrackDrag(HexEnt*, gp::Ctx* cx, const gp::DragMoveEvent* ev, int64_t arg);
};

static gp::Entity<HexEnt> gHexEnt;

static float HexThumbH(HexView* hex, const gp::Bounds& track) {
    u64 total = (hex->size + (u64)hex->perRow - 1) / (u64)hex->perRow;
    double part = total > 0 ? (double)hex->rows / (double)total : 1;
    float h = track.h * (float)std::min(part, 1.0);
    return std::max(h, kHexMinThumbH);
}

static void HexDragTo(MainWindow* win, HexView* hex, float y) {
    const gp::Bounds& track = GeomFor(win)->track;
    float thumb = HexThumbH(hex, track);
    float room = track.h - thumb;
    if (room <= 0) {
        return;
    }
    double frac = (double)(y - track.y - thumb / 2) / (double)room;
    frac = std::clamp(frac, 0.0, 1.0);
    HexSetTop(hex, (u64)(frac * (double)HexLastTop(hex)));
}

void HexEnt::OnWheel(HexEnt*, gp::Ctx* cx, const gp::ScrollWheelEvent* ev, int64_t arg) {
    MainWindow* win = (MainWindow*)arg;
    HexView* hex = HexOfWindow(win);
    if (!hex) {
        return;
    }
    const_cast<gp::ScrollWheelEvent*>(ev)->propagate = false;
    hex->wheelRem += ev->deltaY;
    int rows = (int)(hex->wheelRem / kHexRowH);
    if (rows == 0) {
        return;
    }
    hex->wheelRem -= (float)rows * kHexRowH;
    HexScrollRows(hex, -rows);
    gp::Notify(cx);
}

void HexEnt::OnTrackDown(HexEnt*, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t arg) {
    MainWindow* win = (MainWindow*)arg;
    HexView* hex = HexOfWindow(win);
    if (!hex) {
        return;
    }
    HexDragTo(win, hex, ev->y);
    gp::Notify(cx);
}

void HexEnt::OnTrackDrag(HexEnt*, gp::Ctx* cx, const gp::DragMoveEvent* ev, int64_t arg) {
    MainWindow* win = (MainWindow*)arg;
    HexView* hex = HexOfWindow(win);
    if (!hex) {
        return;
    }
    HexDragTo(win, hex, ev->event.y);
    gp::Notify(cx);
}

bool HexViewOnKeyDown(MainWindow* win, int vk) {
    HexView* hex = HexOfWindow(win);
    if (!hex) {
        return false;
    }
    switch (vk) {
        case VK_UP:
            HexScrollRows(hex, -1);
            break;
        case VK_DOWN:
            HexScrollRows(hex, 1);
            break;
        case VK_PRIOR:
            HexScrollPages(hex, -1);
            break;
        case VK_NEXT:
            HexScrollPages(hex, 1);
            break;
        case VK_HOME:
            HexSetTop(hex, 0);
            break;
        case VK_END:
            HexSetTop(hex, hex->size);
            break;
        default:
            return false;
    }
    return true;
}

gp::El* HexViewBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gHexEnt.IsValid()) {
        gHexEnt = gp::EntityNewState<HexEnt>(cx->app);
    }
    HexView* hex = HexOfWindow(win);
    if (!hex) {
        return gp::Div(cx->a)->Flex1()->W(gp::kFill)->Bg(ThemeGpuiCanvasBg());
    }
    HexGeom* geom = GeomFor(win);
    gp::Arena* a = cx->a;
    const gp::Theme& th = gp::ThemeNow(cx->app);
    intptr_t arg = (intptr_t)win;

    // fit bytes per row and rows to what the previous frame laid out
    int digits = HexOffsetDigits(hex->size);
    if (geom->view.w > 0 && cx->win) {
        int weight = gp::kFontMono | gp::FontFamilyBits(gp::FontFamilyIntern(th.monoFontFamily));
        gp::Str sample = GStrL("0123456789ABCDEF");
        float charW = gp::MeasureText(&cx->win->paint, sample, kHexFontSize, 0, false, weight, 0).w / 16.f;
        hex->perRow = HexPerRowFor(geom->view.w - 2 * kHexPadX - 2, charW, kHexGap, digits);
    }
    if (geom->view.h > 0) {
        // the first line is the column header
        int fit = (int)(geom->view.h / kHexRowH) - 1;
        hex->rows = std::max(fit, 1);
    } else {
        // gpui lays out while it paints: fit rows to this frame on the next one
        PostForWindow(win, HexRelayout, "HexRelayout");
    }
    const u8* bytes = nullptr;
    int got = HexBytesShown(hex, &bytes);
    int perRow = hex->perRow;

    auto mono = [&](Str text, gp::Rgba color) {
        return gp::TextEl(a, GpuiDup(a, text))
            ->Font(kHexFontSize)
            ->Mono()
            ->FontFamily(th.monoFontFamily)
            ->Fg(color)
            ->Shrink0();
    };
    gp::El* lines = gp::Div(a)
                        ->FlexCol()
                        ->Grow()
                        ->Basis(0)
                        ->MinW(0)
                        ->H(gp::kFill)
                        ->ClipY()
                        ->PadX(kHexPadX)
                        ->BoundsOut(&geom->view)
                        ->OnScrollWheel(gp::ListenTo(gHexEnt, &HexEnt::OnWheel, arg));
    {
        str::Builder blank(GetTempArena());
        for (int i = 0; i < digits; i++) {
            blank.AppendChar(' ');
        }
        lines->Child(gp::Div(a)
                         ->FlexRow()
                         ->H(kHexRowH)
                         ->Shrink0()
                         ->ItemsCenter()
                         ->Gap(kHexGap)
                         ->Child(mono(blank.TakeStr(), th.blue))
                         ->Child(mono(HexColumnsHeaderTemp(perRow), th.blue))
                         ->Child(mono(Tr("Text"), th.blue)));
    }
    gp::Rgba markBg = gp::RgbaOpacity(th.warning, 0.5f);
    for (int r = 0; r < hex->rows; r++) {
        int at = r * perRow;
        if (at >= got) {
            break;
        }
        int n = std::min(got - at, perRow);
        u64 offset = hex->top + (u64)at;
        // the byte gone to, if it is in this row
        int from = -1;
        if (hex->hasMark && hex->mark >= offset && hex->mark < offset + (u64)n) {
            from = (int)(hex->mark - offset);
        }
        auto column = [&](Str text, int partFrom, int partLen, gp::Rgba color) -> gp::El* {
            if (partFrom < 0) {
                return mono(text, color);
            }
            int partTo = partFrom + partLen;
            return gp::Div(a)
                ->FlexRow()
                ->Shrink0()
                ->Child(mono(Str(text.s, partFrom), color))
                ->Child(mono(Str(text.s + partFrom, partLen), th.foreground)->Bg(markBg))
                ->Child(mono(Str(text.s + partTo, len(text) - partTo), color));
        };
        gp::El* row =
            gp::Div(a)
                ->FlexRow()
                ->H(kHexRowH)
                ->Shrink0()
                ->ItemsCenter()
                ->Gap(kHexGap)
                ->Child(mono(HexOffsetTemp(offset, hex->size), th.blue))
                ->Child(column(HexBytesTemp(bytes + at, n, perRow), from < 0 ? -1 : from * 3, 2, th.foreground))
                ->Child(column(HexCharsTemp(bytes + at, n), from, 1, th.mutedFg));
        lines->Child(row);
    }
    if (got == 0 && hex->size > 0) {
        lines->Child(mono(Tr("The file could not be read"), th.foreground));
    }

    gp::El* track = gp::Div(a)
                        ->W(kHexScrollW)
                        ->Shrink0()
                        ->H(gp::kFill)
                        ->BoundsOut(&geom->track)
                        ->OnMouseDown(gp::ListenTo(gHexEnt, &HexEnt::OnTrackDown, arg))
                        ->OnDragMove(gp::ListenTo(gHexEnt, &HexEnt::OnTrackDrag, arg));
    u64 lastTop = HexLastTop(hex);
    if (lastTop > 0 && geom->track.h > 0) {
        float thumbH = HexThumbH(hex, geom->track);
        float y = (float)((double)hex->top / (double)lastTop * (double)(geom->track.h - thumbH));
        track->Child(gp::Div(a)
                         ->Absolute()
                         ->Left(2)
                         ->Top(y)
                         ->W(kHexScrollW - 4)
                         ->H(thumbH)
                         ->Radius(3)
                         ->Bg(gp::RgbaOpacity(th.mutedFg, 0.4f)));
    }

    gp::Rgba fg = ToGpui(ThemeWindowTextColor());
    gp::El* status = gp::Div(a)
                         ->FlexRow()
                         ->ItemsCenter()
                         ->W(gp::kFill)
                         ->H(kHexStatusDy)
                         ->Shrink0()
                         ->PadX(8)
                         ->Gap(16)
                         ->Bg(ToGpui(ThemeControlBackgroundColor()))
                         ->BorderT(1, th.border)
                         ->Child(gp::TextEl(a, GpuiDup(a, hex->path))->Font(12)->Fg(fg)->Flex1()->MinW(0)->Truncate())
                         ->Child(gp::TextEl(a, GpuiDup(a, HexPositionTemp(hex, got)))->Font(12)->Fg(fg)->Shrink0());

    return gp::Div(a)
        ->FlexCol()
        ->Flex1()
        ->MinH(0)
        ->W(gp::kFill)
        ->Bg(ThemeGpuiCanvasBg())
        ->Child(gp::Div(a)->FlexRow()->W(gp::kFill)->Grow()->Basis(0)->MinH(0)->PadT(4)->Child(lines)->Child(track))
        ->Child(status);
}

// shows the tab's file as hex: in a new tab, or in the tab itself if its
// document failed to load
void ViewTabAsHex(WindowTab* tab) {
    if (!tab || len(tab->filePath) == 0 || tab->IsHexTab()) {
        return;
    }
    MainWindow* win = tab->win;
    TempStr path = str::DupTemp(tab->filePath);
    HexView* hex = HexViewOpen(path);
    if (!hex) {
        TempStr msg = fmt(Tr("Couldn't open '%s'").s, path);
        ShowPlainWarningNotification(win, msg, kNotif5SecsTimeOut);
        return;
    }

    bool inPlace = !tab->ctrl && tab->loadState == WindowTab::LoadState::Error;
    if (!inPlace) {
        SaveCurrentWindowTab(win);
        tab = new WindowTab(win);
        tab->SetFilePath(path);
    }
    // after SetFilePath(), which makes it a document tab
    tab->type = WindowTab::Type::HexView;
    tab->hex = hex;
    if (inPlace) {
        tab->loadState = WindowTab::LoadState::None;
    } else {
        AddTabToWindow(win, tab);
    }
    LoadModelIntoTab(tab);
    AppShellInvalidate(win);
}
