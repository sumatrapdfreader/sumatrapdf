/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Toolbar.cpp builds a tree of virtual controls in a window of its
// own and keeps it in sync by poking at single items. Here the row is made of
// gpui elements every frame, so the tables, the layout rules, the availability
// / enabled / checked policy and the tooltips are orig's while the painting is
// gpui's.

#include "gui/GpuiBridge.h"
#include "base/File.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "DisplayModel.h"
#include "PagePosition.h"
#include "Commands.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Translations.h"
#include "CommandAvailability.h"
#include "Theme.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "SvgIcons.h"
#include "Menu.h"
#include "ReadAloud.h"
#include "Annotation.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "AnnotPlacement.h"
#include "gui/DialogWidgets.h"
#include "SumatraDialogs.h"
#include "Toolbar.h"

#include "SumatraLog.h"

// those are not real commands but we have to refer to toolbar buttons
// is by a command
constexpr int PageInfoId = (int)CmdLast + 16;
constexpr int WarningMsgId = (int)CmdLast + 17;

constexpr int kButtonSpacingX = 4;

// distance between label and edit field
constexpr int kTextPaddingRight = 6;

struct ToolbarButtonInfo {
    const char* icon = nullptr; // gIcon*, or null for a separator / page box / text
    int cmdId = 0;
    Str toolTip;
    Str svgIcon; // custom SVG from settings
    bool isText = false;
};

static ToolbarButtonInfo gToolbarButtons[] = {
    {gIconFileOpen, CmdOpenFile, TrN("Open")},
    {gIconPrint, CmdPrint, TrN("Print")},
    {nullptr, 0, {}},          // separator
    {nullptr, PageInfoId, {}}, // text box for page number + show current page / no of pages
    {gIconPagePrev, CmdGoToPrevPage, TrN("Previous Page")},
    {gIconPageNext, CmdGoToNextPage, TrN("Next Page")},
    {nullptr, 0, {}}, // separator
    {gIconNavigateBack, CmdNavigateBack, TrN("Back")},
    {gIconNavigateForward, CmdNavigateForward, TrN("Forward")},
    {nullptr, 0, {}}, // separator
    {gIconSpeak, CmdToggleReadAloud, TrN("Read Aloud")},
    {nullptr, 0, {}}, // separator
    {gIconLayoutContinuous, CmdZoomFitWidthAndContinuous, TrN("Fit Width and Show Pages Continuously")},
    {gIconLayoutSinglePage, CmdZoomFitPageAndSinglePage, TrN("Fit a Single Page")},
    {gIconRotateLeft, CmdRotateLeft, TrN("Rotate &Left")},
    {gIconRotateRight, CmdRotateRight, TrN("Rotate &Right")},
    {gIconZoomOut, CmdZoomOut, TrN("Zoom Out")},
    {gIconZoomIn, CmdZoomIn, TrN("Zoom In")},
    {nullptr, 0, {}}, // separator
    {gIconSearch, CmdFindFirst, TrN("Find")},
    {nullptr, 0, {}}, // separator
    {gIconEditAnnotations, CmdToggleEditPDF, TrN("Edit PDF")},
};

constexpr int kButtonsCount = dimof(gToolbarButtons);

// orig's gPdfAnnotationButtons: the "Edit PDF" row under the toolbar
static ToolbarButtonInfo gPdfAnnotationButtons[] = {
    {gIconAnnotHighlightBrush, CmdAnnotationHighlightBrush, TrN("Highlighter: select text to highlight it")},
    {gIconAnnotInk, CmdCreateAnnotInk, TrN("Ink")},
    {gIconAnnotHighlight, CmdCreateAnnotHighlight, TrN("Highlight Selection")},
    {gIconAnnotUnderline, CmdCreateAnnotUnderline, TrN("Underline")},
    {gIconAnnotSquiggly, CmdCreateAnnotSquiggly, TrN("Squiggly")},
    {gIconAnnotStrikeOut, CmdCreateAnnotStrikeOut, TrN("Strike Out")},
    {nullptr, 0, {}},
    {gIconAnnotText, CmdCreateAnnotText, TrN("Text")},
    {gIconAnnotFreeText, CmdCreateAnnotFreeText, TrN("Free Text")},
    {nullptr, 0, {}},
    {gIconAnnotLine, CmdCreateAnnotLine, TrN("Line")},
    {gIconAnnotPolyLine, CmdCreateAnnotPolyLine, TrN("Polyline")},
    {gIconAnnotSquare, CmdCreateAnnotSquare, TrN("Square")},
    {gIconAnnotCircle, CmdCreateAnnotCircle, TrN("Circle")},
    {gIconAnnotPolygon, CmdCreateAnnotPolygon, TrN("Polygon")},
    {nullptr, 0, {}},
    {gIconAnnotRedact, CmdCreateAnnotRedact, TrN("Redact")},
    {gIconApplyRedactions, CmdApplyRedactions, TrN("Apply Redactions")},
    {gIconAnnotStamp, CmdCreateAnnotStamp, TrN("Stamp")},
    {gIconAnnotCaret, CmdCreateAnnotCaret, TrN("Caret")},
    {gIconAnnotFileAttachment, CmdCreateAnnotFileAttachment, TrN("File Attachment")},
    {nullptr, 0, {}},
    {gIconUndo, CmdUndo, TrN("Undo")},
    {gIconRedo, CmdRedo, TrN("Redo")},
    {nullptr, 0, {}},
    {gIconFindAnnotation, CmdFindAnnotation, TrN("Find Annotation")},
    {nullptr, 0, {}},
    // the tooltip names the file, see ToolbarBuild
    {gIconSave, CmdSaveAnnotations, TrN("Save changes to existing PDF")},
};

constexpr int kPdfAnnotationButtonsCount = dimof(gPdfAnnotationButtons);

// The built-in buttons actually on the toolbar, which is gToolbarButtons unless
// ToolbarCustomLayout asks for a different set / order (issue #5095). A layout
// can repeat a button, so allow for more than the default count.
constexpr int kMaxLayoutButtons = 64;
static ToolbarButtonInfo gLayoutButtons[kMaxLayoutButtons];
static int gLayoutButtonsCount = 0;
static Str gLayoutParsedFrom;
static bool gLayoutParsed = false;

// 128 should be more than enough
constexpr int kMaxCustomButtons = 127;
static ToolbarButtonInfo gCustomButtons[kMaxCustomButtons + 1];
static int gCustomButtonsCount = 0;

// --- colors -----------------------------------------------------------------

// Light theme ControlBackgroundColor is white, which is what the old themed
// rebar/toolbar painted. Other themes use their control background.
static Color TbBgColor() {
    return ThemeControlBackgroundColor();
}

static Color TbTextColor() {
    if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
        return SysControlTextColor();
    }
    return ThemeWindowTextColor();
}

static Color TbDisabledColor() {
    if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
        return SysDisabledTextColor();
    }
    return ThemeWindowTextDisabledColor();
}

static Color TbHoverColor() {
    return ThemeHotBackgroundColor();
}

// orig's TbSubtleBgColor: a cue, well short of the hover highlight
static Color TbSubtleBgColor() {
    return AccentColor(TbBgColor(), 8);
}

static Color TbSelectedColor() {
    return AccentColor(TbBgColor(), 28);
}

static Color TbEdgeColor() {
    return ThemeEdgeColor();
}

// --- sizes ------------------------------------------------------------------

// Old Win32 toolbar: TBMETRICS.cyPad defaults to 6, then we added DpiScale(2).
// TB_SETBUTTONSIZE cannot go below image + 2*cyPad, so that was the bar height.
static int ToolbarCyPad() {
    return 6 + DpiScale(2);
}

static int ToolbarRowDy(int iconSize) {
    return iconSize + (2 * ToolbarCyPad());
}

int ToolbarIconSize() {
    return RoundUp(DpiScale(gSettings->toolbarSize), 4);
}

static bool HasToolbarButtonContent(const ToolbarButtonInfo& tbi) {
    return tbi.icon || tbi.isText || !str::IsEmptyOrWhiteSpace(tbi.svgIcon);
}

// --- the button tables ------------------------------------------------------

