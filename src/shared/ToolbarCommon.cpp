/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/UIModels.h"
#include "Accelerators.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayMode.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AnnotPlacement.h"
#include "WindowTab.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "SearchAndDDE.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "PagePosition.h"
#include "FindBar.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Theme.h"
#include "ReadAloud.h"
#include "Toolbar.h"
#include "ToolbarCommon.h"

int gLayoutButtonsCount = 0;

// 128 should be more than enough
// we use static array so that we don't have to generate
// code for Vec<ToolbarButtonInfo>

// +1 to ensure there's always space for WarningsMsgId button
ToolbarButtonInfo gCustomButtons[kMaxCustomButtons + 1];

static int gCustomButtonsCount = 0;

// Light theme ControlBackgroundColor is white, which is what the old themed
// rebar/toolbar painted. Other themes use their control background.
Color TbBgColor() {
    return ThemeControlBackgroundColor();
}

Color TbTextColor() {
    if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
        return SysControlTextColor();
    }
    return ThemeWindowTextColor();
}

Color TbDisabledColor() {
    if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
        return SysDisabledTextColor();
    }
    return ThemeWindowTextDisabledColor();
}

Color TbHoverColor() {
    return ThemeHotBackgroundColor();
}

// A ground a shade off the normal one, for telling two areas of a drop-down
// apart. Well short of the hover highlight, which is 20 units off: this is a
// cue, not something lit up.
Color TbSubtleBgColor() {
    return AccentColor(TbBgColor(), 8);
}

Color TbSelectedColor() {
    return AccentColor(TbBgColor(), 28);
}

Color TbEdgeColor() {
    return ThemeEdgeColor();
}

// Old Win32 toolbar: TBMETRICS.cyPad defaults to 6, then we added DpiScale(2).
// TB_SETBUTTONSIZE cannot go below image + 2*cyPad, so that was the bar height.
int ToolbarCyPad() {
    return 6 + DpiScale(2);
}

int ToolbarRowDy(int iconSize) {
    return iconSize + (2 * ToolbarCyPad());
}

bool HasToolbarButtonContent(const ToolbarButtonInfo& tbi) {
    return tbi.icon || tbi.isText || !str::IsEmptyOrWhiteSpace(tbi.svgIcon);
}

int TotalButtonsCount() {
    return gLayoutButtonsCount + gCustomButtonsCount;
}

int OriginalCommandId(int cmdId) {
    CustomCommand* cmd = FindCustomCommand(cmdId);
    return cmd ? cmd->origId : cmdId;
}

TempStr ToolbarTipTemp(int cmdId, Str tip, bool translate) {
    TempStr s = translate ? trans::GetTranslation(tip) : TempStr(tip);
    TempStr accelStr = AppendAccelKeyToMenuStringTemp({}, cmdId);
    if (accelStr) {
        Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
        s = str::JoinTemp(s, fmt(" (%s)", accel));
    }
    return s;
}

// toolbar mode for this window: Fullscreen.Toolbar in fullscreen, else Toolbar
int ToolbarModeForWindow(MainWindow* win) {
    if (win->isFullScreen) {
        return FullscreenToolbarModeFromPrefs();
    }
    return ToolbarModeFromPrefs();
}

// the find UI is now a floating Chrome-style bar (see FindBar.cpp). When the
// toolbar moves/resizes we keep the bar centered over the search icon.
void UpdateToolbarFindText(MainWindow* win) {
    FindBarReposition(win);
}

static TempStr ShortcutToolbarToolTipTemp(Shortcut* shortcut) {
    if (!str::IsEmptyOrWhiteSpace(shortcut->name)) {
        return shortcut->name;
    }
    CustomCommand* cmd = FindCustomCommand(shortcut->cmdId);
    if (cmd && cmd->name) {
        return cmd->name;
    }
    int origId = cmd ? cmd->origId : shortcut->cmdId;
    if (origId > 0 && origId < CmdLast) {
        Str desc = GetCommandDescription(origId);
        if (desc) {
            return desc;
        }
    }
    return shortcut->cmd;
}

static TempStr CustomCommandToolbarToolTipTemp(CustomCommand* cmd, Str fallback) {
    if (cmd && !str::IsEmptyOrWhiteSpace(cmd->name)) {
        return cmd->name;
    }
    if (!str::IsEmptyOrWhiteSpace(fallback)) {
        return fallback;
    }
    return StrL("External Viewer");
}

