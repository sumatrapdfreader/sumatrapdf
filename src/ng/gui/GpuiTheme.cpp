/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Theme.cpp spreads the current theme over gui/GuiColors, the color
// defaults of its win32 virtual controls (UpdateGuiColorsFromTheme()). gpui
// components read one gp::Theme instead, so the same colors are written into
// one here and installed with ThemeInstall(). The derivations (AccentColor of
// the control background by 14 / 20 / 25 / 28 / 36 / 40 / 45 / 60) are orig's,
// slot for slot, so a theme looks the same in both.

#include "gui/GpuiBridge.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Theme.h"
#include "MainWindow.h"
#include "gui/AppShell.h"
#include "gui/GpuiTheme.h"

static gp::Rgba Col(Color c) {
    return ToGpui(c);
}

static gp::Rgba Accent(Color c, int light) {
    return ToGpui(AccentColor(c, light));
}

gp::Rgba ThemeGpuiCanvasBg() {
    Color bg;
    ThemeDocumentColors(bg);
    return ToGpui(bg);
}

// orig's UpdateGuiColorsFromTheme(), slot for slot, onto gpui's theme fields
static void FillTheme(gp::Theme* t) {
    Color text = ThemeWindowTextColor();
    Color disabled = ThemeWindowTextDisabledColor();
    Color darker = ThemeWindowDarkerTextColor();
    Color link = ThemeWindowLinkColor();
    // what dialogs, side panels and the chrome around the document put their
    // controls on
    Color ctlBg = ThemeWindowControlBackgroundColor();
    Color winBg = ThemeWindowBackgroundColor();
    Color edge = ThemeEdgeColor();
    Color hotEdge = ThemeHotEdgeColor();

    // orig's buttons, edits, drop-downs and check boxes are plain rectangles
    // with a 1 px edge (VirtButton::Paint &c). radiusLg stays: it rounds what
    // is a window of its own in orig (dialogs, popup menus), which Windows 11
    // rounds too
    t->radius = 0;

    t->background = Col(ctlBg);
    t->foreground = Col(text);
    t->border = Col(edge);
    t->mutedFg = Col(darker);

    t->inputBorder = Col(edge);
    t->inputBg = Col(ctlBg);
    t->ring = Col(hotEdge);
    t->caret = Col(text);
    t->dragBorder = Col(hotEdge);
    t->windowBorder = Col(edge);

    // gColsListBox
    t->selection = Accent(ctlBg, 25);
    t->list = Col(ctlBg);
    t->listEven = Col(ctlBg);
    t->listHead = Accent(ctlBg, 14);
    t->listHover = Accent(ctlBg, 20);
    t->listActive = Accent(ctlBg, 45);
    t->listActiveBorder = Col(hotEdge);

    t->titleBar = Col(ctlBg);
    t->titleBarBorder = Col(edge);
    t->statusBar = Col(ctlBg);
    t->statusBarBorder = Col(edge);

    // gColsTab; the strip itself carries the window background, as orig's does
    t->tabBar = Col(winBg);
    t->tab = Col(winBg);
    t->tabBarSegmented = Accent(ctlBg, 14);
    t->tabActiveBg = Col(ThemeActiveTabBackgroundColor());
    t->tabActiveFg = Col(text);
    t->tabFg = Col(text);

    t->tableBg = Col(ctlBg);
    t->tableHead = Accent(ctlBg, 14);
    t->tableHeadFg = Col(darker);
    t->tableFoot = Accent(ctlBg, 14);
    t->tableFootFg = Col(darker);
    t->tableRowBorder = Col(edge);
    t->tableEven = Accent(ctlBg, 8);
    t->tableHover = Accent(ctlBg, 20);
    t->tableActive = Accent(ctlBg, 45);
    t->tableActiveBorder = Col(hotEdge);

    // gColsWin: dialogs and popups sit their content on the control background
    t->popover = Col(ctlBg);
    t->popoverFg = Col(text);
    t->accordion = Col(ctlBg);
    t->groupBox = Col(ctlBg);
    t->groupBoxFg = Col(text);
    t->descListLabel = Accent(ctlBg, 10);
    t->descListLabelFg = Col(darker);
    t->tiles = Col(ctlBg);
    t->skeleton = Accent(ctlBg, 14);

    // gColsBtn / gColsBtnDefault / gColsIconBtn
    t->button = Accent(ctlBg, 14);
    t->buttonFg = Col(text);
    t->buttonHover = Accent(ctlBg, 28);
    t->buttonActive = Accent(ctlBg, 36);
    t->secondary = Accent(ctlBg, 14);
    t->secondaryFg = Col(text);
    t->secondaryHover = Accent(ctlBg, 28);
    t->secondaryActive = Accent(ctlBg, 36);
    t->buttonSecondary = t->secondary;
    t->buttonSecondaryFg = t->secondaryFg;
    t->buttonSecondaryHover = t->secondaryHover;
    t->buttonSecondaryActive = t->secondaryActive;
    t->primary = Accent(ctlBg, 26);
    t->primaryFg = Col(text);
    t->primaryHover = Accent(ctlBg, 40);
    t->primaryActive = Accent(ctlBg, 45);
    t->buttonPrimary = t->primary;
    t->buttonPrimaryFg = t->primaryFg;
    t->buttonPrimaryHover = t->primaryHover;
    t->buttonPrimaryActive = t->primaryActive;
    t->muted = Accent(ctlBg, 20);
    t->accent = Accent(ctlBg, 20);
    t->accentFg = Col(text);
    t->dropTarget = Col(MkRgba(GetRed(link), GetGreen(link), GetBlue(link), 0x33));

    // the sidebar is a panel like any other, so it takes the control colors
    t->sidebar = Col(ctlBg);
    t->sidebarFg = Col(text);
    t->sidebarPrimary = Col(text);
    t->sidebarPrimaryFg = Col(ctlBg);
    t->sidebarAccent = Accent(ctlBg, 20);
    t->sidebarAccentFg = Col(text);
    t->sidebarBorder = Col(edge);

    // gColsSlider
    t->sliderBar = Accent(ctlBg, 40);
    t->sliderThumb = Col(text);
    t->switchBg = Accent(ctlBg, 36);
    t->switchThumb = Col(text);

    // gColsListBox[kColListScrollbar]
    t->scrollbarThumb = Accent(ctlBg, 60);
    t->scrollbarThumbHover = Accent(ctlBg, 75);

    // gColsLink / gColsRichText
    t->link = Col(link);
    t->linkHover = Col(link);
    t->linkActive = Col(link);

    // the notification palette (orig's Notifications.cpp colors)
    t->warning = Col(ThemeNotificationsHighlightColor());
    t->warningFg = Col(ThemeNotificationsHighlightTextColor());
    t->progress = Col(ThemeNotificationsProgressColor());
    t->danger = Col(ThemeErrorBackgroundColor());
    t->dangerFg = Col(text);

    // ThemeWindowTextDisabledColor() has no slot of its own in gpui: a
    // disabled element is drawn with mutedFg or with the element's own opacity
    (void)disabled;
}

void ThemeStartPlatformColors() {}

void ThemeApplyPlatformColors() {
    gp::App* app = AppShellGetApp();
    if (!app) {
        return;
    }
    bool isDark = !IsLightColor(ThemeWindowBackgroundColor());
    gp::ThemeMode mode = isDark ? gp::ThemeMode::Dark : gp::ThemeMode::Light;

    gp::Theme t = isDark ? gp::ThemeDefaultDark() : gp::ThemeDefaultLight();
    t.mode = mode;
    FillTheme(&t);
    gp::ThemeTokensReset(&t);

    gp::ThemeInstall(app, mode, t);
    gp::ThemeSet(app, mode);

    for (MainWindow* win : gWindows) {
        AppShellInvalidate(win);
    }
}

void ThemeFinishPlatformColors() {}