// Work out which built-in buttons the toolbar has, and in which order. Empty
// ToolbarCustomLayout (the default) means the standard layout; otherwise the
// setting lists the buttons the user wants: a command name puts that button
// there, `|` a separator, `PageInfo` the page number box, and leaving a button
// out is how you hide it (issue #5095).
static void PopulateToolbarLayout() {
    Str setting = gSettings->toolbarCustomLayout;
    if (gLayoutParsed && str::Eq(setting, gLayoutParsedFrom)) {
        return;
    }
    str::Free(gLayoutParsedFrom);
    gLayoutParsedFrom = str::Dup(setting);
    gLayoutParsed = true;
    gLayoutButtonsCount = 0;

    auto addButton = [](const ToolbarButtonInfo& tbi) {
        if (gLayoutButtonsCount < kMaxLayoutButtons) {
            gLayoutButtons[gLayoutButtonsCount++] = tbi;
        }
    };
    auto useDefaultLayout = [&addButton]() {
        for (const ToolbarButtonInfo& tbi : gToolbarButtons) {
            addButton(tbi);
        }
    };

    if (str::IsEmptyOrWhiteSpace(setting)) {
        useDefaultLayout();
        return;
    }

    // commas and semicolons are a natural way to write a list, so accept them
    TempStr normalized = str::ReplaceTemp(setting, StrL(","), StrL(" "));
    normalized = str::ReplaceTemp(normalized, StrL(";"), StrL(" "));
    StrVec names;
    Split(&names, normalized, StrL(" "), true);
    for (Str name : names) {
        Str tok = name;
        str::TrimWSInPlace(tok, str::TrimOpt::Both);
        if (len(tok) == 0) {
            continue;
        }
        if (str::Eq(tok, StrL("|")) || str::EqI(tok, StrL("Separator"))) {
            addButton({nullptr, 0, {}});
            continue;
        }
        if (str::EqI(tok, StrL("PageInfo"))) {
            addButton({nullptr, PageInfoId, {}});
            continue;
        }
        int cmdId = GetCommandIdByName(tok);
        const ToolbarButtonInfo* found = nullptr;
        for (int i = 0; i < kButtonsCount && cmdId != CmdNone; i++) {
            if (gToolbarButtons[i].cmdId == cmdId) {
                found = &gToolbarButtons[i];
                break;
            }
        }
        if (!found) {
            logf("ToolbarCustomLayout: no built-in toolbar button for '%s'\n", tok);
            continue;
        }
        addButton(*found);
    }
    if (gLayoutButtonsCount == 0) {
        logf("ToolbarCustomLayout: nothing usable in '%s', using the standard layout\n", setting);
        useDefaultLayout();
    }
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

static void PopulateCustomToolbarButtons() {
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

    // add toolbar buttons from custom commands with toolbar settings (e.g.
    // ExternalViewers). gFirstCustomCommand is a prepend-only list, so walking
    // it directly yields the commands in reverse creation order (#5869)
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

static int TotalButtonsCount() {
    return gLayoutButtonsCount + gCustomButtonsCount;
}

static ToolbarButtonInfo& GetToolbarButtonInfoByIdx(int idx) {
    if (idx < gLayoutButtonsCount) {
        return gLayoutButtons[idx];
    }
    return gCustomButtons[idx - gLayoutButtonsCount];
}

static int OriginalCommandId(int cmdId) {
    CustomCommand* cmd = FindCustomCommand(cmdId);
    return cmd ? cmd->origId : cmdId;
}

// --- availability and enabled state -----------------------------------------

// some commands are only avialble in certain contexts
// we remove toolbar buttons for un-availalbe commands
static bool IsCmdAvailable(MainWindow* win, int cmdId, AppCommandCtx* ctx) {
    switch (cmdId) {
        case CmdZoomFitWidthAndContinuous:
        case CmdZoomFitPageAndSinglePage:
        case CmdRotateLeft:
        case CmdRotateRight:
            return !IsBrowserDocController(win->ctrl);
        case CmdFindFirst:
            // CHM has its own find bar even though NeedsFindUI() is false for
            // it; show the Search button so it's reachable
            return NeedsFindUI(win) || IsBrowserDocController(win->ctrl);
        case CmdFindNext:
        case CmdFindPrev:
        case CmdFindToggleMatchCase:
        case CmdFindToggleMatchWholeWord:
            return NeedsFindUI(win);
        case CmdToggleReadAloud:
            // opt-in: the button and its drop-down only show if asked for
            return TtsIsAvailable() && gSettings->toolbarShowReadAloud;
        case PageInfoId:
            return true;
    }
    // Toolbar buttons stay visible (but disabled) when no document is open, so
    // decide visibility as if a document were loaded; otherwise the no-document
    // gate in GetCommandVisibility would remove them.
    bool savedLoaded = ctx->isDocLoaded;
    ctx->isDocLoaded = true;
    bool remove, disable;
    GetCommandIdState(ctx, cmdId, &remove, &disable);
    ctx->isDocLoaded = savedLoaded;
    return !remove;
}

static bool IsCmdEnabled(MainWindow* win, int cmdId, AppCommandCtx* ctx) {
    switch (cmdId) {
        case CmdNextTab:
        case CmdPrevTab:
        case CmdNextTabSmart:
        case CmdPrevTabSmart:
            return SettingsUseTabs();
        case PageInfoId:
            return true;
    }

    bool remove, disable;
    GetCommandIdState(ctx, cmdId, &remove, &disable);
    if (remove || disable) {
        return false;
    }
    switch (cmdId) {
        case CmdOpenFile:
        case CmdOpenFileNoHistory:
            if (!CanAccessDisk()) {
                return false;
            }
            break;
        case CmdPrint:
            if (!HasPermission(Perm::PrinterAccess)) {
                return false;
            }
            break;
    }

    // if no file is open, only enable buttons for commands that don't require a
    // document (issue #5657)
    if (!win->IsDocLoaded()) {
        return CmdWorksWithoutDocument(OriginalCommandId(cmdId));
    }

    switch (cmdId) {
        case CmdOpenFile:
        case CmdOpenFileNoHistory:
            // opening different files isn't allowed in plugin mode
            return !gPluginMode;

#ifndef DISABLE_DOCUMENT_RESTRICTIONS
        case CmdPrint:
            return !win->AsFixed() || win->AsFixed()->GetEngine()->AllowsPrinting();
#endif

        case CmdFindFirst:
            return NeedsFindUI(win) || IsBrowserDocController(win->ctrl);

        case CmdFindNext:
        case CmdFindPrev: {
            if (FindEditTextLen(win) == 0) {
                return false;
            }
            // When we already know there are zero matches, disable next/prev.
            if (win->ctrl && win->ctrl->CanFindInPage()) {
                return win->browserFindTotal != 0;
            }
            if (win->findCountValid && len(win->findCountPositions) == 0) {
                return false;
            }
            return true;
        }

        case CmdGoToNextPage:
            return win->ctrl->CurrentPageNo() < win->ctrl->PageCount();
        case CmdGoToPrevPage:
            return win->ctrl->CurrentPageNo() > 1;

        case CmdNavigateBack:
            return win->ctrl->CanNavigate(-1);
        case CmdNavigateForward:
            return win->ctrl->CanNavigate(1);

        default:
            return true;
    }
}

static bool IsCmdChecked(MainWindow* win, int cmdId) {
    if (!win->IsDocLoaded()) {
        return false;
    }
    DisplayMode dm = win->ctrl->GetDisplayMode();
    float zoomVirtual = win->ctrl->GetZoomVirtual();
    switch (cmdId) {
        case CmdZoomFitWidthAndContinuous:
            return dm == DisplayMode::Continuous && zoomVirtual == kZoomFitWidth;
        case CmdZoomFitPageAndSinglePage:
            return dm == DisplayMode::SinglePage && zoomVirtual == kZoomFitPage;
        default:
            return false;
    }
}

static TempStr ToolbarTipTemp(int cmdId, Str tip, bool translate) {
    TempStr s = translate ? trans::GetTranslation(tip) : TempStr(tip);
    TempStr accelStr = AppendAccelKeyToMenuStringTemp({}, cmdId);
    if (accelStr) {
        Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
        s = str::JoinTemp(s, fmt(" (%s)", accel));
    }
    return s;
}

// --- toolbar mode -----------------------------------------------------------

// toolbar mode for this window: Fullscreen.Toolbar in fullscreen, else Toolbar
static int ToolbarModeForWindow(MainWindow* win) {
    if (win->isFullScreen) {
        return FullscreenToolbarModeFromPrefs();
    }
    return ToolbarModeFromPrefs();
}

bool ShouldShowToolbar(MainWindow* win) {
    if (win->presentation || win->isQuickLook) {
        return false;
    }
    return ToolbarModeForWindow(win) == kToolbarShow;
}

bool ShouldOverlayToolbar(MainWindow* win) {
    if (win->presentation || win->isQuickLook) {
        return false;
    }
    if (ToolbarModeForWindow(win) != kToolbarOverlay) {
        return false;
    }
    // don't float the overlay toolbar over the home / about page
    return !win->IsCurrentTabAbout();
}

// --- annotation colors (orig's) ---------------------------------------------

static ParsedColor* AnnotPresetColorSetting(int cmdId) {
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
static Color AnnotDefaultColor(int cmdId) {
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

// the color the button's next annotation is made in
Color AnnotColorForCmd(int cmdId) {
    ParsedColor* setting = AnnotPresetColorSetting(cmdId);
    Color col = setting ? GetParsedColor(*setting, kColorUnset) : kColorUnset;
    return col != kColorUnset ? col : AnnotDefaultColor(cmdId);
}

// The colors a button offers. Ink has its own, translucent ones: they are
// exactly what it paints. cmdId 0 is not a button, and gets the presets
static Str* AnnotPresetColorList(int cmdId) {
    if (!gSettings) {
        return nullptr;
    }
    Annotations& a = gSettings->annotations;
    return (cmdId == CmdCreateAnnotInk) ? &a.inkColors : &a.presetColors;
}

void AnnotPresetColors(int cmdId, Vec<Color>& out) {
    Str* list = AnnotPresetColorList(cmdId);
    if (!list) {
        return;
    }
    ParseColorList(*list, out, 0);
}

// alpha 0 and 0xff both mean opaque, so a palette color matches an
// annotation's even when only one of the two spells the alpha out
static bool SameColorAndAlpha(Color a, Color b) {
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

// the color a button makes annotations in is always one of the presets, so
// its drop-down can show it; one set some other way joins the list
static void EnsureAnnotPresetColor(int cmdId, Color col) {
    Str* list = AnnotPresetColorList(cmdId);
    if (!list || col == kColorUnset) {
        return;
    }
    Vec<Color> colors;
    AnnotPresetColors(cmdId, colors);
    for (Color c : colors) {
        if (SameColorAndAlpha(c, col)) {
            return;
        }
    }
    VecAppend(colors, col);
    str::ReplaceWithCopy(list, SerializeColorList(colors));
    ScheduleSaveSettings();
}

// whether the button's annotation can be made out of the text selected right now
static bool CanCreateAnnotFromSelection(MainWindow* win, int cmdId) {
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

void SetAnnotPresetColor(int cmdId, Color col) {
    ParsedColor* setting = AnnotPresetColorSetting(cmdId);
    if (!setting) {
        return;
    }
    SetColorText(*setting, SerializeColorTemp(col));
    ScheduleSaveSettings();
}

// --- the gpui view ----------------------------------------------------------

struct ToolbarView;

// one row or cell of the drop-down that is up, for the -dbg-control dump.
// `text` is owned. The vec does not run destructors, so free it by hand.
struct HoverDumpItem {
    gp::Bounds bounds{};
    int cmdId = 0;
    bool isCurrent = false;
    bool isRight = false;
    Str text;
};

struct ToolbarUI {
    gp::Entity<ToolbarView> view;
    // laid-out rect of each built button, parallel to btnCmds
    Vec<gp::Bounds> btnBounds;
    // the command each slot carries, so a rect can be found by command id
    Vec<int> btnCmds;
    // the zoom levels the drop-down that is up lists
    Vec<int> stripCmds;
    // the rows/cells of the drop-down that is up, and the box around them.
    // gpui writes the rects back through BoundsOut on the next layout.
    Vec<HoverDumpItem> hoverItems;
    gp::Bounds hoverBox{};
    // the first frame centres on a guess; the next one uses the laid-out width
    bool hoverRecenter = false;
    gp::SliderState inkThickness;
    bool inkThicknessInit = false;
    // the Read Aloud button's drop-down, rebuilt every frame
    MenuModel* ttsMenu = nullptr;
    gp::Entity<gp::PopupMenuState> ttsPopup;
    // orig's VirtIconButton::hoverOnDropdown
    bool ttsHovered = false;
    bool ttsHoverOnDropdown = false;
    // orig's ToolbarDropdownJustClosed(): the press that closes the menu
    // must not also run the button
    bool ttsMenuWasOpen = false;

    ~ToolbarUI();
};

struct ToolbarView {
    MainWindow* win = nullptr;

    static void OnButton(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnButtonHover(ToolbarView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx);
    static void OnStripCell(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnAnnotSwatch(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnAnnotColorsEdit(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnInkThickness(ToolbarView* self, gp::Ctx* cx, const gp::SliderEvent* ev);
    static void OnPageInput(ToolbarView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnChapterInput(ToolbarView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnButtonUp(ToolbarView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t idx);
    static void OnBarWheel(ToolbarView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
    static void OnStripHover(ToolbarView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
    static void OnBarHover(ToolbarView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
    static void OnTtsItem(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnTtsButtonMove(ToolbarView* self, gp::Ctx* cx, const gp::MouseMoveEvent* ev);
    static void OnTtsButtonDown(ToolbarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
};

static void FreeHoverItems(ToolbarUI* ui) {
    if (!ui) {
        return;
    }
    for (HoverDumpItem& it : ui->hoverItems) {
        str::Free(it.text);
        it.text = {};
    }
    VecClear(ui->hoverItems);
    ui->hoverBox = {};
}

ToolbarUI::~ToolbarUI() {
    FreeHoverItems(this);
    DeleteMenuModel(ttsMenu);
}

Toolbar::~Toolbar() {
    str::Free(pageTotal);
    str::Free(chapterTotal);
    delete pageEdit;
    delete chapterEdit;
    delete ui;
}

static Toolbar* Tb(MainWindow* win) {
    if (win && !win->toolbar) {
        win->toolbar = new Toolbar();
    }
    return win ? win->toolbar : nullptr;
}

static ToolbarUI* Ui(MainWindow* win) {
    Toolbar* tb = Tb(win);
    if (!tb->ui) {
        tb->ui = new ToolbarUI();
    }
    return tb->ui;
}

// --- the API the rest of the app calls --------------------------------------

void CreateToolbar(MainWindow* win) {
    Toolbar* tb = Tb(win);
    tb->iconSize = ToolbarIconSize();
    tb->rowDy = ToolbarRowDy(tb->iconSize);
    win->isToolbarVisible = ShouldShowToolbar(win);
    win->isToolbarOverlay = ShouldOverlayToolbar(win);
    PopulateToolbarLayout();
    PopulateCustomToolbarButtons();
}

// ng: the row is rebuilt from the tables every frame, so "re-create" only has
// to drop the parsed layout and let the next frame read the settings again
void ReCreateToolbar(MainWindow* win) {
    gLayoutParsed = false;
    CreateToolbar(win);
    AppShellInvalidate(win);
}

void DestroyToolbar(MainWindow* win) {
    delete win->toolbar;
    win->toolbar = nullptr;
}

// is the annotation ("Edit PDF") row shown for this window?
static bool AnnotRowVisible(MainWindow* win) {
    if (!win || !win->pdfAnnotationsToolbarEnabled) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    return EngineMupdfIsPdf(engine) && EngineSupportsAnnotations(engine);
}

static void SetPdfAnnotationsToolbarEnabled(MainWindow* win, bool enabled) {
    if (!win || win->pdfAnnotationsToolbarEnabled == enabled) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    EngineBase* engine = tab ? tab->GetEngine() : nullptr;
    if (!EngineMupdfIsPdf(engine) || !EngineSupportsAnnotations(engine)) {
        return;
    }
    if (win->pdfAnnotationsToolbarEnabled) {
        FinishInkAnnotationPlacement(win);
        // a half-placed line / shape / stamp is editing UI too: its notification
        // and cross cursor would outlive the mode it belongs to
        CancelAnnotationPlacement(win);
    }
    win->pdfAnnotationsToolbarEnabled = enabled;
    if (!enabled) {
        // leaving the mode leaves no editing UI behind
        if (tab && tab->selectedAnnotation) {
            SetSelectedAnnotation(tab, nullptr);
        }
        HideAnnotEditToolbar(win);
        UpdateAnnotFilterToolbar(win);
    }
    logf("EditPDF: %s\n", enabled ? StrL("on") : StrL("off"));
    AppShellInvalidate(win);
}

void TogglePdfAnnotationsToolbar(MainWindow* win) {
    if (!win) {
        return;
    }
    SetPdfAnnotationsToolbarEnabled(win, !win->pdfAnnotationsToolbarEnabled);
}

void EnablePdfAnnotationsToolbar(MainWindow* win) {
    SetPdfAnnotationsToolbarEnabled(win, true);
}

// ng: the shell asks for this before it builds the frame, so this is also
// where the mode is re-read (orig reacts to the commands that change it)
int ToolbarDy(MainWindow* win) {
    Toolbar* tb = Tb(win);
    if (tb->rowDy == 0) {
        CreateToolbar(win);
    }
    ShowOrHideToolbar(win);
    if (!win->isToolbarVisible || win->isToolbarOverlay) {
        return 0;
    }
    return AnnotRowVisible(win) ? (2 * tb->rowDy) : tb->rowDy;
}

// ng: every button's state is recomputed while the row is built, so these are
// only a repaint request. They keep orig's names so the callers stay unchanged.
void ToolbarUpdateStateForWindow(MainWindow* win, bool) {
    AppShellInvalidate(win);
}

void UpdateToolbarButtonsToolTipsForWindow(MainWindow* win) {
    AppShellInvalidate(win);
}

void SetToolbarButtonEnableState(MainWindow* win, int, bool) {
    AppShellInvalidate(win);
}

void SetToolbarButtonCheckedState(MainWindow* win, int, bool) {
    AppShellInvalidate(win);
}

void UpdateToolbarState(MainWindow* win) {
    AppShellInvalidate(win);
}

void UpdateToolbarAfterThemeChange(MainWindow* win) {
    AppShellInvalidate(win);
}

void UpdateFindbox(MainWindow* win) {
    AppShellInvalidate(win);
}

// the find UI is a floating Chrome-style bar (see FindBar.cpp). When the
// toolbar moves/resizes we keep the bar centered over the search icon.
void UpdateToolbarFindText(MainWindow* win) {
    FindBarReposition(win);
}

// One line per toolbar button and one per Edit PDF button, same shape as
// orig's dump. The harness matches `annotation-idx` for Undo, Redo and Save.
static TempStr HoverDropdownStateTemp(MainWindow* win);

TempStr ToolbarButtonsResultTemp(int* exitCodeOut) {
    str::Builder out;
    MainWindow* win = len(gWindows) == 0 ? nullptr : gWindows[0];
    if (!win || !win->toolbar) {
        *exitCodeOut = 1;
        out.Append(StrL("ERROR no-toolbar\n"));
        return ToStrTemp(out);
    }
    PopulateToolbarLayout();
    PopulateCustomToolbarButtons();
    auto* ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
    AutoCall delCtx(DeleteBuildMenuCtx, ctx);

    int n = TotalButtonsCount();
    Vec<bool> hidden;
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        bool hide = false;
        if (bi.cmdId != WarningMsgId && bi.cmdId != 0) {
            hide = !IsCmdAvailable(win, bi.cmdId, ctx);
        }
        VecAppend(hidden, hide);
    }
    bool prevVisibleNonSep = false;
    int lastSep = -1;
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (bi.cmdId == 0) {
            hidden[i] = !prevVisibleNonSep;
            prevVisibleNonSep = false;
            if (!hidden[i]) {
                lastSep = i;
            }
            continue;
        }
        if (!hidden[i]) {
            prevVisibleNonSep = true;
            lastSep = -1;
        }
    }
    if (lastSep >= 0) {
        hidden[lastSep] = true;
    }

    int nTools = 0;
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (!hidden[i] && bi.cmdId != 0 && bi.cmdId != PageInfoId && len(bi.toolTip) > 0) {
            nTools++;
        }
    }
    out.Append(fmt("buttons=%d tooltipTools=%d\n", n, nTools));

    int toolIdx = 0;
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        // the page box has no icon but still owns a bounds slot
        Rect r{};
        if (!hidden[i] && bi.cmdId != 0) {
            r = GetToolbarButtonRect(win, bi.cmdId);
        }
        // the drop-down takes its button's tooltip, so the bubble does not sit on it
        Str text = bi.toolTip;
        if (win->toolbar->hoverCmdId == bi.cmdId) {
            text = StrL("");
        }
        out.Append(fmt("idx=%d cmd=%d hidden=%d rect=%d,%d,%d,%d text=%s\n", i, bi.cmdId, hidden[i] ? 1 : 0, r.x, r.y,
                       r.x + r.dx, r.y + r.dy, text));
        if (!hidden[i] && bi.cmdId != 0 && bi.cmdId != PageInfoId && len(bi.toolTip) > 0) {
            out.Append(fmt("tool=%d uid=%d rect=%d,%d,%d,%d\n", toolIdx, bi.cmdId, r.x, r.y, r.x + r.dx, r.y + r.dy));
            toolIdx++;
        }
    }

    bool annotationsVisible = AnnotRowVisible(win);
    bool buttonsEnabled = !IsPlacingAnnotation(win);
    out.Append(fmt("annotationButtons=%d visible=%d\n", kPdfAnnotationButtonsCount, annotationsVisible ? 1 : 0));
    WindowTab* tab = win->CurrentTab();
    TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
    for (int i = 0; i < kPdfAnnotationButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gPdfAnnotationButtons[i];
        bool isSep = bi.cmdId == 0 || !HasToolbarButtonContent(bi);
        bool available = !isSep && IsCmdAvailable(win, bi.cmdId, ctx);
        bool shown = annotationsVisible && available;
        bool enabled = shown && buttonsEnabled && IsCmdEnabled(win, bi.cmdId, ctx);
        Rect r{};
        if (shown) {
            r = GetToolbarButtonRect(win, bi.cmdId);
        }
        Str tip{};
        bool tipTaken = win->toolbar->hoverCmdId == bi.cmdId;
        if (!tipTaken && len(bi.toolTip) > 0) {
            if (bi.cmdId == CmdSaveAnnotations && len(base) > 0) {
                tip = ToolbarTipTemp(bi.cmdId, fmt(Tr("Save changes to %s").s, base), false);
            } else {
                tip = ToolbarTipTemp(bi.cmdId, bi.toolTip, true);
            }
        }
        out.Append(fmt("annotation-idx=%d cmd=%d hidden=%d enabled=%d rect=%d,%d,%d,%d text=%s tip=%s\n", i, bi.cmdId,
                       shown ? 0 : 1, enabled ? 1 : 0, r.x, r.y, r.x + r.dx, r.y + r.dy, bi.toolTip, tip));
    }
    out.Append(AnnotFilterToolbarStateTemp(win));
    out.Append(HoverDropdownStateTemp(win));
    *exitCodeOut = 0;
    return ToStrTemp(out);
}

Rect GetToolbarButtonRect(MainWindow* win, int cmdId) {
    if (!win || !win->toolbar || !win->toolbar->ui) {
        return {};
    }
    if (!win->isToolbarVisible && !win->toolbar->overlayShown) {
        return {};
    }
    ToolbarUI* ui = win->toolbar->ui;
    for (int i = 0; i < len(ui->btnCmds); i++) {
        if (ui->btnCmds[i] == cmdId) {
            return FromGpui(ui->btnBounds[i]);
        }
    }
    return {};
}

// ng: orig pushes the label into a VirtText; here it lands in the Toolbar and
// the next frame draws it. Returns true when it changed.
static bool ComputePageText(MainWindow* win, int pageCount) {
    Toolbar* tb = Tb(win);
    tb->hasChapters = ShowChapterUi(win->ctrl);

    TempStr txt;
    if (-1 == pageCount || !pageCount) {
        txt = StrL(" ");
    } else if (tb->hasChapters) {
        int chapter = win->ctrl->CurrentLocation().chapter;
        txt = fmt(" / %d", win->ctrl->ChapterPageCount(chapter));
        str::ReplaceWithCopy(&tb->chapterTotal, fmt(" / %d", win->ctrl->ChapterCount()));
    } else if (!win->ctrl || !win->ctrl->HasPageLabels()) {
        txt = fmt(" / %d", pageCount);
    } else {
        int logical = pageCount;
        DisplayModel* dm = win->ctrl->AsFixed();
        if (dm) {
            logical = dm->LogicalPageCount();
        }
        if (logical > 0 && logical != pageCount) {
            txt = fmt(" / %d (%d / %d)", logical, win->ctrl->CurrentPageNo(), pageCount);
        } else {
            txt = fmt("%d / %d", win->ctrl->CurrentPageNo(), pageCount);
        }
    }
    if (str::Eq(tb->pageTotal, txt)) {
        return false;
    }
    str::ReplaceWithCopy(&tb->pageTotal, txt);
    return true;
}

void UpdateToolbarPageText(MainWindow* win, int pageCount, bool) {
    if (ComputePageText(win, pageCount)) {
        AppShellInvalidate(win);
    }
}

// --- the overlay toolbar ----------------------------------------------------

static void SetOverlayShown(MainWindow* win, bool shown) {
    Toolbar* tb = Tb(win);
    if (shown == tb->overlayShown) {
        return;
    }
    tb->overlayShown = shown;
    tb->overlayHideLeftMs = 0;
    logf("ToolbarOverlay: %s\n", shown ? StrL("shown") : StrL("hidden"));
    AppShellInvalidate(win);
}

void RevealOverlayToolbar(MainWindow* win) {
    if (!win->isToolbarOverlay) {
        return;
    }
    Tb(win)->overlayHideLeftMs = 0;
    SetOverlayShown(win, true);
}

// when the overlay toolbar sits at the bottom, lift it above the horizontal
// scrollbar so it doesn't cover it. The height is reserved even when the
// scrollbar isn't currently visible, so the toolbar's position is stable.
static int OverlayToolbarBottomScrollbarOffset() {
    if (ScrollbarsAreHidden()) {
        return 0;
    }
    if (ScrollbarsUseOverlay()) {
        // smart/overlay: the thick overlay scrollbar height
        return DpiScale(16);
    }
    return DpiGetSystemMetrics(SM_CYHSCROLL);
}

// top of the overlay toolbar in frame coords (orig's OverlayToolbarRect)
static int OverlayToolbarY(MainWindow* win) {
    Rect canvas = win->canvasRc;
    if (!ToolbarAtBottom()) {
        return canvas.y;
    }
    return canvas.y + canvas.dy - Tb(win)->rowDy - OverlayToolbarBottomScrollbarOffset();
}

// orig's reveal band: it spans the full canvas width so the toolbar also
// appears when the mouse is to the left or right of it, and extends a bit past
// the toolbar (toward the page) so it shows before the cursor reaches it
static Rect OverlayRevealBand(MainWindow* win) {
    Rect canvas = win->canvasRc;
    int rowDy = Tb(win)->rowDy;
    int my = DpiScale(16);
    int tbY = OverlayToolbarY(win);
    int bandY = ToolbarAtBottom() ? (tbY - my) : tbY;
    return Rect{canvas.x, bandY, canvas.dx, rowDy + my};
}

// re-evaluate overlay toolbar visibility based on the cursor's position; the
// canvas calls this on every mouse move, as orig's does
void UpdateOverlayToolbarForMouse(MainWindow* win, Point ptInFrame) {
    if (!win->isToolbarOverlay) {
        return;
    }
    Toolbar* tb = Tb(win);
    bool show = OverlayRevealBand(win).Contains(ptInFrame) || IsToolbarPageBoxFocused(win);
    if (show) {
        RevealOverlayToolbar(win);
        return;
    }
    // don't hide immediately; give the user kDelayToolbarHide to come back
    if (tb->overlayShown && tb->overlayHideLeftMs == 0) {
        tb->overlayHideLeftMs = kDelayToolbarHide;
    }
}

void ShowOrHideToolbar(MainWindow* win) {
    bool show = ShouldShowToolbar(win);
    bool overlay = ShouldOverlayToolbar(win);
    if (show == win->isToolbarVisible && overlay == win->isToolbarOverlay) {
        return;
    }
    bool enteredOverlay = overlay && !win->isToolbarOverlay;
    win->isToolbarVisible = show;
    win->isToolbarOverlay = overlay;
    Toolbar* tb = Tb(win);
    if (!overlay) {
        tb->overlayShown = false;
        tb->overlayHideLeftMs = 0;
    }
    if (enteredOverlay) {
        // reveal immediately on entering overlay mode (e.g. via F8) so the
        // change is visible; it auto-hides after kDelayToolbarHide
        tb->overlayShown = true;
        tb->overlayHideLeftMs = kDelayToolbarHide;
    }
    // move the focus out of the toolbar when it goes away
    if (!show && !overlay && IsToolbarPageBoxFocused(win) && win->gpuiWin) {
        gp::InputBlur(tb->pageEdit, win->gpuiWin->app, win->gpuiWin);
        gp::InputBlur(tb->chapterEdit, win->gpuiWin->app, win->gpuiWin);
    }
    AppShellInvalidate(win);
}

// --- the zoom drop-down -----------------------------------------------------

struct ZoomHoverLevel {
    float zoom;
    int cmdId;
};

// What the zoom strip lists: the levels the zoom buttons step through, plus the
// two fit modes where 100% is.
static void ZoomHoverLevels(Vec<ZoomHoverLevel>& out) {
    int n = 0;
    float* levels = GetDefaultZoomLevels(&n);
    Vec<int>* cmdIds = GetZoomStepCmdIds();
    if (!levels || !cmdIds || len(*cmdIds) != n) {
        return;
    }
    bool addedFits = false;
    for (int i = 0; i < n; i++) {
        if (!addedFits && levels[i] > 100) {
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

// which of the levels the document is at, exact match only
static int ZoomHoverCurrentIdx(MainWindow* win, const Vec<ZoomHoverLevel>& levels) {
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

static bool CmdIsAnnotColorDropdown(int cmdId) {
    return AnnotPresetColorSetting(cmdId) != nullptr;
}

static bool CmdHasHoverDropdown(int cmdId) {
    return cmdId == CmdZoomIn || cmdId == CmdZoomOut || cmdId == CmdSaveAnnotations || CmdIsAnnotColorDropdown(cmdId);
}

void HideToolbarHoverDropdown(MainWindow* win) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    if (!tb) {
        return;
    }
    tb->hoverCmdId = 0;
    tb->hoverAnchorCmdId = 0;
    tb->hoverPendingCmdId = 0;
    tb->hoverOpenLeftMs = 0;
    tb->hoverCloseLeftMs = 0;
    if (tb->ui) {
        FreeHoverItems(tb->ui);
    }
}

static bool ZoomHoverGroup(int cmdId) {
    return cmdId == CmdZoomIn || cmdId == CmdZoomOut;
}

// dips of the frame to screen pixels. The dump's rects are screen pixels, the
// same as orig's VirtHost::ToScreen.
static Rect HoverBoundsScreen(MainWindow* win, gp::Bounds b) {
    float k = CanvasScale(win);
    if (!(k > 0)) {
        k = 1;
    }
    POINT o{0, 0};
    HWND hwnd = AppShellNativeHwnd(win);
    if (hwnd) {
        ClientToScreen(hwnd, &o);
    }
    int x = o.x + (int)(b.x / k + 0.5f);
    int y = o.y + (int)(b.y / k + 0.5f);
    int x2 = o.x + (int)((b.x + b.w) / k + 0.5f);
    int y2 = o.y + (int)((b.y + b.h) / k + 0.5f);
    return Rect{x, y, x2 - x, y2 - y};
}

// The rows/cells of the drop-down that is up, in screen coordinates.
static TempStr HoverDropdownStateTemp(MainWindow* win) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    ToolbarUI* ui = tb ? tb->ui : nullptr;
    int cmd = tb ? tb->hoverCmdId : 0;
    if (!ui || cmd == 0 || ui->hoverBox.w < 1 || ui->hoverBox.h < 1 || len(ui->hoverItems) == 0) {
        return fmt("dropdown cmd=%d items=0\n", cmd);
    }
    str::Builder out;
    Rect box = HoverBoundsScreen(win, ui->hoverBox);
    Color bg = TbBgColor();
    Color shade = TbSubtleBgColor();
    out.Append(fmt("dropdown cmd=%d items=%d box=%d,%d,%d,%d bg=%u shade=%u gen=%d\n", cmd, len(ui->hoverItems), box.x,
                   box.y, box.Right(), box.Bottom(), (unsigned)bg, (unsigned)shade, tb->hoverGen));
    for (int i = 0; i < len(ui->hoverItems); i++) {
        HoverDumpItem& it = ui->hoverItems[i];
        Rect r = HoverBoundsScreen(win, it.bounds);
        out.Append(fmt("dropdown-item idx=%d cmd=%d current=%d rect=%d,%d,%d,%d text=%s\n", i, it.cmdId,
                       it.isCurrent ? 1 : 0, r.x, r.y, r.Right(), r.Bottom(), it.text));
        out.Append(fmt("dropdown-dip idx=%d rect=%d,%d,%d,%d\n", i, (int)it.bounds.x, (int)it.bounds.y,
                       (int)(it.bounds.x + it.bounds.w), (int)(it.bounds.y + it.bounds.h)));
        if (it.isRight) {
            out.Append(fmt("dropdown-right idx=%d\n", i));
        }
    }
    return ToStrTemp(out);
}

// Slots for the items about to be built. Reserve first so BoundsOut pointers
// stay put while the elements are created.
static void BeginHoverDump(ToolbarUI* ui, int n) {
    FreeHoverItems(ui);
    VecReserve(ui->hoverItems, n);
}

static HoverDumpItem* AddHoverDumpItem(ToolbarUI* ui, int cmdId, Str text, bool isCurrent, bool isRight) {
    HoverDumpItem it;
    it.cmdId = cmdId;
    it.isCurrent = isCurrent;
    it.isRight = isRight;
    it.text = str::Dup(text);
    VecAppend(ui->hoverItems, it);
    return &ui->hoverItems[len(ui->hoverItems) - 1];
}

// orig's kCloseHoverDropdownTimerId: the drop-down stays while the cursor is on
// its button or on it, and for this long after it left both
constexpr int kCloseHoverDropdownDelayMs = 150;

// The widest row of the pyramid: the smallest w with 1+2+...+w >= n, so the
// rows w, w-1, ... w-k hold every item with only the last one part-full.
static int HoverPyramidTopRow(int n) {
    int w = 1;
    while (((w * (w + 1)) / 2) < n) {
        w++;
    }
    return w;
}

// A row of NewToolbarHoverMenu(): an icon on the left, text on the right, and a
// background that lights up under the mouse, like a menu item.
constexpr int kHoverRowPadY = 6;
constexpr int kHoverRowPadX = 10;
constexpr int kHoverRowIconGapX = 8;
// between the label and the shortcut that sits at the right edge, as in a menu
constexpr int kHoverRowShortcutGapX = 24;
constexpr int kHoverCellPadX = 8;

struct ToolbarHoverMenuItem {
    Str svgIcon;
    Str text;
    int cmdId = 0;
    bool enabled = true;
};

static gp::El* ToolbarIcon(gp::Ctx* cx, Str svg, int iconSize, Color col);

// orig's NewToolbarHoverMenu / ToolbarHoverRow
static gp::El* NewToolbarHoverMenu(MainWindow* win, gp::Ctx* cx, const Vec<ToolbarHoverMenuItem>& items) {
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    const gp::Theme& th = gp::ThemeNow(cx->app);
    BeginHoverDump(ui, len(items));
    gp::El* box = gp::Div(cx->a)
                      ->FlexCol()
                      ->ItemsStretch()
                      ->Absolute()
                      ->Bg(ToGpui(TbBgColor()))
                      ->Border(1, th.border)
                      ->BoundsOut(&ui->hoverBox)
                      ->OnHover(gp::ListenTo(ui->view, &ToolbarView::OnStripHover));
    for (const ToolbarHoverMenuItem& it : items) {
        Color col = it.enabled ? TbTextColor() : TbDisabledColor();
        HoverDumpItem* slot = AddHoverDumpItem(ui, it.cmdId, it.text, false, false);
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->ItemsCenter()
                          ->PadX((float)DpiScale(kHoverRowPadX))
                          ->PadY((float)DpiScale(kHoverRowPadY))
                          ->Gap((float)DpiScale(kHoverRowIconGapX))
                          ->BoundsOut(&slot->bounds);
        if (it.enabled) {
            row->HoverBg(ToGpui(TbHoverColor()))
                ->PathClick(GpuiDup(cx->a, fmt("tb-hover-row-%d", it.cmdId)))
                ->OnClick(gp::ListenTo(ui->view, &ToolbarView::OnStripCell, (intptr_t)it.cmdId));
        }
        row->Child(ToolbarIcon(cx, it.svgIcon, tb->iconSize, col));
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, it.text))->Font(12)->Fg(ToGpui(col)));
        TempStr shortcut = ShortcutsForCmdTemp(it.cmdId, 1);
        if (len(shortcut) > 0) {
            // right-aligned and dimmer, the way a menu shows its accelerator
            row->Child(gp::Div(cx->a)->Flex1()->MinW((float)DpiScale(kHoverRowShortcutGapX - kHoverRowIconGapX)));
            row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, shortcut))->Font(12)->Fg(ToGpui(TbDisabledColor())));
        }
        box->Child(row);
    }
    return box;
}

// The Save button's drop-down: the three ways to end an editing session.
static gp::El* BuildSaveHoverMenu(MainWindow* win, gp::Ctx* cx) {
    Rect anchor = GetToolbarButtonRect(win, CmdSaveAnnotations);
    if (anchor.IsEmpty()) {
        return nullptr;
    }
    WindowTab* tab = win->CurrentTab();
    auto* ctx = NewBuildMenuCtx(tab, Point{0, 0});
    AutoCall delCtx(DeleteBuildMenuCtx, ctx);
    bool dirty = ctx->hasUnsavedAnnotations;

    TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
    Str saveText = Tr("Save changes to existing PDF");
    if (len(base) > 0) {
        saveText = fmt(Tr("Save changes to %s").s, base);
    }

    Vec<ToolbarHoverMenuItem> items;
    VecAppend(items, {Str(gIconSave), saveText, CmdSaveAnnotations, dirty});
    VecAppend(items, {Str(gIconSaveToNewFile), Tr("Save changes to a new PDF"), CmdSaveAnnotationsNewFile, dirty});
    VecAppend(items, {Str(gIconTrash), Tr("Discard changes"), CmdDiscardChanges, dirty});
    gp::El* box = NewToolbarHoverMenu(win, cx, items);
    // under the button, left edges aligned
    box->Left((float)anchor.x)->Top((float)anchor.Bottom());
    return box;
}

static gp::El* StripCell(MainWindow* win, gp::Ctx* cx, Str text, int cmdId, bool isCurrent, bool isRightHalf) {
    gp::El* cell = gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->MinW(44)
                       ->PadX((float)DpiScale(kHoverCellPadX))
                       ->PadY((float)DpiScale(kHoverRowPadY))
                       ->HoverBg(ToGpui(TbHoverColor()))
                       ->PathClick(GpuiDup(cx->a, fmt("tb-zoom-%d", cmdId)))
                       ->OnClick(gp::ListenTo(Ui(win)->view, &ToolbarView::OnStripCell, (intptr_t)cmdId));
    if (isRightHalf) {
        cell->Bg(ToGpui(TbSubtleBgColor()));
    }
    // every cell keeps the border, so boxing the current level does not change
    // the strip's width and slide it out from under the mouse
    Color edge = isCurrent ? TbTextColor() : (isRightHalf ? TbSubtleBgColor() : TbBgColor());
    cell->Border(1, ToGpui(edge));
    cell->Child(gp::TextEl(cx->a, GpuiDup(cx->a, text))->Font(12)->Fg(ToGpui(TbTextColor())));
    return cell;
}

// The zoom buttons' drop-down: the levels as a compact pyramid centred on the
// button, the one in use boxed. A pyramid rather than one long row: the middle
// of the list goes in the top row, next to the button, and each row below it
// holds what surrounds the middle, down to the two extremes.
static gp::El* BuildZoomStrip(MainWindow* win, gp::Ctx* cx) {
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    int anchorCmd = tb->hoverAnchorCmdId != 0 ? tb->hoverAnchorCmdId : tb->hoverCmdId;
    Rect anchor = GetToolbarButtonRect(win, anchorCmd);
    if (anchor.IsEmpty() || !win->ctrl) {
        return nullptr;
    }
    Vec<ZoomHoverLevel> levels;
    ZoomHoverLevels(levels);
    int n = len(levels);
    if (n == 0) {
        return nullptr;
    }
    int currentIdx = ZoomHoverCurrentIdx(win, levels);
    VecReset(ui->stripCmds);
    // last frame's width. The guess below is only for the frame it first opens,
    // before gpui has laid the pyramid out.
    float laidW = ui->hoverBox.w;
    BeginHoverDump(ui, n);
    for (int i = 0; i < n; i++) {
        AddHoverDumpItem(ui, levels[i].cmdId, ZoomLevelStrExact(levels[i].zoom), i == currentIdx, false);
    }

    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* box = gp::Div(cx->a)
                      ->FlexCol()
                      ->ItemsStretch()
                      ->Absolute()
                      ->Bg(ToGpui(TbBgColor()))
                      ->Border(1, th.border)
                      ->BoundsOut(&ui->hoverBox)
                      ->OnHover(gp::ListenTo(ui->view, &ToolbarView::OnStripHover));

    int top = HoverPyramidTopRow(n);
    int lo = (n - top) / 2;
    int hi = lo + top;
    // orig's isRightHalf: the top row splits at its own middle, and everything
    // past that point is on the larger side in every row
    int firstRight = lo + (top / 2);

    // orig's PaintHoverDropdownRightHalf: the values above the middle get their
    // own ground, and each band runs to the right edge, covering the empty
    // space beside a short row as well. The two equal fillers center the row
    auto addRow = [&](int a0, int a1, int b0, int b1) {
        gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsStretch()->W(gp::kFill);
        row->Child(gp::Div(cx->a)->Flex1());
        auto addCell = [&](int i) {
            bool isRight = i >= firstRight;
            ui->hoverItems[i].isRight = isRight;
            Str text = ui->hoverItems[i].text;
            gp::El* cell = StripCell(win, cx, text, levels[i].cmdId, i == currentIdx, isRight);
            cell->BoundsOut(&ui->hoverItems[i].bounds);
            row->Child(cell);
            VecAppend(ui->stripCmds, levels[i].cmdId);
        };
        for (int i = a0; i < a1; i++) {
            addCell(i);
        }
        for (int i = b0; i < b1; i++) {
            addCell(i);
        }
        row->Child(gp::Div(cx->a)->Flex1()->Bg(ToGpui(TbSubtleBgColor())));
        box->Child(row);
    };

    addRow(lo, hi, 0, 0);
    int rowLen = top - 1;
    while (lo > 0 || hi < n) {
        int nLeft = std::min(lo, (rowLen + 1) / 2);
        int nRight = std::min(n - hi, rowLen - nLeft);
        nLeft = std::min(lo, rowLen - nRight);
        addRow(lo - nLeft, lo, hi, hi + nRight);
        lo -= nLeft;
        hi += nRight;
        rowLen = std::max(rowLen - 1, 1);
    }

    // hangs off the middle of the button, so it opens around where the mouse
    // already is (orig's centerOnButton)
    float stripDx = laidW > 1 ? laidW : (float)(top * (DpiScale(kHoverCellPadX) * 2 + 44));
    float x = (float)anchor.x + ((float)anchor.dx - stripDx) / 2;
    x = limitValue(x, 0.f, std::max(0.f, (float)win->frameRc.dx - stripDx));
    box->Left(x)->Top((float)anchor.Bottom());
    if (!(laidW > 1)) {
        ui->hoverRecenter = true;
    }
    return box;
}

constexpr int kInkThicknessMin = 1;
constexpr int kInkThicknessMax = 16;

// ring space around a swatch, which the pencil button gets too
constexpr int kAnnotSwatchPad = 5;

// what the button is, as its tooltip says, since the drop-down takes the
// tooltip's place
static Str AnnotButtonTitle(int cmdId) {
    for (const ToolbarButtonInfo& bi : gPdfAnnotationButtons) {
        if (bi.cmdId == cmdId && len(bi.toolTip) > 0) {
            return trans::GetTranslation(bi.toolTip);
        }
    }
    return {};
}

// An annotation button's drop-down (orig's BuildAnnotColorsHoverMenu /
// MakeAnnotColorsPanel): a label, the preset colors as discs with the one the
// button makes its annotations in ringed, a pencil that opens the color dialog
// on the whole set, plus the ink Thickness slider.
static gp::El* BuildAnnotColorStrip(MainWindow* win, gp::Ctx* cx) {
    Toolbar* tb = Tb(win);
    int cmdId = tb->hoverCmdId;
    Rect anchor = GetToolbarButtonRect(win, cmdId);
    if (anchor.IsEmpty()) {
        return nullptr;
    }
    Color current = AnnotColorForCmd(cmdId);
    EnsureAnnotPresetColor(cmdId, current);
    Vec<Color> colors;
    AnnotPresetColors(cmdId, colors);
    if (len(colors) == 0) {
        return nullptr;
    }
    ToolbarUI* ui = Ui(win);
    bool hasThickness = cmdId == CmdCreateAnnotInk;
    int width = 0;
    if (hasThickness) {
        width = limitValue(gSettings->annotations.inkBorderWidth, kInkThicknessMin, kInkThicknessMax);
    }
    // the slider is one more slot; reserving after the swatches would move the
    // bounds the swatches already point at
    BeginHoverDump(ui, len(colors) + (hasThickness ? 1 : 0));
    for (int i = 0; i < len(colors); i++) {
        bool on = SameColorAndAlpha(colors[i], current);
        AddHoverDumpItem(ui, cmdId, SerializeColorTemp(colors[i]), on, false);
    }
    int sliderIdx = -1;
    if (hasThickness) {
        sliderIdx = len(ui->hoverItems);
        AddHoverDumpItem(ui, cmdId, fmt("thickness=%d", width), false, false);
    }
    VecReset(ui->stripCmds);

    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* box = gp::Div(cx->a)
                      ->FlexCol()
                      ->Gap(6)
                      ->Pad((float)DpiScale(6))
                      ->Absolute()
                      ->Bg(ToGpui(TbBgColor()))
                      ->Border(1, th.border)
                      ->BoundsOut(&ui->hoverBox)
                      ->OnHover(gp::ListenTo(ui->view, &ToolbarView::OnStripHover));
    // a note's color fills its icon, behind the note
    Str label = (cmdId == CmdCreateAnnotText) ? Tr("Background Color") : Tr("Color");
    gp::El* labelRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->JustifyBetween()->Gap((float)DpiScale(16));
    labelRow->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(12)->Fg(ToGpui(TbTextColor())));
    Str title = AnnotButtonTitle(cmdId);
    if (len(title) > 0) {
        labelRow->Child(gp::TextEl(cx->a, GpuiDup(cx->a, title))->Font(12)->Fg(ToGpui(TbTextColor())));
    }
    box->Child(labelRow);

    gp::El* colorRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(4);
    float d = (float)DpiScale(20);
    for (int i = 0; i < len(colors); i++) {
        Color c = colors[i];
        // the swatch's command id is the color, tagged so OnAnnotSwatch can
        // tell it from a zoom cell
        gp::El* sw = gp::Div(cx->a)
                         ->W(d)
                         ->H(d)
                         ->Radius(d / 2)
                         ->Bg(ToGpui(c))
                         ->Cursor(gp::CursorKind::Pointer)
                         ->BoundsOut(&ui->hoverItems[i].bounds)
                         ->PathClick(GpuiDup(cx->a, fmt("tb-annot-col-%d", i)))
                         ->OnClick(gp::ListenTo(ui->view, &ToolbarView::OnAnnotSwatch, (intptr_t)i));
        if (SameColorAndAlpha(c, current)) {
            sw->Border(2, ToGpui(TbTextColor()));
        } else {
            sw->Border(1, ToGpui(TbEdgeColor()));
        }
        colorRow->Child(sw);
    }
    // the pencil opens the color dialog on the whole set
    float pad = (float)DpiScale(kAnnotSwatchPad);
    gp::El* editBtn = gp::Div(cx->a)
                          ->Pad(pad)
                          ->HoverBg(ToGpui(TbHoverColor()))
                          ->Cursor(gp::CursorKind::Pointer)
                          ->Tip(ToGpui(Tr("Edit colors")))
                          ->PathClick(GStrL("tb-annot-col-edit"))
                          ->OnClick(gp::ListenTo(ui->view, &ToolbarView::OnAnnotColorsEdit, (intptr_t)cmdId));
    editBtn->Child(ToolbarIcon(cx, Str(gIconEditAnnotations), tb->iconSize, TbTextColor()));
    colorRow->Child(editBtn);
    box->Child(colorRow);
    float stripDx = (float)(len(colors) * (DpiScale(20) + 4) + 2 * DpiScale(6)) + (float)tb->iconSize + 2 * pad;
    if (hasThickness) {
        if (!ui->inkThicknessInit) {
            ui->inkThickness = gp::SliderStateNew(kInkThicknessMin, kInkThicknessMax, gp::SliderSingle((float)width));
            ui->inkThicknessInit = true;
        } else if (!ui->inkThickness.dragging) {
            gp::SliderSetValue(&ui->inkThickness, gp::SliderSingle((float)width));
        }
        gp::El* sliderRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(6);
        sliderRow->Child(gp::TextEl(cx->a, ToGpui(Tr("Thickness")))->Font(12)->Fg(th.foreground));
        // ng: the listener goes to the component; Slider::IntoEl() replaces
        // the one set on the state
        sliderRow->Child(gpc::Slider::New(cx, GStrL("tb-ink-thickness"), &ui->inkThickness)
                             ->OnChange(gp::ListenTo(ui->view, &ToolbarView::OnInkThickness))
                             ->W(160)
                             ->IntoEl()
                             ->BoundsOut(&ui->hoverItems[sliderIdx].bounds));
        sliderRow->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", width)))->Font(12)->Fg(th.foreground)->MinW(16));
        box->Child(sliderRow);
        stripDx = std::max(stripDx, (float)DpiScale(240));
    }
    float x = (float)anchor.x + ((float)anchor.dx - stripDx) / 2;
    x = limitValue(x, 0.f, std::max(0.f, (float)win->frameRc.dx - stripDx));
    box->Left(x)->Top((float)anchor.Bottom());
    return box;
}

// --- the row ----------------------------------------------------------------

static gp::El* ToolbarSeparator(gp::Ctx* cx, int rowDy) {
    int inset = DpiScale(6);
    gp::El* box =
        gp::Div(cx->a)->FlexRow()->W((float)DpiScale(8))->H((float)rowDy)->Shrink0()->ItemsCenter()->JustifyCenter();
    box->Child(gp::Div(cx->a)->W(1)->H((float)(rowDy - 2 * inset))->Bg(ToGpui(TbEdgeColor())));
    return box;
}

static gp::El* ToolbarIcon(gp::Ctx* cx, Str svg, int iconSize, Color col) {
    return gpc::Icon::New(cx, gp::IconName::None)
        ->Data(ToGpui(svg))
        ->Size((float)iconSize)
        ->Color(ToGpui(col))
        ->IntoEl();
}

// --- the Read Aloud button's drop-down --------------------------------------

static gpc::PopupMenu* TtsPopupFromModel(MainWindow* win, gp::Ctx* cx, MenuModel* model, Str id) {
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GpuiDup(cx->a, id))->MinW(220);
    if (!model) {
        return menu;
    }
    int i = -1;
    for (const MenuItemModel& it : model->items) {
        i++;
        if (it.separator) {
            menu->Separator();
            continue;
        }
        gp::Str label = GpuiDup(cx->a, ParseMenuAccelTextTemp(it.title).display);
        if (it.submenu) {
            TempStr subId = fmt("%s-%d", id, i);
            menu->Submenu(label, TtsPopupFromModel(win, cx, it.submenu, subId));
            menu->Disabled(it.disabled);
            continue;
        }
        // ng: a menu item's action is dispatched from the focused element,
        // which the button is no ancestor of, so the items call back directly
        menu->Menu(label)->OnClick(gp::ListenTo(Ui(win)->view, &ToolbarView::OnTtsItem, (intptr_t)it.cmdId));
        menu->Disabled(it.disabled);
        menu->Checked(it.checked);
    }
    return menu;
}

// orig's VirtIconButton::DropdownDx(): the width of the arrow inside the button
constexpr int kIconBtnDropdownDx = 12;

// orig's ShowTtsVoiceMenu: the menu drops under the button
static void ShowTtsVoiceMenu(MainWindow* win, gp::Ctx* cx) {
    OpenPopupMenuAt(cx, Ui(win)->ttsPopup, 0, (float)Tb(win)->rowDy);
    AppShellInvalidate(win);
}

// orig's VirtIconButton with hasDropdown: the arrow is part of the button, the
// hover lights up the half the mouse is over and the click's x decides whether
// the command or the menu runs
static gp::El* ToolbarReadAloudGroup(MainWindow* win, gp::Ctx* cx, gp::El* b, gp::El* icon, Color fg, bool enabled) {
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    DeleteMenuModel(ui->ttsMenu);
    ui->ttsMenu = new MenuModel();
    RebuildReadAloudMenu(win, ui->ttsMenu);

    int iconPad = DpiScale(6);
    int dropDx = DpiScale(kIconBtnDropdownDx);
    b->PadX(0)
        ->W((float)(tb->iconSize + 2 * iconPad + dropDx))
        ->OnMouseMove(gp::ListenTo(ui->view, &ToolbarView::OnTtsButtonMove))
        ->OnMouseDown(gp::ListenTo(ui->view, &ToolbarView::OnTtsButtonDown));
    gp::El* action = gp::Div(cx->a)->FlexRow()->Flex1()->H((float)tb->rowDy)->ItemsCenter()->JustifyCenter();
    action->Child(icon);
    gp::El* drop =
        gp::Div(cx->a)->FlexRow()->W((float)dropDx)->H((float)tb->rowDy)->Shrink0()->ItemsCenter()->JustifyCenter();
    // U+25BE: the same small filled triangle as the Win32 TBSTYLE_EX_DRAWDDARROWS glyph
    drop->Child(gp::TextEl(cx->a, GStrL("\xE2\x96\xBE"))->Font(14)->Fg(ToGpui(fg)));
    if (enabled && ui->ttsHovered) {
        (ui->ttsHoverOnDropdown ? drop : action)->Bg(ToGpui(TbHoverColor()));
    }
    b->Child(action);
    b->Child(drop);

    gpc::PopupMenu* popup = TrackPopup(cx, TtsPopupFromModel(win, cx, ui->ttsMenu, StrL("tb-tts-menu")));
    ui->ttsPopup = popup->state;
    gp::El* group = gp::Div(cx->a)->FlexRow()->H((float)tb->rowDy)->Shrink0()->ItemsCenter()->Child(b);
    return gpc::ContextMenu::New(cx, GStrL("tb-tts-ctx"))->Child(group)->Menu(popup)->IntoEl();
}

static gp::El* ToolbarButton(MainWindow* win, gp::Ctx* cx, int idx, const ToolbarButtonInfo& bi, bool enabled,
                             bool checked) {
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    int iconPad = DpiScale(6);
    Color fg = enabled ? TbTextColor() : TbDisabledColor();

    gp::El* b = gp::Div(cx->a)
                    ->FlexRow()
                    ->H((float)tb->rowDy)
                    ->Shrink0()
                    ->ItemsCenter()
                    ->JustifyCenter()
                    ->PadX((float)iconPad)
                    ->BoundsOut(&ui->btnBounds[idx])
                    // ng: gpui tracks the hover by click path, so a disabled
                    // button has one too, or it gets no tooltip / drop-down
                    ->PathClick(GpuiDup(cx->a, fmt("tb-btn-%d", idx)))
                    ->OnHover(gp::ListenTo(ui->view, &ToolbarView::OnButtonHover, (intptr_t)idx));
    if (checked) {
        b->Bg(ToGpui(TbSelectedColor()));
    }
    bool hasDropdown = (bi.cmdId == CmdToggleReadAloud);
    if (enabled) {
        if (!hasDropdown) {
            b->HoverBg(ToGpui(TbHoverColor()));
        }
        b->Cursor(gp::CursorKind::Pointer)
            ->OnClick(gp::ListenTo(ui->view, &ToolbarView::OnButton, (intptr_t)idx))
            ->OnMouseUp(gp::ListenTo(ui->view, &ToolbarView::OnButtonUp, (intptr_t)idx));
    }
    bool noTranslate = idx >= gLayoutButtonsCount;
    if (bi.isText) {
        Str label = noTranslate ? bi.toolTip : trans::GetTranslation(bi.toolTip);
        b->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(13)->Fg(ToGpui(fg)));
        return b;
    }
    Str svg = bi.svgIcon ? bi.svgIcon : Str(bi.icon);
    // orig swaps the Read Aloud icon while a voice is speaking
    if (bi.cmdId == CmdToggleReadAloud && TtsIsSpeaking()) {
        svg = Str(gIconPauseSpeaking);
    }
    if (hasDropdown) {
        return ToolbarReadAloudGroup(win, cx, b, ToolbarIcon(cx, svg, tb->iconSize, fg), fg, enabled);
    }
    b->W((float)(tb->iconSize + 2 * iconPad));
    b->Child(ToolbarIcon(cx, svg, tb->iconSize, fg));
    return b;
}

// orig's gPdfAnnotationButtons row, under the toolbar. A placement mode owns
// the page until it ends, so every button is disabled for its duration.
static gp::El* BuildAnnotRow(MainWindow* win, gp::Ctx* cx, BuildMenuCtx* ctx) {
    if (!AnnotRowVisible(win)) {
        return nullptr;
    }
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    bool buttonsEnabled = !IsPlacingAnnotation(win);

    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->W(gp::kFill)
                      ->H((float)tb->rowDy)
                      ->Shrink0()
                      ->ItemsCenter()
                      ->PadX((float)DpiScale(4))
                      ->Bg(ToGpui(TbBgColor()))
                      ->OnHover(gp::ListenTo(ui->view, &ToolbarView::OnBarHover))
                      ->OnScrollWheel(gp::ListenTo(ui->view, &ToolbarView::OnBarWheel));
    bool prevVisibleNonSep = false;
    for (int i = 0; i < kPdfAnnotationButtonsCount; i++) {
        ToolbarButtonInfo bi = gPdfAnnotationButtons[i];
        if (bi.cmdId == 0 || !HasToolbarButtonContent(bi)) {
            if (prevVisibleNonSep) {
                row->Child(ToolbarSeparator(cx, tb->rowDy));
            }
            prevVisibleNonSep = false;
            continue;
        }
        if (!IsCmdAvailable(win, bi.cmdId, ctx)) {
            continue;
        }
        bool enabled = buttonsEnabled && IsCmdEnabled(win, bi.cmdId, ctx);
        VecAppend(ui->btnCmds, bi.cmdId);
        row->Child(ToolbarButton(win, cx, len(ui->btnCmds) - 1, bi, enabled, IsCmdChecked(win, bi.cmdId)));
        prevVisibleNonSep = true;
    }
    row->Child(gp::Div(cx->a)->Flex1()->H(gp::kFill)->Click(gp::ClickWinCaption));
    return row;
}

// the page number box: "Page:" / "Chapter:", the chapter box for a document
// that has chapters, the page box and the " / N" after it
static void AddPageInfo(MainWindow* win, gp::Ctx* cx, gp::El* row) {
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    Color fg = TbTextColor();
    // Old toolbar: the label HWND was text + kTextPaddingRight + kButtonSpacingX
    // so "Page:" and "/ N" were not flush against the edit
    float pageGap = (float)(DpiScale(kTextPaddingRight) + DpiScale(kButtonSpacingX));
    float smallGap = (float)DpiScale(4);
    float editDx = (float)DpiScale(52);

    // the group stands in for the page box when a rect is asked for
    int idx = len(ui->btnCmds);
    VecAppend(ui->btnCmds, PageInfoId);
    gp::El* group =
        gp::Div(cx->a)->FlexRow()->H((float)tb->rowDy)->Shrink0()->ItemsCenter()->BoundsOut(&ui->btnBounds[idx]);

    auto text = [&](Str s, float padL, float padR) {
        return gp::TextEl(cx->a, GpuiDup(cx->a, s))->Font(13)->Fg(ToGpui(fg))->PadL(padL)->PadR(padR);
    };
    auto edit = [&](gp::Str id, gp::InputState* st) {
        return gpc::Input::New(cx, id, st)
            ->WithSize(gp::UiSize::Small)
            ->Align(gpc::InputAlign::Right)
            ->W(editDx)
            ->IntoEl();
    };

    group->Child(text(tb->hasChapters ? Tr("Chapter:") : Tr("Page:"), smallGap, pageGap));
    if (tb->hasChapters) {
        group->Child(edit(GStrL("tb-chapter"), tb->chapterEdit));
        group->Child(text(tb->chapterTotal, pageGap, smallGap));
        group->Child(text(Tr("Page:"), smallGap, pageGap));
    }
    group->Child(edit(GStrL("tb-page"), tb->pageEdit));
    group->Child(text(tb->pageTotal, pageGap, smallGap));
    row->Child(group);
}

static bool IsAllDigits(gp::Str s, int64_t) {
    for (int i = 0; i < s.len; i++) {
        if (s.s[i] < '0' || s.s[i] > '9') {
            return false;
        }
    }
    return true;
}

static gp::InputState* EnsureLocationEdit(MainWindow* win, gp::InputState** slot) {
    if (*slot) {
        return *slot;
    }
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    s->validate = IsAllDigits;
    *slot = s;
    return s;
}

static void SetEditText(gp::InputState* st, Str s) {
    if (!str::Eq(FromGpui(gp::InputValue(st)), s)) {
        gp::InputSetValue(st, ToGpui(s));
    }
}

// ng: orig pushes the page number into the edit from PageNoChanged and
// UpdateToolbarPageText; the row is rebuilt from the model here, so the boxes
// are synced on the way in - unless one has the focus and is being typed into
static void SyncLocationEdits(MainWindow* win) {
    Toolbar* tb = win->toolbar;
    if (IsToolbarPageBoxFocused(win)) {
        return;
    }
    DocController* ctrl = win->ctrl;
    if (!ctrl) {
        SetEditText(tb->pageEdit, {});
        SetEditText(tb->chapterEdit, {});
        return;
    }
    if (tb->hasChapters) {
        Location cur = ctrl->CurrentLocation();
        SetEditText(tb->chapterEdit, fmt("%d", cur.chapter));
        SetEditText(tb->pageEdit, fmt("%d", cur.page));
        return;
    }
    SetEditText(tb->chapterEdit, {});
    SetEditText(tb->pageEdit, ctrl->GetPageLabeTemp(ctrl->CurrentPageNo()));
}

// ng: orig dumps the toolbar for its -dbg-control tests
// (ToolbarButtonsResultTemp); the log is where this port's tests read it. Only
// when it changed: this runs on every frame.
static Str gLoggedButtons;

static void LogToolbarButtons(MainWindow* win, const Vec<bool>& hidden, AppCommandCtx* ctx) {
    str::Builder b;
    int n = TotalButtonsCount();
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (hidden[i]) {
            continue;
        }
        b.AppendNonEmpty(len(b) == 0 ? StrL("") : StrL(" "));
        if (bi.cmdId == 0) {
            b.Append(StrL("|"));
            continue;
        }
        if (bi.cmdId == PageInfoId) {
            b.Append(StrL("PageInfo"));
            continue;
        }
        Str name = GetCommandName(bi.cmdId);
        b.Append(len(name) > 0 ? name : StrL("Custom"));
        if (!IsCmdEnabled(win, bi.cmdId, ctx)) {
            b.Append(StrL("(off)"));
        } else if (IsCmdChecked(win, bi.cmdId)) {
            b.Append(StrL("(checked)"));
        }
    }
    Str s = b.TakeStr();
    if (!str::Eq(s, gLoggedButtons)) {
        str::ReplaceWithCopy(&gLoggedButtons, s);
        logf("ToolbarButtons: %s\n", s);
    }
    str::Free(s);
}

gp::El* ToolbarBuild(MainWindow* win, gp::Ctx* cx) {
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    if (!ui->view.IsValid()) {
        ui->view = gp::EntityNewState<ToolbarView>(cx->app);
    }
    ui->view.Get(cx)->win = win;

    if (tb->rowDy == 0) {
        CreateToolbar(win);
    }
    bool floating = win->isToolbarOverlay;
    if (!win->isToolbarVisible && !floating) {
        return nullptr;
    }
    if (floating && !tb->overlayShown) {
        // the canvas reveals it from its mouse move, as orig does
        return nullptr;
    }

    PopulateToolbarLayout();
    PopulateCustomToolbarButtons();
    EnsureLocationEdit(win, &tb->pageEdit);
    EnsureLocationEdit(win, &tb->chapterEdit);
    tb->pageEdit->onChange = gp::ListenTo(ui->view, &ToolbarView::OnPageInput);
    tb->chapterEdit->onChange = gp::ListenTo(ui->view, &ToolbarView::OnChapterInput);
    ComputePageText(win, win->ctrl ? win->ctrl->PageCount() : 0);
    SyncLocationEdits(win);

    int n = TotalButtonsCount();
    // gpui writes each button's laid-out rect back through BoundsOut, so the
    // slots have to exist (and keep their address) before the row is built
    VecReset(ui->btnCmds);
    while (len(ui->btnBounds) < n + kPdfAnnotationButtonsCount + 1) {
        VecAppend(ui->btnBounds, gp::Bounds{});
    }

    auto* ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
    AutoCall delCtx(DeleteBuildMenuCtx, ctx);

    Vec<bool> hidden;
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        bool hide = false;
        if (bi.cmdId != WarningMsgId && bi.cmdId != 0) {
            hide = !IsCmdAvailable(win, bi.cmdId, ctx);
        }
        VecAppend(hidden, hide);
    }
    // drop a separator that would sit next to another, or at either end
    // (Read Aloud is hidden by default, which would otherwise leave ||)
    bool prevVisibleNonSep = false;
    int lastSep = -1;
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (bi.cmdId == 0) {
            hidden[i] = !prevVisibleNonSep;
            prevVisibleNonSep = false;
            if (!hidden[i]) {
                lastSep = i;
            }
            continue;
        }
        if (!hidden[i]) {
            prevVisibleNonSep = true;
            lastSep = -1;
        }
    }
    if (lastSep >= 0) {
        hidden[lastSep] = true;
    }

    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->H((float)tb->rowDy)
                      ->Shrink0()
                      ->ItemsCenter()
                      ->PadX((float)DpiScale(4))
                      ->Bg(ToGpui(TbBgColor()))
                      ->OnHover(gp::ListenTo(ui->view, &ToolbarView::OnBarHover))
                      ->OnScrollWheel(gp::ListenTo(ui->view, &ToolbarView::OnBarWheel));
    if (!floating) {
        row->W(gp::kFill);
    }
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (hidden[i]) {
            continue;
        }
        if (bi.cmdId == PageInfoId) {
            AddPageInfo(win, cx, row);
            continue;
        }
        if (bi.cmdId == 0 || !HasToolbarButtonContent(bi)) {
            row->Child(ToolbarSeparator(cx, tb->rowDy));
            continue;
        }
        bool enabled = IsCmdEnabled(win, bi.cmdId, ctx);
        VecAppend(ui->btnCmds, bi.cmdId);
        row->Child(ToolbarButton(win, cx, len(ui->btnCmds) - 1, bi, enabled, IsCmdChecked(win, bi.cmdId)));
    }
    if (!floating) {
        row->Child(gp::Div(cx->a)->Flex1()->H(gp::kFill)->Click(gp::ClickWinCaption));
    }
    LogToolbarButtons(win, hidden, ctx);

    if (tb->wantPageFocus) {
        tb->wantPageFocus = false;
        gp::InputFocus(tb->pageEdit, cx->app, cx->win);
    }
    if (tb->wantSelectAll) {
        gp::InputState* edit = tb->wantSelectAll;
        tb->wantSelectAll = nullptr;
        gp::InputSelectAll(edit, cx->app, cx->win);
    }

    if (!floating) {
        // orig's PaintToolbarEdge: the default theme separates the toolbar
        // from the canvas with a hairline. Use the document background, not
        // ThemeEdgeColor: on Light that is #c0c0c0 and reads as a dark strip
        // against the page. It is the toolbar's own last row of pixels
        gp::El* box = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0();
        gp::El* line = nullptr;
        if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
            Color canvasBg;
            ThemeDocumentColors(canvasBg);
            line = gp::Div(cx->a)->Absolute()->Left(0)->W(gp::kFill)->H(1)->Bg(ToGpui(canvasBg));
        }
        gp::El* annotRow = BuildAnnotRow(win, cx, ctx);
        if (ToolbarAtBottom()) {
            if (annotRow) {
                box->Child(annotRow);
            }
            box->Child(row);
            if (line) {
                box->Child(line->Top(0));
            }
            return box;
        }
        box->Child(row);
        if (annotRow) {
            box->Child(annotRow);
        }
        if (line) {
            box->Child(line->Bottom(0));
        }
        return box;
    }
    // orig floats the bar at its natural width, centered over the canvas. The
    // container it sits in is transparent and has no listener, so gpui gives it
    // no hit rect and the canvas keeps the mouse beside the bar.
    row->Border(1, th.border);
    Rect canvas = win->canvasRc;
    float y = (float)OverlayToolbarY(win);
    gp::El* box = gp::Div(cx->a)
                      ->FlexRow()
                      ->JustifyCenter()
                      ->Absolute()
                      ->Left((float)canvas.x)
                      ->Top(y)
                      ->W((float)canvas.dx)
                      ->H((float)tb->rowDy);
    box->Child(row);
    return box;
}

// what floats over the canvas: the zoom drop-down. Built after the row so the
// button rects it hangs off are the ones the last frame laid out.
gp::El* ToolbarOverlayBuild(MainWindow* win, gp::Ctx* cx) {
    Toolbar* tb = win->toolbar;
    if (!tb || tb->hoverCmdId == 0) {
        return nullptr;
    }
    if (tb->hoverCmdId == CmdSaveAnnotations) {
        return BuildSaveHoverMenu(win, cx);
    }
    if (CmdIsAnnotColorDropdown(tb->hoverCmdId)) {
        return BuildAnnotColorStrip(win, cx);
    }
    return BuildZoomStrip(win, cx);
}

// --- listeners --------------------------------------------------------------

void ToolbarView::OnButton(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win) || !ev || ev->button != gp::MouseButton::Left) {
        return;
    }
    ToolbarUI* ui = Ui(win);
    if (idx < 0 || idx >= len(ui->btnCmds)) {
        return;
    }
    int cmdId = ui->btnCmds[(int)idx];
    if (cmdId == CmdToggleReadAloud) {
        if (ui->ttsMenuWasOpen) {
            ui->ttsMenuWasOpen = false;
            return;
        }
        // the arrow inside the button opens the menu
        float dropDx = (float)DpiScale(kIconBtnDropdownDx);
        if (ev->el.w > 0 && ev->x >= ev->el.x + ev->el.w - dropDx) {
            ShowTtsVoiceMenu(win, cx);
            return;
        }
    }
    HideToolbarHoverDropdown(win);
    // orig leaves the pending id on the button, so resting there does not open
    // the drop-down again: a save just ended the session those rows are for
    if (cmdId == CmdSaveAnnotations || CmdIsAnnotColorDropdown(cmdId)) {
        Tb(win)->hoverPendingCmdId = cmdId;
    }
    ExecuteCmd(win, cmdId);
    gp::Notify(cx);
}