void PopulateCustomToolbarButtons() {
    gCustomButtonsCount = 0;
    for (Shortcut* shortcut : *gSettings->shortcuts) {
        if (gCustomButtonsCount >= kMaxCustomButtons) {
            break;
        }
        if (!str::IsEmptyOrWhiteSpace(shortcut->toolbarSvgIcon)) {
            ToolbarButtonInfo tbi;
            tbi.cmdId = shortcut->cmdId;
            tbi.svgIcon = shortcut->toolbarSvgIcon;
            tbi.toolTip = ShortcutToolbarToolTipTemp(shortcut);
            gCustomButtons[gCustomButtonsCount++] = tbi;
            continue;
        }
        if (!str::IsEmptyOrWhiteSpace(shortcut->toolbarText)) {
            ToolbarButtonInfo tbi;
            tbi.cmdId = shortcut->cmdId;
            tbi.toolTip = shortcut->toolbarText;
            tbi.isText = true;
            gCustomButtons[gCustomButtonsCount++] = tbi;
        }
    }

    // add toolbar buttons from custom commands with toolbar settings (e.g. ExternalViewers).
    // gFirstCustomCommand is a prepend-only list, so walking it directly yields
    // the commands in reverse creation order and the buttons would show up in
    // the reverse of the order the user listed them in (#5869)
    Vec<CustomCommand*> customCmds;
    for (auto* cc = gFirstCustomCommand; cc; cc = cc->next) {
        VecAppend(customCmds, cc);
    }
    VecReverse(customCmds);
    for (CustomCommand* cc : customCmds) {
        if (gCustomButtonsCount >= kMaxCustomButtons) {
            break;
        }
        Str svgIcon = GetCommandStringArg(cc, kCmdArgToolbarSvgIcon, {});
        Str tbText = GetCommandStringArg(cc, kCmdArgToolbarText, {});
        if (!str::IsEmptyOrWhiteSpace(svgIcon)) {
            ToolbarButtonInfo tbi;
            tbi.cmdId = cc->id;
            tbi.svgIcon = svgIcon;
            tbi.toolTip = CustomCommandToolbarToolTipTemp(cc, tbText);
            gCustomButtons[gCustomButtonsCount++] = tbi;
            continue;
        }
        if (str::IsEmptyOrWhiteSpace(tbText)) {
            continue;
        }
        ToolbarButtonInfo tbi;
        tbi.cmdId = cc->id;
        tbi.toolTip = tbText;
        tbi.isText = true;
        gCustomButtons[gCustomButtonsCount++] = tbi;
    }
}

int ToolbarIconSize() {
    return RoundUp(DpiScale(gSettings->toolbarSize), 4);
}

// The widest row of the pyramid: the smallest w with 1+2+...+w >= n, so the
// rows w, w-1, ... w-k hold every item with only the last one part-full. 26
// items give 7, 6, 5, 4, 3 and a last row of 1.
int HoverPyramidTopRow(int n) {
    int w = 1;
    while (((w * (w + 1)) / 2) < n) {
        w++;
    }
    return w;
}

// What the zoom strip lists: the levels the zoom buttons step through, so a
// click on one of them is the same jump the buttons make in one go, plus the
// two fit modes where 100% is. GetDefaultZoomLevels() is ZoomLevels from the
// settings when it is set, otherwise the built-in list, and GetZoomStepCmdIds()
// has a command for each of them, in the same order.
void ZoomHoverLevels(Vec<ZoomHoverLevel>& out) {
    int n = 0;
    float* levels = GetDefaultZoomLevels(&n);
    Vec<int>* cmdIds = GetZoomStepCmdIds();
    if (!levels || !cmdIds || len(*cmdIds) != n) {
        return;
    }
    bool addedFits = false;
    for (int i = 0; i < n; i++) {
        if (!addedFits && levels[i] > 100) {
            // the fit modes go where their size puts them, i.e. right after
            // 100% in every list that has it
            VecAppend(out, ZoomHoverLevel{kZoomFitPage, CmdZoomFitPage});
            VecAppend(out, ZoomHoverLevel{kZoomFitWidth, CmdZoomFitWidth});
            addedFits = true;
        }
        VecAppend(out, ZoomHoverLevel{levels[i], (*cmdIds)[i]});
    }
    if (!addedFits) {
        VecAppend(out, ZoomHoverLevel{kZoomFitPage, CmdZoomFitPage});
        VecAppend(out, ZoomHoverLevel{kZoomFitWidth, CmdZoomFitWidth});
    }
}

// which of the levels the document is at, exact match only, -1 when it is at
// none of them (a zoom typed into Custom Zoom, or a fit mode not listed)
int ZoomHoverCurrentIdx(MainWindow* win, const Vec<ZoomHoverLevel>& levels) {
    DocController* ctrl = win ? win->ctrl : nullptr;
    if (!ctrl) {
        return -1;
    }
    float current = ctrl->GetZoomVirtual(false);
    // the same fuzz DisplayModel::GetNextZoomStep uses to match a level
    constexpr float kZoomFuzz = 0.01f;
    for (int i = 0; i < len(levels); i++) {
        float zl = levels[i].zoom;
        if (current + kZoomFuzz >= zl && current - kZoomFuzz <= zl) {
            return i;
        }
    }
    return -1;
}