// orig's OnToolbarButtonClicked fires for any mouse button: a right click
// opens the button's drop-down when it has one, anything else runs the command
void ToolbarView::OnButtonUp(ToolbarView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t idx) {
    bool context = IsContextClick(ev->button, ev->modifiers);
    if (!context && ev->button != gp::MouseButton::Middle) {
        return;
    }
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    ToolbarUI* ui = Ui(win);
    if (idx < 0 || idx >= len(ui->btnCmds)) {
        return;
    }
    int cmdId = ui->btnCmds[(int)idx];
    // right-click: the drop-down, not the button's command
    if (context && cmdId == CmdToggleReadAloud) {
        ShowTtsVoiceMenu(win, cx);
        return;
    }
    if (context && CmdHasHoverDropdown(cmdId)) {
        Toolbar* tb = Tb(win);
        // already up for this button: a second right-click leaves that one
        if (tb->hoverCmdId != cmdId) {
            bool sameStrip = tb->hoverCmdId != 0 && ZoomHoverGroup(tb->hoverAnchorCmdId) && ZoomHoverGroup(cmdId);
            if (!sameStrip) {
                tb->hoverGen++;
                tb->hoverAnchorCmdId = cmdId;
            }
            tb->hoverCmdId = cmdId;
        }
        tb->hoverPendingCmdId = 0;
        tb->hoverOpenLeftMs = 0;
        tb->hoverCloseLeftMs = 0;
        HoverTooltipHide(cx);
        gp::Notify(cx);
        return;
    }
    gp::ClickEvent click;
    click.button = ev->button;
    OnButton(self, cx, &click, idx);
}

void ToolbarView::OnButtonHover(ToolbarView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    Toolbar* tb = Tb(win);
    ToolbarUI* ui = Ui(win);
    if (idx < 0 || idx >= len(ui->btnCmds)) {
        return;
    }
    int cmdId = ui->btnCmds[(int)idx];
    if (cmdId == CmdToggleReadAloud && ui->ttsHovered != ev->hovered) {
        ui->ttsHovered = ev->hovered;
        if (!ev->hovered) {
            ui->ttsHoverOnDropdown = false;
        }
        AppShellInvalidate(win);
    }
    if (!ev->hovered) {
        if (tb->hoverPendingCmdId == cmdId) {
            tb->hoverPendingCmdId = 0;
            tb->hoverOpenLeftMs = 0;
        }
        HoverTooltipHide(cx);
        return;
    }
    tb->overlayHideLeftMs = 0;
    // a button with a drop-down opens it instead of showing a tooltip once the
    // mouse has rested on it; buttons in the same group share the one that is up
    if (CmdHasHoverDropdown(cmdId)) {
        tb->hoverCloseLeftMs = 0;
        if (tb->hoverCmdId != 0) {
            // Zoom In and Zoom Out share the strip. The mouse's button changes
            // (its tooltip goes); the strip stays on the button that opened it.
            bool sameStrip = ZoomHoverGroup(tb->hoverAnchorCmdId) && ZoomHoverGroup(cmdId);
            tb->hoverCmdId = cmdId;
            if (!sameStrip) {
                tb->hoverAnchorCmdId = cmdId;
            }
        } else if (tb->hoverPendingCmdId != cmdId) {
            tb->hoverPendingCmdId = cmdId;
            tb->hoverOpenLeftMs = kOpenHoverDropdownDelay;
        }
    } else if (tb->hoverCmdId != 0 && tb->hoverCloseLeftMs <= 0) {
        tb->hoverCloseLeftMs = kCloseHoverDropdownDelayMs;
    }
    // the tooltip is orig's: the label plus the command's shortcut
    Str tip;
    for (int i = 0, n = TotalButtonsCount(); i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (bi.cmdId != cmdId || len(bi.toolTip) == 0) {
            continue;
        }
        bool translate = i < gLayoutButtonsCount && !bi.isText;
        tip = ToolbarTipTemp(cmdId, bi.toolTip, translate);
        if (cmdId == CmdToggleReadAloud || cmdId == CmdPauseReadAloud) {
            // tooltip reflects what clicking the button will do
            Str label = Tr("Read Aloud");
            if (TtsIsSpeaking()) {
                label = Tr("Pause Reading");
            } else if (CanContinueReadAloud(win->CurrentTab())) {
                label = Tr("Continue Reading");
            }
            tip = ToolbarTipTemp(cmdId, label, false);
        }
        break;
    }
    if (len(tip) == 0) {
        for (const ToolbarButtonInfo& bi : gPdfAnnotationButtons) {
            if (bi.cmdId != cmdId || len(bi.toolTip) == 0) {
                continue;
            }
            Str label = bi.toolTip;
            // the Save button names the file it writes to, as the list does
            WindowTab* tab = win->CurrentTab();
            TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
            if (cmdId == CmdSaveAnnotations && len(base) > 0) {
                tip = ToolbarTipTemp(cmdId, fmt(Tr("Save changes to %s").s, base), false);
                break;
            }
            tip = ToolbarTipTemp(cmdId, label, true);
            break;
        }
    }
    if (len(tip) > 0 && tb->hoverCmdId != cmdId) {
        HoverTooltipShow(cx, tip, ui->btnBounds[(int)idx]);
    }
    gp::Notify(cx);
}