ParsedColor* AnnotPresetColorSetting(int cmdId) {
    if (!gSettings) {
        return nullptr;
    }
    Annotations& a = gSettings->annotations;
    switch (cmdId) {
        // the highlighter makes highlight annotations
        case CmdAnnotationHighlightBrush:
        case CmdCreateAnnotHighlight:
            return &a.highlightColor;
        case CmdCreateAnnotUnderline:
            return &a.underlineColor;
        case CmdCreateAnnotSquiggly:
            return &a.squigglyColor;
        case CmdCreateAnnotStrikeOut:
            return &a.strikeOutColor;
        case CmdCreateAnnotText:
            return &a.textIconColor;
        case CmdCreateAnnotFreeText:
            // the text's color; the box behind it is FreeTextBackgroundColor
            return &a.freeTextColor;
        case CmdCreateAnnotLine:
            return &a.lineColor;
        case CmdCreateAnnotPolyLine:
            return &a.polyLineColor;
        case CmdCreateAnnotSquare:
            return &a.squareColor;
        case CmdCreateAnnotCircle:
            return &a.circleColor;
        case CmdCreateAnnotPolygon:
            return &a.polygonColor;
        case CmdCreateAnnotInk:
            return &a.inkColor;
        case CmdCreateAnnotStamp:
            return &a.stampColor;
        case CmdCreateAnnotCaret:
            return &a.caretColor;
        case CmdCreateAnnotFileAttachment:
            return &a.fileAttachmentColor;
    }
    return nullptr;
}

// What an annotation is made in when its setting is empty: MuPDF's defaults,
// which are also what Acrobat, PDF-XChange and Foxit use
Color AnnotDefaultColor(int cmdId) {
    switch (cmdId) {
        case CmdCreateAnnotText:
        case CmdCreateAnnotFileAttachment:
            return MkRgb(0xff, 0xff, 0);
        case CmdCreateAnnotFreeText:
            return MkRgb(0, 0, 0);
        case CmdCreateAnnotCaret:
            return MkRgb(0, 0, 0xff);
        case CmdCreateAnnotLine:
        case CmdCreateAnnotPolyLine:
        case CmdCreateAnnotSquare:
        case CmdCreateAnnotCircle:
        case CmdCreateAnnotPolygon:
        case CmdCreateAnnotStamp:
            return MkRgb(0xff, 0, 0);
        case CmdCreateAnnotInk:
            // 40% yellow, Annotations.InkColor's default
            return 0x6600ffff;
    }
    return kColorUnset;
}

// The colors a button offers. Ink has its own, translucent ones: they are
// exactly what it paints. cmdId 0 is not a button, and gets the presets
Str* AnnotPresetColorList(int cmdId) {
    if (!gSettings) {
        return nullptr;
    }
    Annotations& a = gSettings->annotations;
    return (cmdId == CmdCreateAnnotInk) ? &a.inkColors : &a.presetColors;
}

void SetAnnotPresetColor(int cmdId, Color col) {
    ParsedColor* setting = AnnotPresetColorSetting(cmdId);
    if (!setting) {
        return;
    }
    SetColorText(*setting, SerializeColorTemp(col));
    ScheduleSaveSettings();
}

// whether the button's annotation can be made out of the text selected right now
bool CanCreateAnnotFromSelection(MainWindow* win, int cmdId) {
    switch (cmdId) {
        case CmdCreateAnnotHighlight:
        case CmdCreateAnnotUnderline:
        case CmdCreateAnnotSquiggly:
        case CmdCreateAnnotStrikeOut:
            break;
        default:
            return false;
    }
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    if (!tab || !win->showSelection || !tab->selectionOnPage) {
        return false;
    }
    DisplayModel* dm = win->AsFixed();
    return dm && dm->textSelection && dm->textSelection->result.len > 0;
}

// alpha 0 and 0xff both mean opaque, so a palette color matches an
// annotation's even when only one of the two spells the alpha out
bool SameColorAndAlpha(Color a, Color b) {
    u8 aa = GetAlpha(a);
    u8 ab = GetAlpha(b);
    if (aa == 0) {
        aa = 0xff;
    }
    if (ab == 0) {
        ab = 0xff;
    }
    return ((a & 0xffffff) == (b & 0xffffff)) && (aa == ab);
}