// ng: a command run from a gpui popup runs while the popup still holds the
// keyboard and the mouse, so it goes through the ui task queue
struct TtsMenuCmd {
    MainWindow* win;
    int cmdId;
};

static void RunTtsMenuCmd(TtsMenuCmd* c) {
    if (IsMainWindowValidAndNotClosing(c->win)) {
        ExecuteCmd(c->win, c->cmdId);
    }
    delete c;
}

void ToolbarView::OnTtsItem(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    auto* c = new TtsMenuCmd{self->win, (int)cmdId};
    uitask::Post(MkFunc0<TtsMenuCmd>(RunTtsMenuCmd, c), "TtsMenuCmd");
    gp::Notify(cx);
}

// orig's VirtIconButton::OnMouseMove: which half of the button the mouse is on
void ToolbarView::OnTtsButtonMove(ToolbarView* self, gp::Ctx*, const gp::MouseMoveEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    ToolbarUI* ui = Ui(win);
    bool onDrop = ev->x >= ev->el.x + ev->el.w - (float)DpiScale(kIconBtnDropdownDx);
    if (onDrop == ui->ttsHoverOnDropdown && ui->ttsHovered) {
        return;
    }
    ui->ttsHovered = true;
    ui->ttsHoverOnDropdown = onDrop;
    AppShellInvalidate(win);
}

void ToolbarView::OnTtsButtonDown(ToolbarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (ev->button == gp::MouseButton::Right) {
        // ng: gpui's ContextMenu would open at the press; orig's menu drops
        // under the button when the button comes up
        gp::WindowStopPropagation(cx);
    }
    ToolbarUI* ui = Ui(win);
    gp::PopupMenuState* st = ui->ttsPopup.Get(cx);
    ui->ttsMenuWasOpen = st && st->open;
}

void ToolbarView::OnStripCell(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    HideToolbarHoverDropdown(win);
    ExecuteCmd(win, (int)cmdId);
    gp::Notify(cx);
}

// picking a color in an annotation button's drop-down makes it the color that
// button's next annotation is made in
void ToolbarView::OnAnnotSwatch(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    Toolbar* tb = Tb(win);
    int cmdId = tb->hoverCmdId;
    Vec<Color> colors;
    AnnotPresetColors(cmdId, colors);
    HideToolbarHoverDropdown(win);
    if (VecIsValidIndex(colors, (int)idx)) {
        SetAnnotPresetColor(cmdId, colors[(int)idx]);
        if (CanCreateAnnotFromSelection(win, cmdId)) {
            // with text selected, picking a color is also a request to mark it up
            ExecuteCmd(win, cmdId);
        }
    }
    gp::Notify(cx);
}

// which button's color the generic color dialog is editing
struct AnnotColorsTarget {
    int cmdId = 0;
};

static void AnnotColorsPicked(AnnotColorsTarget* target, ChangeColorsArgs* args) {
    Str* list = AnnotPresetColorList(target->cmdId);
    if (args->colorsChanged && list) {
        str::ReplaceWithCopy(list, SerializeColorList(args->colors));
        ScheduleSaveSettings();
    }
    if (args->didSelect && args->color != kColorUnset) {
        SetAnnotPresetColor(target->cmdId, args->color);
    }
    delete target;
}

// orig's OnAnnotColorsEditClicked
void ToolbarView::OnAnnotColorsEdit(ToolbarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    HideToolbarHoverDropdown(win);

    auto* target = new AnnotColorsTarget();
    target->cmdId = (int)cmdId;

    auto* args = new ChangeColorsArgs();
    args->win = win;
    args->title = Tr("Annotation Colors");
    args->color = AnnotColorForCmd((int)cmdId);
    args->withOpacity = true;
    AnnotPresetColors((int)cmdId, args->colors);
    args->onClose = MkFunc1(AnnotColorsPicked, target);
    ShowChangeColorsDialog(args);
    gp::Notify(cx);
    AppShellInvalidate(win);
}

void ToolbarView::OnInkThickness(ToolbarView* self, gp::Ctx* cx, const gp::SliderEvent* ev) {
    if (!IsMainWindowValidAndNotClosing(self->win) || !gSettings) {
        return;
    }
    int width = limitValue((int)(ev->value.End() + 0.5f), kInkThicknessMin, kInkThicknessMax);
    gSettings->annotations.inkBorderWidth = width;
    if (ev->kind == gp::SliderEventKind::Release) {
        logf("Toolbar: ink thickness %d\n", width);
        ScheduleSaveSettings();
    }
    gp::Notify(cx);
}

// the pointer is on this drop-down, or on a button that shares it. A leave
// notification can arrive after the move that landed on the other zoom button,
// and that must not start the close.
static bool PointKeepsHover(MainWindow* win, float x, float y) {
    Toolbar* tb = Tb(win);
    if (!tb || tb->hoverCmdId == 0) {
        return false;
    }
    ToolbarUI* ui = tb->ui;
    if (ui && ui->hoverBox.w > 1 && ui->hoverBox.h > 1) {
        gp::Bounds b = ui->hoverBox;
        if (x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h) {
            return true;
        }
    }
    auto onBtn = [&](int cmdId) {
        Rect r = GetToolbarButtonRect(win, cmdId);
        return !r.IsEmpty() && x >= (float)r.x && y >= (float)r.y && x < (float)r.x + (float)r.dx &&
               y < (float)r.y + (float)r.dy;
    };
    if (onBtn(tb->hoverAnchorCmdId) || onBtn(tb->hoverCmdId)) {
        return true;
    }
    if (ZoomHoverGroup(tb->hoverAnchorCmdId)) {
        return onBtn(CmdZoomIn) || onBtn(CmdZoomOut);
    }
    return false;
}

static bool PointerKeepsHover(MainWindow* win) {
    gp::Window* gw = win ? win->gpuiWin : nullptr;
    if (!gw) {
        return false;
    }
    return PointKeepsHover(win, gw->mouseX, gw->mouseY);
}

// the pointer is over the bar (or the drop-down): keep the overlay up
void ToolbarView::OnBarHover(ToolbarView* self, gp::Ctx* cx, const gp::HoverEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    Toolbar* tb = Tb(win);
    if (ev->hovered) {
        tb->overlayHideLeftMs = 0;
        return;
    }
    if (tb->overlayShown) {
        tb->overlayHideLeftMs = kDelayToolbarHide;
    }
    // the pointer may be on its way to the drop-down, or across to the other
    // zoom button: OnStripHover cancels, and so does landing on that button
    if (tb->hoverCmdId != 0 && tb->hoverCloseLeftMs <= 0 && !PointerKeepsHover(win)) {
        tb->hoverCloseLeftMs = kCloseHoverDropdownDelayMs;
    }
    (void)cx;
}

// the drop-down itself: the pointer on it keeps it open
void ToolbarView::OnStripHover(ToolbarView* self, gp::Ctx*, const gp::HoverEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    Toolbar* tb = Tb(win);
    if (ev->hovered) {
        tb->overlayHideLeftMs = 0;
        tb->hoverCloseLeftMs = 0;
        return;
    }
    if (tb->overlayShown) {
        tb->overlayHideLeftMs = kDelayToolbarHide;
    }
    if (tb->hoverCmdId != 0 && !PointerKeepsHover(win)) {
        tb->hoverCloseLeftMs = kCloseHoverDropdownDelayMs;
    }
}

void ToolbarView::OnBarWheel(ToolbarView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    DocCanvasWheelFromFrame(self->win, cx, ev);
}

// orig's OnLocationEditChar: Enter goes to the page (or chapter + page)
static void OnLocationInput(MainWindow* win, gp::Ctx* cx, const gp::InputEvent* ev, gp::InputState* edit) {
    if (!IsMainWindowValidAndNotClosing(win) || !win->IsDocLoaded()) {
        return;
    }
    Toolbar* tb = Tb(win);
    // orig creates both boxes with selectAllOnFocus, so typing replaces the
    // page number instead of inserting into it
    // ng: gpui places the caret from the click that focused the box, after the
    // Focus event, so the selection has to be made on the next frame
    if (ev->kind == gp::InputEventKind::Focus) {
        tb->wantSelectAll = edit;
        AppShellInvalidate(win);
        return;
    }
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    DocController* ctrl = win->ctrl;
    if (ShowChapterUi(ctrl)) {
        int chapter = ParseInt(FromGpui(gp::InputValue(tb->chapterEdit)));
        int page = ParseInt(FromGpui(gp::InputValue(tb->pageEdit)));
        Location loc = ctrl->ClampLocation({chapter, page});
        ctrl->GoToLocation(loc, true);
    } else {
        TempStr s = str::DupTemp(FromGpui(gp::InputValue(tb->pageEdit)));
        int newPageNo = ctrl->GetPageByLabel(s);
        logf("ToolbarPageBox: '%s' -> page %d\n", s, newPageNo);
        if (!ctrl->ValidPageNo(newPageNo)) {
            return;
        }
        ctrl->GoToPage(newPageNo, true);
    }
    gp::InputBlur(tb->pageEdit, cx->app, cx->win);
    gp::InputBlur(tb->chapterEdit, cx->app, cx->win);
    UpdateOverlayToolbarForMouse(win, Point{});
    gp::Notify(cx);
}

void ToolbarView::OnPageInput(ToolbarView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    OnLocationInput(self->win, cx, ev, Tb(self->win)->pageEdit);
}

void ToolbarView::OnChapterInput(ToolbarView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    OnLocationInput(self->win, cx, ev, Tb(self->win)->chapterEdit);
}

// --- the tick ---------------------------------------------------------------

void ToolbarTick(MainWindow* win, int elapsedMs) {
    Toolbar* tb = win->toolbar;
    if (!tb) {
        return;
    }
    // the open frame guessed the pyramid's width. Once gpui has laid it out,
    // rebuild so it is centred on the button.
    if (tb->ui && tb->ui->hoverRecenter && tb->hoverCmdId != 0 && tb->ui->hoverBox.w > 1) {
        tb->ui->hoverRecenter = false;
        AppShellInvalidate(win);
    }
    if (tb->hoverOpenLeftMs > 0) {
        tb->hoverOpenLeftMs -= elapsedMs;
        if (tb->hoverOpenLeftMs <= 0) {
            tb->hoverOpenLeftMs = 0;
            tb->hoverGen++;
            tb->hoverCmdId = tb->hoverPendingCmdId;
            tb->hoverAnchorCmdId = tb->hoverCmdId;
            tb->hoverPendingCmdId = 0;
            // orig's TakeHoverButtonTooltip: the drop-down takes the tooltip's place
            if (win->gpuiWin) {
                gp::TooltipRequestHide(win->gpuiWin);
            }
            AppShellInvalidate(win);
        }
    }
    if (tb->hoverCloseLeftMs > 0) {
        tb->hoverCloseLeftMs -= elapsedMs;
        if (tb->hoverCloseLeftMs <= 0) {
            HideToolbarHoverDropdown(win);
            AppShellInvalidate(win);
        }
    }
    if (tb->overlayHideLeftMs > 0) {
        tb->overlayHideLeftMs -= elapsedMs;
        if (tb->overlayHideLeftMs <= 0) {
            tb->overlayHideLeftMs = 0;
            if (!IsToolbarPageBoxFocused(win)) {
                SetOverlayShown(win, false);
            }
        }
    }
}

// --- the page box -----------------------------------------------------------

// The page box is a gpui input, so tests cannot read it as a child Edit.
TempStr ToolbarPageBoxTextTemp(MainWindow* win) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    if (!tb || !tb->pageEdit) {
        return StrL("");
    }
    return str::DupTemp(FromGpui(gp::InputValue(tb->pageEdit)));
}

// Last document closed, window kept: the boxes still hold the old number.
void ClearToolbarLocationEdits(MainWindow* win) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    if (!tb) {
        return;
    }
    if (tb->pageEdit) {
        SetEditText(tb->pageEdit, {});
    }
    if (tb->chapterEdit) {
        SetEditText(tb->chapterEdit, {});
    }
    UpdateToolbarPageText(win, 0);
}

bool IsToolbarPageBoxFocused(MainWindow* win) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    if (!tb || !win->gpuiWin) {
        return false;
    }
    if (tb->pageEdit && gp::FocusHandleIsFocused(win->gpuiWin, tb->pageEdit->focus)) {
        return true;
    }
    return tb->chapterEdit && gp::FocusHandleIsFocused(win->gpuiWin, tb->chapterEdit->focus);
}

// orig's OnMenuGoToPage: with the toolbar up, Ctrl+G moves the focus into the
// page box instead of opening the dialog
bool ToolbarHasChapterBox(MainWindow* win) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    return tb && tb->hasChapters && tb->chapterEdit;
}

bool IsToolbarLocationBoxFocused(MainWindow* win, bool chapter) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    if (!tb || !win->gpuiWin) {
        return false;
    }
    gp::InputState* edit = chapter ? tb->chapterEdit : tb->pageEdit;
    return edit && gp::FocusHandleIsFocused(win->gpuiWin, edit->focus);
}

void ToolbarFocusLocationBox(MainWindow* win, bool chapter) {
    Toolbar* tb = win ? win->toolbar : nullptr;
    gp::InputState* edit = tb ? (chapter ? tb->chapterEdit : tb->pageEdit) : nullptr;
    if (!edit || !win->gpuiWin) {
        return;
    }
    RevealOverlayToolbar(win);
    gp::InputFocus(edit, win->gpuiWin->app, win->gpuiWin);
    // orig creates the boxes with selectAllOnFocus
    tb->wantSelectAll = edit;
    AppShellInvalidate(win);
}

bool ToolbarFocusPageBox(MainWindow* win) {
    if (!win->isToolbarVisible && !win->isToolbarOverlay) {
        return false;
    }
    RevealOverlayToolbar(win);
    Tb(win)->wantPageFocus = true;
    AppShellInvalidate(win);
    return true;
}

// Escape leaves the page box; the shell asks before it does anything else
bool ToolbarOnEscape(MainWindow* win) {
    // orig's OnFrameKeyEsc: Esc closes an open hover drop-down first
    if (Tb(win) && Tb(win)->hoverCmdId != 0) {
        HideToolbarHoverDropdown(win);
        AppShellInvalidate(win);
        return true;
    }
    if (!IsToolbarPageBoxFocused(win)) {
        return false;
    }
    Toolbar* tb = Tb(win);
    if (win->gpuiWin) {
        gp::InputBlur(tb->pageEdit, win->gpuiWin->app, win->gpuiWin);
        gp::InputBlur(tb->chapterEdit, win->gpuiWin->app, win->gpuiWin);
    }
    UpdateOverlayToolbarForMouse(win, Point{});
    AppShellInvalidate(win);
    return true;
}
