/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/BitManip.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/UITask.h"
#include "base/Win.h"

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
#include "Notifications.h"
#include "Canvas.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "AppTools.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "SearchAndDDE.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "Tabs.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/VirtHost.h"
#include "gui/win/TabsCtrl.h"
#include "FindBar.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Theme.h"
#include "TextToSpeech.h"
#include "Toolbar.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/toolbar-control-reference

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
    {gIconFileOpen, CmdOpenFile, _TRN("Open")},
    {gIconPrint, CmdPrint, _TRN("Print")},
    {nullptr, 0, {}},          // separator
    {nullptr, PageInfoId, {}}, // text box for page number + show current page / no of pages
    {gIconPagePrev, CmdGoToPrevPage, _TRN("Previous Page")},
    {gIconPageNext, CmdGoToNextPage, _TRN("Next Page")},
    {nullptr, 0, {}}, // separator
    {gIconNavigateBack, CmdNavigateBack, _TRN("Back")},
    {gIconNavigateForward, CmdNavigateForward, _TRN("Forward")},
    {nullptr, 0, {}}, // separator
    {gIconSpeak, CmdReadAloud, _TRN("Read Aloud")},
    {nullptr, 0, {}}, // separator
    {gIconLayoutContinuous, CmdZoomFitWidthAndContinuous, _TRN("Fit Width and Show Pages Continuously")},
    {gIconLayoutSinglePage, CmdZoomFitPageAndSinglePage, _TRN("Fit a Single Page")},
    {gIconRotateLeft, CmdRotateLeft, _TRN("Rotate &Left")},
    {gIconRotateRight, CmdRotateRight, _TRN("Rotate &Right")},
    {gIconZoomOut, CmdZoomOut, _TRN("Zoom Out")},
    {gIconZoomIn, CmdZoomIn, _TRN("Zoom In")},
    {nullptr, 0, {}}, // separator
    {gIconSearch, CmdFindFirst, _TRN("Find")},
    {nullptr, 0, {}}, // separator
    {gIconEditAnnotations, CmdToggleEditPDF, _TRN("Edit PDF")},
};
// unicode chars: https://www.compart.com/en/unicode/U+25BC

constexpr int kButtonsCount = dimof(gToolbarButtons);

static ToolbarButtonInfo gPdfAnnotationButtons[] = {
    {gIconAnnotHighlightBrush, CmdAnnotationHighlightBrush, _TRN("Highlighter")},
    {gIconAnnotHighlight, CmdCreateAnnotHighlight, _TRN("Highlight")},
    {gIconAnnotUnderline, CmdCreateAnnotUnderline, _TRN("Underline")},
    {gIconAnnotSquiggly, CmdCreateAnnotSquiggly, _TRN("Squiggly")},
    {gIconAnnotStrikeOut, CmdCreateAnnotStrikeOut, _TRN("Strike Out")},
    {nullptr, 0, {}},
    {gIconAnnotText, CmdCreateAnnotText, _TRN("Text")},
    {gIconAnnotFreeText, CmdCreateAnnotFreeText, _TRN("Free Text")},
    {nullptr, 0, {}},
    {gIconAnnotLine, CmdCreateAnnotLine, _TRN("Line")},
    {gIconAnnotPolyLine, CmdCreateAnnotPolyLine, _TRN("Polyline")},
    {gIconAnnotSquare, CmdCreateAnnotSquare, _TRN("Square")},
    {gIconAnnotCircle, CmdCreateAnnotCircle, _TRN("Circle")},
    {gIconAnnotPolygon, CmdCreateAnnotPolygon, _TRN("Polygon")},
    {gIconAnnotInk, CmdCreateAnnotInk, _TRN("Ink")},
    {nullptr, 0, {}},
    {gIconAnnotRedact, CmdCreateAnnotRedact, _TRN("Redact")},
    {gIconApplyRedactions, CmdApplyRedactions, _TRN("Apply Redactions")},
    {gIconAnnotStamp, CmdCreateAnnotStamp, _TRN("Stamp")},
    {gIconAnnotCaret, CmdCreateAnnotCaret, _TRN("Caret")},
    {gIconAnnotFileAttachment, CmdCreateAnnotFileAttachment, _TRN("File Attachment")},
    {nullptr, 0, {}},
    {gIconUndo, CmdUndo, _TRN("Undo")},
    {gIconRedo, CmdRedo, _TRN("Redo")},
    {nullptr, 0, {}},
    {gIconFindAnnotation, CmdFindAnnotation, _TRN("Find Annotation")},
    {nullptr, 0, {}},
    // the tooltip names the file, see ToolbarUpdateStateForWindow. Hovering it
    // opens a drop-down with the other two ways to end an editing session
    {gIconSave, CmdSaveAnnotations, _TRN("Save changes to existing PDF")},
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
// we use static array so that we don't have to generate
// code for Vec<ToolbarButtonInfo>
constexpr int kMaxCustomButtons = 127;
// +1 to ensure there's always space for WarningsMsgId button
static ToolbarButtonInfo gCustomButtons[kMaxCustomButtons + 1];
static int gCustomButtonsCount = 0;

// Light theme ControlBackgroundColor is white, which is what the old themed
// rebar/toolbar painted. Other themes use their control background.
static Color TbBgColor() {
    return ThemeControlBackgroundColor();
}

Color TbTextColor() {
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

// A ground a shade off the normal one, for telling two areas of a drop-down
// apart. Well short of the hover highlight, which is 20 units off: this is a
// cue, not something lit up.
static Color TbSubtleBgColor() {
    return AccentColor(TbBgColor(), 8);
}

static Color TbSelectedColor() {
    return AccentColor(TbBgColor(), 28);
}

static Color TbEdgeColor() {
    return ThemeEdgeColor();
}

// Old Win32 toolbar: TBMETRICS.cyPad defaults to 6, then we added DpiScale(2).
// TB_SETBUTTONSIZE cannot go below image + 2*cyPad, so that was the bar height.
static int ToolbarCyPad() {
    return 6 + DpiScale(2);
}

static int ToolbarRowDy(int iconSize) {
    return iconSize + (2 * ToolbarCyPad());
}

static bool HasToolbarButtonContent(const ToolbarButtonInfo& tbi) {
    return tbi.icon || tbi.isText || !str::IsEmptyOrWhiteSpace(tbi.svgIcon);
}

static VirtHost* ToolbarHost(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    return tb ? tb->host : nullptr;
}

static VirtCtrl* ToolbarItemAt(MainWindow* win, int idx) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || idx < 0 || idx >= len(tb->items)) {
        return nullptr;
    }
    return tb->items[idx];
}

static VirtCtrl* PdfAnnotationToolbarItemAt(MainWindow* win, int idx) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || idx < 0 || idx >= len(tb->annotationItems)) {
        return nullptr;
    }
    return tb->annotationItems[idx];
}

// Includes disabled items (those are not hit-testable), so a click on a gray
// button is not treated as empty toolbar and does not start a window drag.
VirtCtrl* ToolbarItemFromPoint(MainWindow* win, Point pt) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return nullptr;
    }
    for (VirtCtrl* w : tb->items) {
        if (!w || w->GetVisibility() != Visibility::Visible) {
            continue;
        }
        if (w->BoundsInWindow().Contains(pt)) {
            return w;
        }
    }
    for (VirtCtrl* w : tb->annotationItems) {
        if (!w || w->GetVisibility() != Visibility::Visible) {
            continue;
        }
        if (w->BoundsInWindow().Contains(pt)) {
            return w;
        }
    }
    if (tb->pageTotal && tb->pageTotal->GetVisibility() == Visibility::Visible &&
        tb->pageTotal->BoundsInWindow().Contains(pt)) {
        return tb->pageTotal;
    }
    if (tb->chapterTotal && tb->chapterTotal->GetVisibility() == Visibility::Visible &&
        tb->chapterTotal->BoundsInWindow().Contains(pt)) {
        return tb->chapterTotal;
    }
    return nullptr;
}

static void SetToolbarButtonEnabledByIdx(MainWindow* win, int idx, bool isEnabled) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w || w->IsEnabled() == isEnabled) {
        return;
    }
    w->SetIsEnabled(isEnabled);
    w->Invalidate();
}

static void SetPdfAnnotationButtonToolTipByIdx(MainWindow* win, int idx, Str tip) {
    VirtCtrl* w = PdfAnnotationToolbarItemAt(win, idx);
    if (w) {
        w->SetTooltip(tip);
    }
}

static void SetPdfAnnotationButtonEnabledByIdx(MainWindow* win, int idx, bool isEnabled) {
    VirtCtrl* w = PdfAnnotationToolbarItemAt(win, idx);
    if (!w || w->IsEnabled() == isEnabled) {
        return;
    }
    w->SetIsEnabled(isEnabled);
    w->Invalidate();
}

// true if the row has to be laid out again
static bool SetPdfAnnotationButtonHiddenByIdx(MainWindow* win, int idx, bool isHidden) {
    VirtCtrl* w = PdfAnnotationToolbarItemAt(win, idx);
    if (!w) {
        return false;
    }
    Visibility want = isHidden ? Visibility::Collapse : Visibility::Visible;
    if (w->GetVisibility() == want) {
        return false;
    }
    w->SetVisibility(want);
    return true;
}

// hiding the page box hides the whole group (label + edit + " / N")
static bool SetToolbarButtonHiddenByIdx(MainWindow* win, int idx, bool isHidden) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w) {
        return false;
    }
    Visibility want = isHidden ? Visibility::Collapse : Visibility::Visible;
    if (w->GetVisibility() == want) {
        return false;
    }
    w->SetVisibility(want);
    ToolbarVirt* tb = win->toolbarVirt;
    if (w->id == PageInfoId && tb) {
        if (tb->pageLabel) {
            tb->pageLabel->SetVisibility(want);
        }
        if (win->pageEdit) {
            win->pageEdit->SetVisibility(want);
        }
        if (tb->pageTotal) {
            tb->pageTotal->SetVisibility(want);
        }
        // chapter widgets stay collapsed unless the doc has chapters;
        // UpdateToolbarPageText() narrows this further right after
        if (win->chapterEdit) {
            win->chapterEdit->SetVisibility(want);
        }
        if (tb->chapterTotal) {
            tb->chapterTotal->SetVisibility(want);
        }
    }
    return true;
}

static void SetToolbarButtonCheckedByIdx(MainWindow* win, int idx, bool isChecked) {
    // a custom button with ToolbarText is a VirtButton, which has no
    // checked state (and is not a VirtIconButton)
    auto* ib = AsVirtIconButton(ToolbarItemAt(win, idx));
    if (!ib || ib->isSelected == isChecked) {
        return;
    }
    ib->isSelected = isChecked;
    ib->Invalidate();
}

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

static int TotalButtonsCount() {
    return gLayoutButtonsCount + gCustomButtonsCount;
}

static ToolbarButtonInfo& GetToolbarButtonInfoByIdx(int idx) {
    if (idx < gLayoutButtonsCount) return gLayoutButtons[idx];
    return gCustomButtons[idx - gLayoutButtonsCount];
}

static int OriginalCommandId(int cmdId) {
    CustomCommand* cmd = FindCustomCommand(cmdId);
    return cmd ? cmd->origId : cmdId;
}

void SetToolbarButtonCheckedState(MainWindow* win, int cmdId, bool isChecked) {
    int originalCmdId = OriginalCommandId(cmdId);
    int n = TotalButtonsCount();
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& tbi = GetToolbarButtonInfoByIdx(i);
        if (OriginalCommandId(tbi.cmdId) == originalCmdId) {
            SetToolbarButtonCheckedByIdx(win, i, isChecked);
        }
    }
}

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
            // CHM has its own (WebView2/IE) find bar even though NeedsFindUI()
            // is false for it; show the Search button so it's reachable
            return NeedsFindUI(win) || IsBrowserDocController(win->ctrl);
        case CmdFindNext:
        case CmdFindPrev:
        case CmdFindToggleMatchCase:
        case CmdFindToggleMatchWholeWord:
            return NeedsFindUI(win);
        case CmdReadAloud:
            // opt-in: the button and its drop-down only show if asked for
            return gSettings->toolbarShowReadAloud;
        case PageInfoId:
            return true;
    }
    // Toolbar buttons stay visible (but disabled) when no document is open, so
    // decide visibility as if a document were loaded; otherwise the no-document
    // gate in GetCommandVisibility would remove them. Document-type-specific
    // removals (e.g. for CHM/image collections) still apply when a real document
    // is loaded, and the enabled state is handled separately in IsCmdEnabled.
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

    // if no file is open, only enable buttons for commands that don't require a document
    // (custom toolbar buttons use a custom command id, the original command decides)
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/5657
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
            // Need non-empty find text (findEdit is the active bar or floating window edit).
            if (CbGetTextLen(win->findEdit) == 0) {
                return false;
            }
            // When we already know there are zero matches, disable next/prev.
            // Unknown count (scan pending / not started) still allows searching.
            if (win->ctrl && win->ctrl->CanFindInPage()) {
                if (win->browserFindTotal == 0) {
                    return false;
                }
                return true;
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

static TempStr ToolbarTipTemp(int cmdId, Str tip, bool translate) {
    TempStr s = translate ? trans::GetTranslation(tip) : TempStr(tip);
    TempStr accelStr = AppendAccelKeyToMenuStringTemp({}, cmdId);
    if (accelStr) {
        Str accel = accelStr.len > 1 ? Str(accelStr.s + 1, accelStr.len - 1) : accelStr;
        s = str::JoinTemp(s, fmt(" (%s)", accel));
    }
    return s;
}

void UpdateToolbarButtonsToolTipsForWindow(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return;
    }
    for (int i = 0; i < gLayoutButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gLayoutButtons[i];
        if (len(bi.toolTip) == 0 || bi.isText) {
            continue;
        }
        VirtCtrl* w = ToolbarItemAt(win, i);
        if (w) {
            w->SetTooltip(ToolbarTipTemp(bi.cmdId, bi.toolTip, true));
        }
    }
    for (int i = 0; i < kPdfAnnotationButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gPdfAnnotationButtons[i];
        if (len(bi.toolTip) == 0) {
            continue;
        }
        VirtCtrl* w = PdfAnnotationToolbarItemAt(win, i);
        if (w) {
            w->SetTooltip(ToolbarTipTemp(bi.cmdId, bi.toolTip, true));
        }
    }
}

static void SetToolbarButtonImageByIdx(MainWindow* win, int idx, const char* icon) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w) {
        return;
    }
    auto* ib = AsVirtIconButton(w);
    if (!ib) {
        return;
    }
    ToolbarVirt* tb = win->toolbarVirt;
    int sz = tb ? tb->iconSize : DpiScale(gSettings->toolbarSize);
    Pixmap* px = GetCachedPixmapForSvg(Str(icon), sz, sz, TbTextColor());
    Pixmap* pxOff = GetCachedPixmapForSvg(Str(icon), sz, sz, TbDisabledColor());
    if (ib->pixmap == px && ib->pixmapDisabled == pxOff) {
        return;
    }
    ib->pixmap = px;
    ib->pixmapDisabled = pxOff;
    ib->Invalidate();
}

static void SetToolbarButtonToolTipByIdx(MainWindow* win, int idx, int cmdId, Str s) {
    VirtCtrl* w = ToolbarItemAt(win, idx);
    if (!w) {
        return;
    }
    w->SetTooltip(ToolbarTipTemp(cmdId, s, false));
}

static void SetPdfAnnotationsToolbarVisible(MainWindow* win, bool visible) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || !tb->annotationRow) {
        return;
    }
    Visibility want = visible ? Visibility::Visible : Visibility::Collapse;
    if (tb->annotationRow->GetVisibility() == want) {
        return;
    }
    tb->annotationRow->SetVisibility(want);
    SetToolbarButtonCheckedState(win, CmdToggleEditPDF, visible);
    ToolbarSetHeight(win, tb->rowDy * (visible ? 2 : 1));
    tb->host->Relayout();
    tb->host->Invalidate(true);
    if (visible) {
        StartLoadingAnnotationsForUi(win->CurrentTab());
        RefreshAnnotFilterAnnotations(win);
    }
    ScheduleUiUpdate(win, kUiForceRelayout | kUiToolbarDirty);
}

// TODO: this is called too often
// TODO: also set checked state instead of calling SetToolbarButtonCheckedState() all over
void ToolbarUpdateStateForWindow(MainWindow* win, bool setButtonsVisibility) {
    int n = TotalButtonsCount();
    bool visibilityChanged = false;
    // One command ctx for the whole pass. Building it per button used to call
    // HasToc() (and page hit-testing) once per toolbar item during load.
    auto* ctx = NewBuildMenuCtx(win->CurrentTab(), Point{0, 0});
    AutoCall delCtx(DeleteBuildMenuCtx, ctx);
    for (int i = 0; i < n; i++) {
        auto& tb = GetToolbarButtonInfoByIdx(i);
        int cmdId = tb.cmdId;
        // cmdId 0 is a separator; GetCommandVisibility treats 0 as Hide, but
        // separators are always drawn. Which ones to drop is decided below,
        // by position, not by command availability.
        if (setButtonsVisibility && cmdId != WarningMsgId && cmdId != 0) {
            bool hide = !IsCmdAvailable(win, cmdId, ctx);
            visibilityChanged |= SetToolbarButtonHiddenByIdx(win, i, hide);
        }
        if (!HasToolbarButtonContent(tb)) {
            continue;
        }
        bool isEnabled = IsCmdEnabled(win, cmdId, ctx);
        SetToolbarButtonEnabledByIdx(win, i, isEnabled);

        if (cmdId == CmdReadAloud || cmdId == CmdPauseReadAloud) {
            bool speaking = TtsIsSpeaking();
            SetToolbarButtonImageByIdx(win, i, speaking ? gIconPauseSpeaking : gIconSpeak);
            // tooltip reflects what clicking the button will do
            Str tip = _TRA("Read Aloud");
            if (speaking) {
                tip = _TRA("Pause Reading");
            } else if (CanContinueReadAloud(win->CurrentTab())) {
                tip = _TRA("Continue Reading");
            }
            SetToolbarButtonToolTipByIdx(win, i, cmdId, tip);
        }
    }

    bool showPdfAnnotationsToolbar = win->pdfAnnotationsToolbarEnabled && ctx->isPdf && ctx->supportsAnnots;
    SetPdfAnnotationsToolbarVisible(win, showPdfAnnotationsToolbar);
    bool annotVisibilityChanged = false;
    for (int i = 0; i < kPdfAnnotationButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gPdfAnnotationButtons[i];
        if (!HasToolbarButtonContent(bi)) {
            continue;
        }
        CommandVisibility v = GetCommandVisibility(bi.cmdId, *ctx, CommandSurface::Toolbar);
        bool remove = CommandShouldRemove(v);
        annotVisibilityChanged |= SetPdfAnnotationButtonHiddenByIdx(win, i, remove);
        SetPdfAnnotationButtonEnabledByIdx(win, i, showPdfAnnotationsToolbar && !CommandShouldDisable(v) && !remove);
        if (bi.cmdId == CmdSaveAnnotations) {
            // name the file it writes to, like the annotation list's Save button
            WindowTab* tab = win->CurrentTab();
            TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
            Str tip = _TRA("Save changes to existing PDF");
            if (len(base) > 0) {
                tip = fmt(_TRA("Save changes to %s").s, base);
            }
            SetPdfAnnotationButtonToolTipByIdx(win, i, ToolbarTipTemp(bi.cmdId, tip, false));
        }
    }

    if (setButtonsVisibility) {
        // drop a separator that would sit next to another, or at either end
        // (Read Aloud is hidden by default, which would otherwise leave ||)
        bool prevVisibleNonSep = false;
        int lastSep = -1;
        for (int i = 0; i < n; i++) {
            const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
            VirtCtrl* w = ToolbarItemAt(win, i);
            if (!w) {
                continue;
            }
            if (bi.cmdId == 0) {
                bool hide = !prevVisibleNonSep;
                visibilityChanged |= SetToolbarButtonHiddenByIdx(win, i, hide);
                prevVisibleNonSep = false;
                if (!hide) {
                    lastSep = i;
                }
                continue;
            }
            // the page box counts as visible content: a separator right after
            // it is not a leading one (ToolbarCustomLayout = PageInfo | ...)
            if (w->GetVisibility() == Visibility::Visible) {
                prevVisibleNonSep = true;
                lastSep = -1;
            }
        }
        if (lastSep >= 0) {
            visibilityChanged |= SetToolbarButtonHiddenByIdx(win, lastSep, true);
        }
    }

    if (visibilityChanged || annotVisibilityChanged) {
        VirtHost* host = ToolbarHost(win);
        if (host) {
            if (host->vroot) {
                host->vroot->RequestLayout();
            }
            host->Relayout();
            host->Invalidate(true);
        }
    }

    // reposition the floating find bar over the search icon (and hide it if the
    // current document doesn't support find) when toolbar buttons change
    if (setButtonsVisibility) {
        UpdateToolbarFindText(win);
    }

    // update dirty (unsaved annotations) flag and tooltip on each tab
    if (win->tabsCtrl) {
        int nTabs = win->TabCount();
        for (int i = 0; i < nTabs; i++) {
            WindowTab* tab = win->GetTab(i);
            bool dirty = false;
            if (tab && tab->AsFixed()) {
                dirty = EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
            }
            // update tooltip before SetTabDirty (which rebuilds tooltips via LayoutTabs).
            // Must use MakeTabTooltipTemp (path+size); path-only overwrote size here.
            TabInfo* ti = win->tabsCtrl->GetTab(i);
            if (ti && tab && tab->filePath) {
                TempStr tooltip = MakeTabTooltipTemp(tab->filePath, dirty);
                str::ReplaceWithCopy(&ti->tooltip, tooltip);
            }
            win->tabsCtrl->SetTabDirty(i, dirty);
        }
    }
}

void SetToolbarButtonEnableState(MainWindow* win, int cmdId, bool isEnabled) {
    int originalCmdId = OriginalCommandId(cmdId);
    int n = TotalButtonsCount();
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& tbi = GetToolbarButtonInfoByIdx(i);
        if (OriginalCommandId(tbi.cmdId) == originalCmdId) {
            SetToolbarButtonEnabledByIdx(win, i, isEnabled);
        }
    }
    for (int i = 0; i < kPdfAnnotationButtonsCount; i++) {
        if (gPdfAnnotationButtons[i].cmdId == originalCmdId) {
            SetPdfAnnotationButtonEnabledByIdx(win, i, isEnabled);
        }
    }
}

static void SetPdfAnnotationsToolbarEnabled(MainWindow* win, bool enabled) {
    if (!win) {
        return;
    }
    AppCommandCtx ctx = NewAppCommandCtx(win);
    if (!ctx.isPdf || !ctx.supportsAnnots) {
        return;
    }
    if (win->pdfAnnotationsToolbarEnabled == enabled) {
        return;
    }
    if (win->pdfAnnotationsToolbarEnabled) {
        FinishInkAnnotationPlacement(win);
        // a half-placed line / shape / stamp is editing UI too: its notification
        // and cross cursor would outlive the mode it belongs to
        CancelAnnotationPlacement(win);
    }
    win->pdfAnnotationsToolbarEnabled = enabled;
    ToolbarUpdateStateForWindow(win, true);
    if (enabled) {
        RemoveNotificationsForGroup(win->hwndCanvas, kNotifAnnotation);
        UpdateAnnotationHoverOverlay(win);
    } else {
        // leaving the mode leaves no editing UI behind: without this the
        // selection marker and its resize handles stay painted on the page
        WindowTab* tab = win->CurrentTab();
        if (tab && tab->selectedAnnotation) {
            SetSelectedAnnotation(tab, nullptr);
        }
        HideAnnotationHoverOverlay(win);
        HideAnnotEditToolbar(win);
    }
    ScheduleRepaint(win, 0);
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
    int mode = ToolbarModeForWindow(win);
    return mode == kToolbarShow;
}

bool ShouldOverlayToolbar(MainWindow* win) {
    if (win->presentation || win->isQuickLook) {
        return false;
    }
    if (ToolbarModeForWindow(win) != kToolbarOverlay) {
        return false;
    }
    // don't float the overlay toolbar over the home / about page (only the
    // pinned "show" mode shows a toolbar there)
    if (win->IsCurrentTabAbout()) {
        return false;
    }
    return true;
}

// natural width of the toolbar content (buttons + page box); the find bar
// floats separately so the page-total label is the rightmost element
static int ToolbarNaturalWidth(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    VirtHost* host = ToolbarHost(win);
    if (!host || !host->layout) {
        return 0;
    }
    int dx = host->layout->MinIntrinsicWidth(tb->rowDy);
    if (dx <= 0) {
        dx = tb->rowDy * 8;
    }
    return dx + DpiScale(12);
}

// when the overlay toolbar sits at the bottom, lift it above the horizontal
// scrollbar so it doesn't cover it. The height is reserved even when the
// scrollbar isn't currently visible, so the toolbar's position is stable.
static int OverlayToolbarBottomScrollbarOffset() {
    if (ScrollbarsAreHidden()) {
        return 0;
    }
    if (ScrollbarsUseOverlay()) {
        // smart/overlay: the thick overlay scrollbar height (see OverlayScrollbarCreate)
        return DpiScale(16);
    }
    return UiHScrollbarDy();
}

// rectangle (frame-client coords) the overlay toolbar occupies when shown
static Rect OverlayToolbarRect(MainWindow* win) {
    Rect canvas = ToolbarCanvasRectInFrame(win);
    int natW = ToolbarNaturalWidth(win);
    if (natW <= 0 || natW > canvas.dx) {
        natW = canvas.dx;
    }
    int h = ToolbarHost(win)->ScreenRect().dy;
    int x = canvas.x + ((canvas.dx - natW) / 2);
    int y = canvas.y;
    if (ToolbarAtBottom()) {
        y = canvas.y + canvas.dy - h - OverlayToolbarBottomScrollbarOffset();
    }
    return {x, y, natW, h};
}

// position/show the floating overlay toolbar; called on relayout and mouse move
void PositionOverlayToolbar(MainWindow* win) {
    VirtHost* host = ToolbarHost(win);
    if (!win->isToolbarOverlay || !host) {
        return;
    }
    Rect r = OverlayToolbarRect(win);
    host->SetPos(r, win->toolbarOverlayShown);
    if (!win->toolbarOverlayShown) {
        ToolbarRepaintUncovered(win, r);
    }
}

// whether the cursor is currently in the reveal band or over the toolbar
static bool OverlayToolbarShouldShowForCursor(MainWindow* win) {
    Point pt = UiCursorScreenPos();
    Point ptFrame = ToolbarScreenToFrame(win, pt);

    Rect tb = OverlayToolbarRect(win);
    // reveal band: spans the full canvas width so the toolbar also appears when
    // the mouse is to the left or right of it, and extends a bit past the
    // toolbar (toward the page) so it shows before the cursor reaches it
    Rect canvas = ToolbarCanvasRectInFrame(win);
    int my = DpiScale(16);
    int bandY = ToolbarAtBottom() ? (tb.y - my) : tb.y;
    Rect band(canvas.x, bandY, canvas.dx, tb.dy + my);
    bool inBand = band.Contains(Point(ptFrame.x, ptFrame.y));

    // also keep shown while the cursor is over the toolbar window itself
    return inBand || ToolbarHost(win)->ContainsScreenPoint(pt);
}

// the overlay toolbar must not vanish while it owns the keyboard focus (e.g.
// the user is typing a page number into the page box after Ctrl+G)
static bool OverlayToolbarHasFocus(MainWindow* win) {
    VirtHost* host = ToolbarHost(win);
    return host && host->HasFocus();
}

static void CancelOverlayHide(MainWindow* win) {
    if (win->toolbarOverlayHidePending) {
        ToolbarHost(win)->KillTimer(kHideOverlayToolbarTimerId);
        win->toolbarOverlayHidePending = false;
    }
}

static void ScheduleOverlayHide(MainWindow* win) {
    if (win->toolbarOverlayHidePending) {
        return; // already scheduled; don't keep pushing it out on every move
    }
    win->toolbarOverlayHidePending = true;
    ToolbarHost(win)->SetTimer(kHideOverlayToolbarTimerId, kDelayToolbarHide);
}

static void SetOverlayShown(MainWindow* win, bool shown) {
    if (shown == win->toolbarOverlayShown) {
        return;
    }
    win->toolbarOverlayShown = shown;
    PositionOverlayToolbar(win);
}

// re-evaluate overlay toolbar visibility based on the cursor's screen position
void UpdateOverlayToolbarForMouse(MainWindow* win) {
    if (!win->isToolbarOverlay || !ToolbarHost(win)) {
        return;
    }
    bool show = OverlayToolbarShouldShowForCursor(win) || OverlayToolbarHasFocus(win);
    if (show) {
        CancelOverlayHide(win);
        SetOverlayShown(win, true);
    } else if (win->toolbarOverlayShown) {
        // don't hide immediately; give the user kDelayToolbarHide to come back
        ScheduleOverlayHide(win);
    }
}

// reveal the overlay toolbar right now, without waiting for the cursor to enter
// the reveal band. Used by commands that drive the toolbar from the keyboard
// (Ctrl+G): the toolbar stays up while it has the focus and auto-hides once the
// focus and the cursor are away from it.
void RevealOverlayToolbar(MainWindow* win) {
    if (!win->isToolbarOverlay || !ToolbarHost(win)) {
        return;
    }
    CancelOverlayHide(win);
    SetOverlayShown(win, true);
}

// the delayed-hide timer fired on the toolbar's own host
static void OnHoverDropdownTimer(MainWindow* win, int timerId);

static void OnToolbarTimer(MainWindow* win, int timerId) {
    if (timerId == kOpenHoverDropdownTimerId || timerId == kCloseHoverDropdownTimerId) {
        OnHoverDropdownTimer(win, timerId);
        return;
    }
    if (timerId != kHideOverlayToolbarTimerId) {
        return;
    }
    win->toolbarOverlayHidePending = false;
    ToolbarHost(win)->KillTimer(kHideOverlayToolbarTimerId);
    if (!win->isToolbarOverlay) {
        return;
    }
    // if the cursor came back near the top while the timer was pending, keep
    // the toolbar shown; otherwise hide it now
    if (OverlayToolbarShouldShowForCursor(win) || OverlayToolbarHasFocus(win)) {
        SetOverlayShown(win, true);
    } else {
        SetOverlayShown(win, false);
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
    if (!overlay) {
        CancelOverlayHide(win);
        win->toolbarOverlayShown = false;
    }
    if (enteredOverlay) {
        // reveal immediately on entering overlay mode (e.g. via F8) so the
        // change is visible; it auto-hides after kDelayToolbarHide
        win->toolbarOverlayShown = true;
    }
    if (!show && !overlay) {
        // Move the focus out of the toolbar
        if ((win->findEdit && win->findEdit->IsFocused()) || (win->pageEdit && win->pageEdit->IsFocused()) ||
            (win->chapterEdit && win->chapterEdit->IsFocused())) {
            ToolbarFocusFrame(win);
        }
        if (win->hwndToolbar) {
            ShowWindow(win->hwndToolbar, SW_HIDE);
        }
    }
    // overlay <-> hide does not flip isToolbarVisible, so RelayoutFrame would
    // skip without this (sidebar stays at the overlay y, toolbar HWND stays)
    ScheduleUiUpdate(win, kUiForceRelayout | kUiRelayout);
    if (enteredOverlay) {
        ScheduleOverlayHide(win);
    }
}

void UpdateFindbox(MainWindow* win) {
    VirtHost* host = ToolbarHost(win);
    if (host) {
        host->Invalidate(true);
        if (ToolbarFrameIsVisible(win)) {
            host->Repaint();
        }
    }
    ToolbarUpdateFindEditCursor(win);
}

// the find UI is now a floating Chrome-style bar (see FindBar.cpp). When the
// toolbar moves/resizes we keep the bar centered over the search icon.
void UpdateToolbarFindText(MainWindow* win) {
    FindBarReposition(win);
}

static void UpdateZoomHoverDropdown(MainWindow* win);

void UpdateToolbarState(MainWindow* win) {
    if (!win->IsDocLoaded()) {
        return;
    }
    // the zoom buttons' strip may be up: the zoom just moved under it
    UpdateZoomHoverDropdown(win);
    DisplayMode dm = win->ctrl->GetDisplayMode();
    float zoomVirtual = win->ctrl->GetZoomVirtual();
    {
        bool isChecked = dm == DisplayMode::Continuous && zoomVirtual == kZoomFitWidth;
        SetToolbarButtonCheckedState(win, CmdZoomFitWidthAndContinuous, isChecked);
    }
    {
        bool isChecked = dm == DisplayMode::SinglePage && zoomVirtual == kZoomFitPage;
        SetToolbarButtonCheckedState(win, CmdZoomFitPageAndSinglePage, isChecked);
        if (!isChecked) {
            win->CurrentTab()->prevZoomVirtual = kInvalidZoom;
        }
    }
}

void UpdateToolbarPageText(MainWindow* win, int pageCount, bool updateOnly) {
    VirtHost* host = ToolbarHost(win);
    if (!host) {
        return;
    }
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb->pageTotal) {
        return;
    }

    bool hasChapters = win->ctrl && win->ctrl->HasChapters();
    if (tb->pageLabel) {
        tb->pageLabel->SetText(hasChapters ? _TRA("Chapter:") : _TRA("Page:"));
    }
    Visibility chapterVis = hasChapters ? Visibility::Visible : Visibility::Collapse;
    bool chapterVisChanged = false;
    if (win->chapterEdit && win->chapterEdit->GetVisibility() != chapterVis) {
        win->chapterEdit->SetVisibility(chapterVis);
        chapterVisChanged = true;
    }
    if (tb->chapterTotal && tb->chapterTotal->GetVisibility() != chapterVis) {
        tb->chapterTotal->SetVisibility(chapterVis);
        chapterVisChanged = true;
    }
    if (tb->pageLabel2 && tb->pageLabel2->GetVisibility() != chapterVis) {
        tb->pageLabel2->SetVisibility(chapterVis);
        chapterVisChanged = true;
    }
    if (chapterVisChanged) {
        host->Relayout();
    }

    TempStr txt;
    if (-1 == pageCount || !pageCount) {
        txt = StrL(" ");
    } else if (hasChapters) {
        int chapter = win->ctrl->CurrentLocation().chapter;
        txt = fmt(" / %d", win->ctrl->ChapterPageCount(chapter));
        if (tb->chapterTotal) {
            tb->chapterTotal->SetText(fmt(" / %d", win->ctrl->ChapterCount()));
        }
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
    if (updateOnly && tb->pageTotal->s && txt && str::Eq(tb->pageTotal->s, txt)) {
        return;
    }
    tb->pageTotal->SetText(txt);
    host->Relayout();
    host->Invalidate(true);
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
        Str desc = SeqStrByIndex(gCommandDescriptions, origId);
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

static void ApplyToolbarItemColors(VirtCtrl* w) {
    Color hover = TbHoverColor();
    Color sel = TbSelectedColor();
    if (auto* ib = AsVirtIconButton(w)) {
        ib->SetColor(kColIconBtnBgHover, hover);
        ib->SetColor(kColIconBtnBgSelected, sel);
        ib->SetColor(kColIconBtnChevron, TbTextColor());
        ib->SetColor(kColIconBtnChevronDisabled, TbDisabledColor());
        return;
    }
    if (auto* b = AsVirtButton(w)) {
        // a toolbar button is a label that highlights on hover, not a box
        b->SetColor(kColBtnBg, kColorTransparent);
        b->SetColor(kColBtnBorder, kColorTransparent);
        b->SetColor(kColBtnBgHover, hover);
        b->SetColor(kColBtnText, TbTextColor());
        b->SetColor(kColBtnTextDisabled, TbDisabledColor());
        return;
    }
    if (auto* t = AsVirtText(w)) {
        t->SetColor(kColText, TbTextColor());
        return;
    }
    if (auto* line = AsVirtLine(w)) {
        line->SetColor(kColLineFg, TbEdgeColor());
    }
}

static void RefreshToolbarIcons(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb) {
        return;
    }
    int sz = tb->iconSize;
    Color fg = TbTextColor();
    Color dis = TbDisabledColor();
    for (int i = 0; i < len(tb->items); i++) {
        VirtCtrl* w = tb->items[i];
        ApplyToolbarItemColors(w);
        auto* ib = AsVirtIconButton(w);
        if (!ib) {
            continue;
        }
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        if (!HasToolbarButtonContent(bi)) {
            continue;
        }
        Str svg = bi.svgIcon ? bi.svgIcon : Str(bi.icon);
        ib->pixmap = GetCachedPixmapForSvg(svg, sz, sz, fg, TbBgColor());
        ib->pixmapDisabled = GetCachedPixmapForSvg(svg, sz, sz, dis, TbBgColor());
    }
    for (int i = 0; i < len(tb->annotationItems); i++) {
        VirtCtrl* w = tb->annotationItems[i];
        ApplyToolbarItemColors(w);
        auto* ib = AsVirtIconButton(w);
        if (!ib) {
            continue;
        }
        const ToolbarButtonInfo& bi = gPdfAnnotationButtons[i];
        if (!HasToolbarButtonContent(bi)) {
            continue;
        }
        ib->pixmap = GetCachedPixmapForSvg(Str(bi.icon), sz, sz, fg, TbBgColor());
        ib->pixmapDisabled = GetCachedPixmapForSvg(Str(bi.icon), sz, sz, dis, TbBgColor());
    }
    if (tb->pageLabel) {
        tb->pageLabel->SetColor(kColText, TbTextColor());
    }
    if (tb->pageLabel2) {
        tb->pageLabel2->SetColor(kColText, TbTextColor());
    }
    if (tb->pageTotal) {
        tb->pageTotal->SetColor(kColText, TbTextColor());
    }
    if (win->pageEdit) {
        win->pageEdit->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    }
    if (tb->chapterTotal) {
        tb->chapterTotal->SetColor(kColText, TbTextColor());
    }
    if (win->chapterEdit) {
        win->chapterEdit->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    }
}

void UpdateToolbarAfterThemeChange(MainWindow* win) {
    RefreshToolbarIcons(win);
    VirtHost* host = ToolbarHost(win);
    if (host) {
        host->bgColor = TbBgColor();
        host->Invalidate(true);
    }
    UpdateAnnotFilterToolbar(win);
}

// bounds of a button in the toolbar's client coords, empty if it has none
static VirtCtrl* ToolbarItemForCmd(MainWindow* win, int cmdId) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return nullptr;
    }
    for (VirtCtrl* w : tb->items) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            return w;
        }
    }
    for (VirtCtrl* w : tb->annotationItems) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            return w;
        }
    }
    return nullptr;
}

static Rect ToolbarButtonRect(MainWindow* win, int cmdId) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return {};
    }
    for (VirtCtrl* w : tb->items) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            return w->BoundsInWindow();
        }
    }
    for (VirtCtrl* w : tb->annotationItems) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            return w->BoundsInWindow();
        }
    }
    return {};
}

// screen-coordinates rect of a toolbar button, used to position the FindBar.
// returns an empty rect when the toolbar isn't visible (e.g. fullscreen /
// presentation) so the caller can fall back to a different anchor.
Rect GetToolbarButtonScreenRect(MainWindow* win, int cmdId) {
    VirtHost* host = ToolbarHost(win);
    if (!host || !host->IsVisible()) {
        return {};
    }
    Rect r = ToolbarButtonRect(win, cmdId);
    if (r.IsEmpty()) {
        return {};
    }
    return host->ToScreen(r);
}

// Dump of the toolbar's buttons for -dbg-control tests (tests/issue-5869.ts).
// One line per button: its command id, its rect and the string the toolbar
// shows as its tooltip. Also reports how many tools the toolbar's tooltip
// control ended up with: the toolbar registers one tool per button keyed by
// command id, so duplicate command ids silently collapse into one tooltip.
static TempStr HoverDropdownStateTemp(MainWindow* win);

TempStr ToolbarButtonsResultTemp(int* exitCodeOut) {
    str::Builder out;
    MainWindow* win = len(gWindows) == 0 ? nullptr : gWindows[0];
    if (!win || !ToolbarHost(win)) {
        *exitCodeOut = 1;
        out.Append(StrL("ERROR no-toolbar\n"));
        return ToStrTemp(out);
    }
    ToolbarVirt* tb = win->toolbarVirt;
    int n = tb ? len(tb->items) : 0;
    int nTools = 0;
    for (int i = 0; i < n; i++) {
        if (tb->items[i] && tb->items[i]->tooltip) {
            nTools++;
        }
    }
    out.Append(fmt("buttons=%d tooltipTools=%d\n", n, nTools));
    int toolIdx = 0;
    for (int i = 0; i < n; i++) {
        VirtCtrl* w = tb->items[i];
        Rect r = w ? w->BoundsInWindow() : Rect{};
        bool hidden = !w || w->GetVisibility() != Visibility::Visible;
        // Match the old Win32 dump: TBIF_TEXT. Built-in buttons store the
        // tooltip (with accelerator); custom ones stored the raw name.
        Str text{};
        if (auto* b = AsVirtButton(w)) {
            text = b->s;
        } else if (i >= gLayoutButtonsCount) {
            text = GetToolbarButtonInfoByIdx(i).toolTip;
        } else if (w && w->tooltip) {
            text = w->tooltip;
        }
        out.Append(fmt("idx=%d cmd=%d hidden=%d rect=%d,%d,%d,%d text=%s\n", i, w ? w->id : 0, hidden ? 1 : 0, r.x, r.y,
                       r.x + r.dx, r.y + r.dy, text));
        if (w && w->tooltip) {
            out.Append(fmt("tool=%d uid=%d rect=%d,%d,%d,%d\n", toolIdx, w->id, r.x, r.y, r.x + r.dx, r.y + r.dy));
            toolIdx++;
        }
    }
    int nAnnotations = len(tb->annotationItems);
    bool annotationsVisible = tb->annotationRow && tb->annotationRow->GetVisibility() == Visibility::Visible;
    out.Append(fmt("annotationButtons=%d visible=%d\n", nAnnotations, annotationsVisible ? 1 : 0));
    for (int i = 0; i < nAnnotations; i++) {
        VirtCtrl* w = tb->annotationItems[i];
        Rect r = w ? w->BoundsInWindow() : Rect{};
        bool hidden = !annotationsVisible || !w || w->GetVisibility() != Visibility::Visible;
        const ToolbarButtonInfo& bi = gPdfAnnotationButtons[i];
        Str tip = w ? w->tooltip : Str{};
        out.Append(fmt("annotation-idx=%d cmd=%d hidden=%d enabled=%d rect=%d,%d,%d,%d text=%s tip=%s\n", i,
                       w ? w->id : 0, hidden ? 1 : 0, w && w->IsEnabled() ? 1 : 0, r.x, r.y, r.x + r.dx, r.y + r.dy,
                       bi.toolTip, tip));
    }
    out.Append(AnnotFilterToolbarStateTemp(win));
    out.Append(HoverDropdownStateTemp(win));
    *exitCodeOut = 0;
    return ToStrTemp(out);
}

// A drop-down menu is modal: the click that dismisses it is delivered to the
// toolbar after the menu closes, and when it lands on the split button that
// opened the menu it would open it right back up. So the button ignores a click
// that arrives on the heels of its menu closing.
static u64 gToolbarDropdownClosedAt = 0;

static bool ToolbarDropdownJustClosed() {
    return GetTickCount64() - gToolbarDropdownClosedAt < 200;
}

// called when a toolbar drop-down menu was dismissed
void ToolbarNoteDropdownClosed() {
    gToolbarDropdownClosedAt = GetTickCount64();
}

static void OnToolbarButtonClicked(MainWindow* win, VirtMouseEvent* ev) {
    VirtCtrl* w = ev->target;
    if (!w || !win || !w->IsEnabled()) {
        return;
    }
    int cmdId = w->id;
    if (cmdId == PageInfoId || cmdId == 0) {
        return;
    }
    if (ToolbarDropdownJustClosed() && (cmdId == CmdReadAloud || cmdId == CmdPauseReadAloud)) {
        ev->didHandle = true;
        return;
    }
    if (auto* ib = AsVirtIconButton(w)) {
        if (ib->hasDropdown) {
            int dropDx = ib->DropdownDx();
            if (dropDx > 0 && ev->pt.x >= w->bounds.dx - dropDx) {
                ShowTtsVoiceMenu(win, GetToolbarButtonScreenRect(win, cmdId));
                ev->didHandle = true;
                return;
            }
        }
    }
    if (cmdId == CmdSaveAnnotations) {
        // the hover menu's rows end the session; they no longer apply
        HideToolbarHoverDropdown(win);
        if (ToolbarVirt* tb = win->toolbarVirt) {
            tb->hoverPendingCmdId = cmdId;
        }
    }
    ToolbarPostCommand(win, cmdId);
    ev->didHandle = true;
}

//--- hover drop-down

// A row of NewToolbarHoverMenu(): an icon on the left, text on the right, and a
// background that lights up under the mouse, like a menu item.
constexpr int kHoverRowPadY = 6;
constexpr int kHoverRowPadX = 10;
constexpr int kHoverRowIconGapX = 8;
// between the label and the shortcut that sits at the right edge, as in a menu
constexpr int kHoverRowShortcutGapX = 24;
constexpr int kHoverMenuBorder = 1;
// around a label in the single-row strip; less than a menu row's, it is a row
// of them and the gaps add up
constexpr int kHoverCellPadX = 8;
// the mouse crosses a seam going from the button to the drop-down; don't close
// on the frame where it is over neither
constexpr int kCloseHoverDropdownDelayMs = 150;

struct ToolbarHoverRow : VirtCtrl {
    Pixmap* pixmap = nullptr; // not owned, from GetCachedPixmapForSvg()
    Str text;                 // owned
    Str shortcut;             // owned; empty when the command has no key
    PlatformFont* font = nullptr;
    int iconSize = 0;

    ToolbarHoverRow() = default;
    ~ToolbarHoverRow() override {
        str::Free(text);
        str::Free(shortcut);
    }

    int ShortcutDx() {
        if (len(shortcut) == 0) {
            return 0;
        }
        return PlatformFontMeasureText(font, shortcut).dx + DpiScale(kHoverRowShortcutGapX);
    }

    Size GetIdealSize() override {
        Size ts = PlatformFontMeasureText(font, text);
        int dx = (2 * DpiScale(kHoverRowPadX)) + iconSize + DpiScale(kHoverRowIconGapX) + ts.dx + ShortcutDx();
        int dy = std::max(ts.dy, iconSize) + (2 * DpiScale(kHoverRowPadY));
        return {dx, dy};
    }

    void Paint(VirtPaintCtx& ctx) override {
        bool enabled = IsEnabled();
        Rect r = ctx.bounds;
        if (enabled && HasFlag(vwfHovered)) {
            ctx.gfx->FillRect(r, TbHoverColor());
        }
        int x = r.x + DpiScale(kHoverRowPadX);
        if (pixmap) {
            int y = r.y + ((r.dy - pixmap->height) / 2);
            ctx.gfx->DrawPixmap(pixmap, {x, y, pixmap->width, pixmap->height});
        }
        x += iconSize + DpiScale(kHoverRowIconGapX);
        int right = r.Right() - DpiScale(kHoverRowPadX);
        Color col = enabled ? TbTextColor() : TbDisabledColor();
        if (shortcut) {
            // right-aligned and dimmer, the way a menu shows its accelerator
            Rect sr{x, r.y, right - x, r.dy};
            ctx.gfx->DrawText(shortcut, sr, gfxTextRight | gfxTextVCenter, font, TbDisabledColor());
            right -= ShortcutDx();
        }
        Rect tr{x, r.y, right - x, r.dy};
        ctx.gfx->DrawText(text, tr, gfxTextVCenter | gfxTextEllipsis, font, col);
    }

    void OnMouseEnter() { Invalidate(); }
    void OnMouseLeave() { Invalidate(); }
};

// A cell of NewToolbarHoverStrip(): a label in a row of them, no icon. The one
// in use is boxed rather than ticked; a tick per cell would double the width of
// a strip whose whole point is to be compact.
struct ToolbarHoverCell : VirtCtrl {
    Str text; // owned
    PlatformFont* font = nullptr;
    bool isCurrent = false;

    ToolbarHoverCell() = default;
    ~ToolbarHoverCell() override { str::Free(text); }

    Size GetIdealSize() override {
        Size ts = PlatformFontMeasureText(font, text);
        return {ts.dx + (2 * DpiScale(kHoverCellPadX)), ts.dy + (2 * DpiScale(kHoverRowPadY))};
    }

    void Paint(VirtPaintCtx& ctx) override {
        bool enabled = IsEnabled();
        Rect r = ctx.bounds;
        if (enabled && HasFlag(vwfHovered)) {
            ctx.gfx->FillRect(r, TbHoverColor());
        }
        if (isCurrent) {
            ctx.gfx->DrawRect(r, TbTextColor(), DpiScale(1));
        }
        Color col = enabled ? TbTextColor() : TbDisabledColor();
        ctx.gfx->DrawText(text, r, gfxTextCenter | gfxTextVCenter, font, col);
    }

    void OnMouseEnter() { Invalidate(); }
    void OnMouseLeave() { Invalidate(); }
};

static void PostedHideHoverDropdown(MainWindow* win) {
    if (IsMainWindowValidAndNotClosing(win)) {
        HideToolbarHoverDropdown(win);
    }
}

static void OnHoverRowClicked(MainWindow* win, VirtMouseEvent* ev) {
    VirtCtrl* w = ev ? ev->target : nullptr;
    if (!w || !w->IsEnabled()) {
        return;
    }
    int cmdId = w->id;
    // the click is being handled by the drop-down's own window, so it can only
    // be torn down once that returns
    uitask::Post(MkFunc0(PostedHideHoverDropdown, win), "HideToolbarHoverDropdown");
    ToolbarPostCommand(win, cmdId);
}

// Remember a row/cell for HoverDropdownStateTemp(). `text` has to be the ctrl's
// own copy: what the caller built the item from is often temp-allocated and
// gone by the time the dump is asked for.
static void RecordHoverItem(ToolbarVirt* tb, VirtCtrl* w, Str text, const ToolbarHoverMenuItem& it) {
    ToolbarHoverItemState st;
    st.ctrl = w;
    st.text = text;
    st.cmdId = it.cmdId;
    st.isCurrent = it.isCurrent;
    VecAppend(tb->hoverItems, st);
}

ILayout* NewToolbarHoverMenu(MainWindow* win, const Vec<ToolbarHoverMenuItem>& items) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return nullptr;
    }
    int iconSize = tb->iconSize;
    Color fg = TbTextColor();
    Color dis = TbDisabledColor();
    Color bg = TbBgColor();
    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    for (const ToolbarHoverMenuItem& it : items) {
        auto* row = new ToolbarHoverRow();
        row->id = it.cmdId;
        row->font = tb->platformFont;
        row->iconSize = iconSize;
        str::ReplaceWithCopy(&row->text, it.text);
        str::ReplaceWithCopy(&row->shortcut, ShortcutsForCmdTemp(it.cmdId, 1));
        if (it.svgIcon) {
            row->pixmap = GetCachedPixmapForSvg(it.svgIcon, iconSize, iconSize, it.enabled ? fg : dis, bg);
        }
        row->SetIsEnabled(it.enabled);
        row->onClick = MkFunc1(OnHoverRowClicked, win);
        vbox->AddChild(row);
        RecordHoverItem(tb, row, row->text, it);
    }
    int b = DpiScale(kHoverMenuBorder);
    return new Padding(vbox, Insets{b, b, b, b});
}

// The widest row of the pyramid: the smallest w with 1+2+...+w >= n, so the
// rows w, w-1, ... w-k hold every item with only the last one part-full. 26
// items give 7, 6, 5, 4, 3 and a last row of 1.
static int HoverPyramidTopRow(int n) {
    int w = 1;
    while (((w * (w + 1)) / 2) < n) {
        w++;
    }
    return w;
}

// one row of the pyramid: items [a0, a1) from below the middle and [b0, b1)
// from above it, so the row still runs smallest to largest
static void AddHoverPyramidRow(VBox* vbox, const Vec<VirtCtrl*>& cells, int a0, int a1, int b0, int b1) {
    auto* hbox = new HBox();
    hbox->alignCross = CrossAxisAlign::Stretch;
    for (int i = a0; i < a1; i++) {
        hbox->AddChild(cells[i]);
    }
    for (int i = b0; i < b1; i++) {
        hbox->AddChild(cells[i]);
    }
    vbox->AddChild(hbox);
}

ILayout* NewToolbarHoverStrip(MainWindow* win, const Vec<ToolbarHoverMenuItem>& items) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return nullptr;
    }
    // a cell per item, made in the order they came in so that hoverItems (and
    // the -dbg-control dump built from it) stays in that order whatever row a
    // cell ends up in
    Vec<VirtCtrl*> cells;
    for (const ToolbarHoverMenuItem& it : items) {
        auto* cell = new ToolbarHoverCell();
        cell->id = it.cmdId;
        cell->font = tb->platformFont;
        cell->isCurrent = it.isCurrent;
        str::ReplaceWithCopy(&cell->text, it.text);
        cell->SetIsEnabled(it.enabled);
        cell->onClick = MkFunc1(OnHoverRowClicked, win);
        VecAppend(cells, (VirtCtrl*)cell);
        RecordHoverItem(tb, cell, cell->text, it);
        tb->hoverItems[len(tb->hoverItems) - 1].isStripCell = true;
    }

    // A pyramid rather than one long row: the middle of the list goes in the
    // top row, next to the button, and each row below it holds what surrounds
    // the middle, down to the two extremes. Every value is then a short move
    // down and across instead of a run along a row as wide as the screen.
    int base = len(tb->hoverItems) - len(cells);
    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::CrossCenter;
    int b = DpiScale(kHoverMenuBorder);
    int n = len(cells);
    if (n == 0) {
        return new Padding(vbox, Insets{b, b, b, b});
    }
    int top = HoverPyramidTopRow(n);
    // items [lo, hi) are the ones already placed
    int lo = (n - top) / 2;
    int hi = lo + top;
    // the top row splits at its own middle, and everything past that point is
    // on the larger side in every row: the rows below it take from the two
    // sides of the middle in order
    for (int i = lo + (top / 2); i < n; i++) {
        tb->hoverItems[base + i].isRightHalf = true;
    }
    AddHoverPyramidRow(vbox, cells, lo, hi, 0, 0);
    int rowLen = top - 1;
    while (lo > 0 || hi < n) {
        // as evenly as the two sides allow: the shorter side runs out first
        // and the other takes what is left of the row
        int nLeft = std::min(lo, (rowLen + 1) / 2);
        int nRight = std::min(n - hi, rowLen - nLeft);
        nLeft = std::min(lo, rowLen - nRight);
        AddHoverPyramidRow(vbox, cells, lo - nLeft, lo, hi, hi + nRight);
        lo -= nLeft;
        hi += nRight;
        rowLen = std::max(rowLen - 1, 1);
    }
    return new Padding(vbox, Insets{b, b, b, b});
}

// The rows/cells of the drop-down that is up, in screen coordinates, for
// -dbg-control tests (tests/toolbar-hover-dropdown.ts).
static TempStr HoverDropdownStateTemp(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    VirtHost* host = tb ? tb->hoverHost : nullptr;
    if (!host) {
        return StrL("dropdown cmd=0 items=0\n");
    }
    str::Builder out;
    int n = len(tb->hoverItems);
    out.Append(fmt("dropdown cmd=%d items=%d\n", tb->hoverCmdId, n));
    for (int i = 0; i < n; i++) {
        ToolbarHoverItemState& st = tb->hoverItems[i];
        Rect r = st.ctrl ? host->ToScreen(st.ctrl->BoundsInWindow()) : Rect{};
        out.Append(fmt("dropdown-item idx=%d cmd=%d current=%d rect=%d,%d,%d,%d text=%s\n", i, st.cmdId,
                       st.isCurrent ? 1 : 0, r.x, r.y, r.x + r.dx, r.y + r.dy, st.text));
    }
    return ToStrTemp(out);
}

// The toolbar sees no mouse moves once the cursor is inside the drop-down, so
// the drop-down has to say when the cursor leaves it.
static void OnHoverDropdownMouseLeave(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (tb && tb->host && tb->hoverCmdId != 0) {
        tb->host->SetTimer(kCloseHoverDropdownTimerId, kCloseHoverDropdownDelayMs);
    }
}

static void OnHoverDropdownMouseMove(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (tb && tb->host && tb->hoverCmdId != 0) {
        tb->host->KillTimer(kCloseHoverDropdownTimerId);
    }
}

// The pyramid's right half - the values above the middle - gets its own
// ground, so which way is bigger can be seen rather than read. The rows are
// staggered, so the two halves meet along a staircase, and each band runs to
// the right edge, covering the empty space beside a short row as well.
static void PaintHoverDropdownRightHalf(MainWindow* win, VirtHostPaintEvent* ev) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return;
    }
    Vec<ToolbarHoverItemState>& items = tb->hoverItems;
    int n = len(items);
    constexpr int kMaxRows = 32;
    int rowTop[kMaxRows];
    int rowBottom[kMaxRows];
    int rowSplit[kMaxRows];
    int nRows = 0;
    int prevTop = INT_MIN;
    while (nRows < kMaxRows) {
        // the topmost row below the last one found
        int y = INT_MAX;
        for (int i = 0; i < n; i++) {
            // only a strip has halves; a menu's rows are all one ground
            VirtCtrl* c = items[i].isStripCell ? items[i].ctrl : nullptr;
            if (c) {
                int cy = c->BoundsInWindow().y;
                if (cy > prevTop && cy < y) {
                    y = cy;
                }
            }
        }
        if (y == INT_MAX) {
            break;
        }
        int bottom = y;
        int right = INT_MIN;
        int split = INT_MAX;
        for (int i = 0; i < n; i++) {
            VirtCtrl* c = items[i].ctrl;
            if (!c) {
                continue;
            }
            Rect r = c->BoundsInWindow();
            if (r.y != y) {
                continue;
            }
            bottom = std::max(bottom, r.y + r.dy);
            right = std::max(right, r.x + r.dx);
            if (items[i].isRightHalf) {
                split = std::min(split, r.x);
            }
        }
        if (split == INT_MAX) {
            // nothing of the larger side in this row: only the space past its
            // end is on that side
            split = right;
        }
        rowTop[nRows] = y;
        rowBottom[nRows] = bottom;
        rowSplit[nRows] = split;
        nRows++;
        prevTop = y;
    }
    Rect cr = ev->clientRect;
    Color col = TbSubtleBgColor();
    for (int i = 0; i < nRows; i++) {
        // the first and last bands take in the border, so no strip of the
        // other ground is left above or below them
        int y = (i == 0) ? cr.y : rowTop[i];
        int bottom = (i == nRows - 1) ? cr.y + cr.dy : rowBottom[i];
        int x = rowSplit[i];
        ev->gfx->FillRect(Rect{x, y, (cr.x + cr.dx) - x, bottom - y}, col);
    }
}

static void PaintHoverDropdownBg(MainWindow* win, VirtHostPaintEvent* ev) {
    ev->gfx->FillRect(ev->clientRect, TbBgColor());
    PaintHoverDropdownRightHalf(win, ev);
    ev->gfx->DrawRect(ev->clientRect, ThemeEdgeColor(), DpiScale(kHoverMenuBorder));
}

// The button a drop-down is up for goes without its tooltip: the bubble would
// sit on top of the drop-down, and WM_SETCURSOR would keep bringing it back.
static void TakeHoverButtonTooltip(MainWindow* win, int cmdId) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (VirtCtrl* btn = ToolbarItemForCmd(win, cmdId)) {
        str::ReplaceWithCopy(&tb->hoverSavedTip, btn->tooltip);
        btn->SetTooltip({});
    }
}

static void GiveHoverButtonTooltipBack(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (tb->hoverCmdId == 0) {
        return;
    }
    if (VirtCtrl* btn = ToolbarItemForCmd(win, tb->hoverCmdId)) {
        btn->SetTooltip(tb->hoverSavedTip);
    }
    str::Free(tb->hoverSavedTip);
    tb->hoverSavedTip = {};
}

void HideToolbarHoverDropdown(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return;
    }
    VecReset(tb->hoverItems);
    GiveHoverButtonTooltipBack(win);
    tb->hoverPendingCmdId = 0;
    tb->hoverCmdId = 0;
    if (tb->host) {
        tb->host->KillTimer(kOpenHoverDropdownTimerId);
        tb->host->KillTimer(kCloseHoverDropdownTimerId);
    }
    if (tb->hoverHost) {
        VirtHost* h = tb->hoverHost;
        tb->hoverHost = nullptr;
        delete h;
    }
}

// Geometry, not WindowFromPoint(): the toolbar only gets a mouse move while
// the cursor is over it, so who is on top does not come into it.
static bool HostHasPoint(VirtHost* host, Point pt) {
    return host && host->IsVisible() && host->ScreenRect().Contains(pt);
}

bool ToolbarHoverDropdownContainsScreenPoint(MainWindow* win, Point pt) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    return HostHasPoint(tb ? tb->hoverHost : nullptr, pt);
}

static ToolbarHoverReg* FindHoverReg(ToolbarVirt* tb, int cmdId) {
    if (!tb || cmdId == 0) {
        return nullptr;
    }
    for (ToolbarHoverReg& reg : tb->hoverRegs) {
        if (reg.cmdId == cmdId) {
            return &reg;
        }
    }
    return nullptr;
}

void SetToolbarHoverDropdown(MainWindow* win, int cmdId, const Func1<ToolbarHoverBuildEvent*>& build, int groupId) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return;
    }
    if (ToolbarHoverReg* reg = FindHoverReg(tb, cmdId)) {
        reg->build = build;
        reg->groupId = groupId;
        return;
    }
    ToolbarHoverReg reg;
    reg.cmdId = cmdId;
    reg.groupId = groupId;
    reg.build = build;
    VecAppend(tb->hoverRegs, reg);
}

static void OpenHoverDropdown(MainWindow* win, int cmdId) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    ToolbarHoverReg* reg = FindHoverReg(tb, cmdId);
    if (!reg || !reg->build.IsValid() || !tb->host) {
        return;
    }
    Rect anchor = GetToolbarButtonScreenRect(win, cmdId);
    if (anchor.IsEmpty()) {
        return;
    }
    ToolbarHoverBuildEvent ev;
    ev.win = win;
    VecReset(tb->hoverItems);
    reg->build.Call(&ev);
    if (!ev.layout) {
        return;
    }

    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStrL(L"SumatraToolbarHoverMenu");
    args.isPopup = true;
    args.visible = false;
    args.noActivate = true;
    args.userData = win;
    args.bgColor = TbBgColor();
    args.isRtl = IsUIRtl();
    args.initialSize = {100, 100};
    VirtHost* host = VirtHost::Create(args);
    if (!host) {
        delete ev.layout;
        return;
    }
    host->onPaintBackground = MkFunc1(PaintHoverDropdownBg, win);
    host->onMouseMove = MkFunc0(OnHoverDropdownMouseMove, win);
    host->onMouseLeave = MkFunc0(OnHoverDropdownMouseLeave, win);
    Size sz = host->SetLayoutSizedToContent(ev.layout);

    // under the button, left edges aligned, kept on the monitor. A build that
    // asked for it instead hangs off the middle of the button, so it opens
    // around where the mouse already is
    int x = anchor.x;
    if (ev.centerOnButton) {
        x = anchor.x + ((anchor.dx - sz.dx) / 2);
    }
    Rect r{x, anchor.Bottom(), sz.dx, sz.dy};
    r = ShiftRectToWorkArea(r, win->hwndFrame, true);
    host->SetPos(r, true);

    TakeHoverButtonTooltip(win, cmdId);
    if (tb->host->vroot) {
        tb->host->vroot->HideTooltip();
    }
    tb->hoverHost = host;
    tb->hoverCmdId = cmdId;
    tb->hoverPendingCmdId = 0;
}

// The mouse moved over the toolbar (or left it): open, keep or close the
// drop-down of whatever button it is resting on. clientPt is the move; null
// on leave, which uses the real cursor so the drop-down stays up in it.
static void ToolbarHoverDropdownOnMouseMove(MainWindow* win, const Point* clientPt) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || !tb->host || len(tb->hoverRegs) == 0) {
        return;
    }
    Point ptScreen = UiCursorScreenPos();
    bool overMenu = ToolbarHoverDropdownContainsScreenPoint(win, ptScreen);
    int cmdId = 0;
    if (clientPt) {
        VirtCtrl* w = ToolbarItemFromPoint(win, *clientPt);
        if (w && FindHoverReg(tb, w->id)) {
            cmdId = w->id;
        }
    } else if (HostHasPoint(tb->host, ptScreen)) {
        // a disabled button still gets its drop-down, the way it still gets its
        // tooltip: the rows say what could be done and why they are greyed
        VirtCtrl* w = ToolbarItemFromPoint(win, tb->host->FromScreen(ptScreen));
        if (w && FindHoverReg(tb, w->id)) {
            cmdId = w->id;
        }
    }

    if (tb->hoverCmdId != 0) {
        // one is open: keep it while the mouse is on its button or in it
        if (overMenu || cmdId == tb->hoverCmdId) {
            tb->host->KillTimer(kCloseHoverDropdownTimerId);
            return;
        }
        if (cmdId != 0) {
            ToolbarHoverReg* from = FindHoverReg(tb, tb->hoverCmdId);
            ToolbarHoverReg* to = FindHoverReg(tb, cmdId);
            int group = from ? from->groupId : 0;
            if (group != 0 && to && to->groupId == group) {
                // both buttons share this drop-down, so it stays put: the two
                // sit side by side and sliding it between them would be a
                // twitch, not a new drop-down
                tb->host->KillTimer(kCloseHoverDropdownTimerId);
                GiveHoverButtonTooltipBack(win);
                tb->hoverCmdId = cmdId;
                TakeHoverButtonTooltip(win, cmdId);
                return;
            }
            // moved straight onto another button that has one: swap to it
            // without the delay, the way a menu bar follows the mouse
            HideToolbarHoverDropdown(win);
            OpenHoverDropdown(win, cmdId);
            return;
        }
        tb->host->SetTimer(kCloseHoverDropdownTimerId, kCloseHoverDropdownDelayMs);
        return;
    }
    if (cmdId == tb->hoverPendingCmdId) {
        return;
    }
    tb->hoverPendingCmdId = cmdId;
    tb->host->KillTimer(kOpenHoverDropdownTimerId);
    if (cmdId != 0) {
        tb->host->SetTimer(kOpenHoverDropdownTimerId, UiTooltipDelayMs());
    }
}

static void OnHoverDropdownTimer(MainWindow* win, int timerId) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || !tb->host) {
        return;
    }
    if (timerId == kOpenHoverDropdownTimerId) {
        tb->host->KillTimer(kOpenHoverDropdownTimerId);
        int cmdId = tb->hoverPendingCmdId;
        tb->hoverPendingCmdId = 0;
        if (cmdId != 0) {
            OpenHoverDropdown(win, cmdId);
        }
        return;
    }
    tb->host->KillTimer(kCloseHoverDropdownTimerId);
    Point pt = UiCursorScreenPos();
    if (ToolbarHoverDropdownContainsScreenPoint(win, pt)) {
        return;
    }
    // still on the button that opened it: leave it up
    if (GetToolbarButtonScreenRect(win, tb->hoverCmdId).Contains(pt)) {
        return;
    }
    HideToolbarHoverDropdown(win);
}

struct ZoomHoverLevel {
    float zoom;
    int cmdId;
};

// What the zoom strip lists: the levels the zoom buttons step through, so a
// click on one of them is the same jump the buttons make in one go, plus the
// two fit modes where 100% is. GetDefaultZoomLevels() is ZoomLevels from the
// settings when it is set, otherwise the built-in list, and GetZoomStepCmdIds()
// has a command for each of them, in the same order.
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

// The zoom buttons' drop-down: the zoom levels as a compact pyramid centred on
// the button, the one in use boxed, so it is a short trip from the button to
// any of the levels.
static void BuildZoomHoverMenu(MainWindow* win, ToolbarHoverBuildEvent* ev) {
    DocController* ctrl = win ? win->ctrl : nullptr;
    if (!ctrl) {
        return;
    }
    Vec<ZoomHoverLevel> levels;
    ZoomHoverLevels(levels);
    int currentIdx = ZoomHoverCurrentIdx(win, levels);

    Vec<ToolbarHoverMenuItem> items;
    for (int i = 0; i < len(levels); i++) {
        ToolbarHoverMenuItem it;
        it.text = ZoomLevelStrExact(levels[i].zoom);
        it.cmdId = levels[i].cmdId;
        it.isCurrent = i == currentIdx;
        VecAppend(items, it);
    }

    ev->layout = NewToolbarHoverStrip(win, items);
    ev->centerOnButton = true;
}

// The zoom moved (the buttons step it, and their strip stays up while they are
// clicked): box the level it landed on, if it landed on one. The strip stays
// where it is; sliding it out from under the mouse mid-click would be worse
// than the mark being off-centre.
static void UpdateZoomHoverDropdown(MainWindow* win) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || !tb->hoverHost) {
        return;
    }
    if (tb->hoverCmdId != CmdZoomIn && tb->hoverCmdId != CmdZoomOut) {
        return;
    }
    Vec<ZoomHoverLevel> levels;
    ZoomHoverLevels(levels);
    int currentIdx = ZoomHoverCurrentIdx(win, levels);
    for (int i = 0; i < len(tb->hoverItems); i++) {
        ToolbarHoverItemState& st = tb->hoverItems[i];
        if (!st.isStripCell || !st.ctrl) {
            continue;
        }
        bool isCurrent = i == currentIdx;
        auto* cell = (ToolbarHoverCell*)st.ctrl;
        if (cell->isCurrent == isCurrent) {
            continue;
        }
        cell->isCurrent = isCurrent;
        st.isCurrent = isCurrent;
        cell->Invalidate();
    }
}

// The Save button's drop-down: the three ways to end an editing session.
static void BuildSaveHoverMenu(MainWindow* win, ToolbarHoverBuildEvent* ev) {
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    auto* ctx = NewBuildMenuCtx(tab, Point{0, 0});
    AutoCall delCtx(DeleteBuildMenuCtx, ctx);
    bool dirty = ctx->hasUnsavedAnnotations;

    TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
    Str saveText = _TRA("Save changes to existing PDF");
    if (len(base) > 0) {
        saveText = fmt(_TRA("Save changes to %s").s, base);
    }

    Vec<ToolbarHoverMenuItem> items;
    VecAppend(items, {Str(gIconSave), saveText, CmdSaveAnnotations, dirty});
    VecAppend(items, {Str(gIconSaveToNewFile), _TRA("Save changes to a new PDF"), CmdSaveAnnotationsNewFile, dirty});
    VecAppend(items, {Str(gIconTrash), _TRA("Discard changes"), CmdDiscardChanges, dirty});
    ev->layout = NewToolbarHoverMenu(win, items);
}

static void OnToolbarMouseMove(MainWindow* win, Point pt) {
    UpdateOverlayToolbarForMouse(win);
    ToolbarHoverDropdownOnMouseMove(win, &pt);
}

static void OnToolbarMouseLeave(MainWindow* win) {
    UpdateOverlayToolbarForMouse(win);
    ToolbarHoverDropdownOnMouseMove(win, nullptr);
}

static void PaintToolbarSeparator(VirtCustom*, VirtPaintCtx* ctx) {
    Rect r = ctx->bounds;
    int inset = DpiScale(6);
    int dy = r.dy - (2 * inset);
    if (dy <= 0) {
        return;
    }
    int x = r.x + (r.dx / 2);
    ctx->gfx->FillRect({x, r.y + inset, 1, dy}, ThemeEdgeColor());
}

static VirtCtrl* MakeToolbarSeparator(int rowDy) {
    auto* sep = new VirtCustom();
    sep->idealSize = {DpiScale(8), rowDy};
    sep->onPaint = MkFunc1(PaintToolbarSeparator, sep);
    sep->SetFlag(vwfNoHitTest, true);
    return sep;
}

// (re)build the tree of virtual controls the toolbar is made of, one per button
static void BuildToolbarLayout(MainWindow* win) {
    PopulateToolbarLayout();
    PopulateCustomToolbarButtons();

    ToolbarVirt* tb = win->toolbarVirt;
    VecReset(tb->items);
    VecReset(tb->annotationItems);
    tb->annotationRow = nullptr;
    tb->pageLabel = nullptr;
    tb->pageLabel2 = nullptr;
    tb->pageTotal = nullptr;
    tb->chapterTotal = nullptr;
    win->pageEdit = nullptr;
    win->chapterEdit = nullptr;

    int cyPad = ToolbarCyPad();
    int iconPad = DpiScale(6);
    tb->rowDy = ToolbarRowDy(tb->iconSize);
    Color fg = TbTextColor();
    Color dis = TbDisabledColor();

    auto* box = new HBox();
    box->alignCross = CrossAxisAlign::CrossCenter;
    box->rtl = IsUIRtl();

    int n = TotalButtonsCount();
    for (int i = 0; i < n; i++) {
        const ToolbarButtonInfo& bi = GetToolbarButtonInfoByIdx(i);
        VirtCtrl* w = nullptr;
        bool noTranslate = i >= gLayoutButtonsCount;
        if (bi.cmdId == PageInfoId) {
            // Old toolbar: label HWND was text + kTextPaddingRight + kButtonSpacingX
            // (10dpi) so "Page:" and "/ N" were not flush against the edit.
            int pageGap = DpiScale(kTextPaddingRight) + DpiScale(kButtonSpacingX);
            auto* label = new VirtText(_TRA("Page:"), tb->platformFont);
            label->isRtl = box->rtl;
            label->SetColor(kColText, fg);
            label->padding = {0, pageGap, 0, DpiScale(4)};
            label->id = PageInfoId;
            tb->pageLabel = label;
            box->AddChild(label);

            // chapter box: [chapterEdit] / N, hidden unless HasChapters()
            Edit* chapterEdit = ToolbarCreateChapterEdit(win, tb->platformFont, tb->iconSize);
            chapterEdit->SetVisibility(Visibility::Collapse);
            win->chapterEdit = chapterEdit;
            box->AddChild(chapterEdit);

            auto* chapterTotal = new VirtText(StrL(" "), tb->platformFont);
            chapterTotal->isRtl = box->rtl;
            chapterTotal->SetColor(kColText, fg);
            chapterTotal->padding = {0, DpiScale(4), 0, pageGap};
            chapterTotal->id = PageInfoId;
            chapterTotal->SetVisibility(Visibility::Collapse);
            tb->chapterTotal = chapterTotal;
            box->AddChild(chapterTotal);

            // second "Page:" label, shown before pageEdit only for HasChapters() docs
            auto* label2 = new VirtText(_TRA("Page:"), tb->platformFont);
            label2->isRtl = box->rtl;
            label2->SetColor(kColText, fg);
            label2->padding = {0, pageGap, 0, DpiScale(4)};
            label2->id = PageInfoId;
            label2->SetVisibility(Visibility::Collapse);
            tb->pageLabel2 = label2;
            box->AddChild(label2);

            Edit* pageEdit = ToolbarCreatePageEdit(win, tb->platformFont, tb->iconSize);
            win->pageEdit = pageEdit;
            box->AddChild(pageEdit);

            auto* total = new VirtText(StrL(" "), tb->platformFont);
            total->isRtl = box->rtl;
            total->SetColor(kColText, fg);
            total->padding = {0, DpiScale(4), 0, pageGap};
            total->id = PageInfoId;
            tb->pageTotal = total;
            box->AddChild(total);
            VecAppend(tb->items, label);
            continue;
        }
        if (bi.cmdId == 0 || !HasToolbarButtonContent(bi)) {
            w = MakeToolbarSeparator(tb->rowDy);
        } else if (bi.isText) {
            auto* b = new VirtButton(noTranslate ? bi.toolTip : trans::GetTranslation(bi.toolTip), tb->platformFont);
            b->isRtl = box->rtl;
            b->textPadding = {cyPad, iconPad, cyPad, iconPad};
            w = b;
        } else {
            auto* ib = new VirtIconButton();
            ib->padding = {cyPad, iconPad, cyPad, iconPad};
            ib->hasDropdown = (bi.cmdId == CmdReadAloud);
            Str svg = bi.svgIcon ? bi.svgIcon : Str(bi.icon);
            ib->pixmap = GetCachedPixmapForSvg(svg, tb->iconSize, tb->iconSize, fg, TbBgColor());
            ib->pixmapDisabled = GetCachedPixmapForSvg(svg, tb->iconSize, tb->iconSize, dis, TbBgColor());
            w = ib;
        }
        ApplyToolbarItemColors(w);
        w->id = bi.cmdId;
        if (bi.toolTip) {
            bool translate = !noTranslate && !bi.isText;
            w->SetTooltip(ToolbarTipTemp(bi.cmdId, bi.toolTip, translate));
        }
        if (bi.cmdId != 0 && bi.cmdId != PageInfoId) {
            w->onClick = MkFunc1(OnToolbarButtonClicked, win);
        }
        VecAppend(tb->items, w);
        box->AddChild(w);
    }

    auto* annotationBox = new HBox();
    annotationBox->alignMain = MainAxisAlign::MainCenter;
    annotationBox->alignCross = CrossAxisAlign::CrossCenter;
    annotationBox->rtl = box->rtl;
    for (const ToolbarButtonInfo& bi : gPdfAnnotationButtons) {
        VirtCtrl* w = nullptr;
        if (!HasToolbarButtonContent(bi)) {
            w = MakeToolbarSeparator(tb->rowDy);
        } else {
            auto* ib = new VirtIconButton();
            ib->padding = {cyPad, iconPad, cyPad, iconPad};
            ib->pixmap = GetCachedPixmapForSvg(Str(bi.icon), tb->iconSize, tb->iconSize, fg, TbBgColor());
            ib->pixmapDisabled = GetCachedPixmapForSvg(Str(bi.icon), tb->iconSize, tb->iconSize, dis, TbBgColor());
            w = ib;
        }
        ApplyToolbarItemColors(w);
        w->id = bi.cmdId;
        if (bi.toolTip) {
            w->SetTooltip(ToolbarTipTemp(bi.cmdId, bi.toolTip, true));
        }
        if (bi.cmdId != 0) {
            w->onClick = MkFunc1(OnToolbarButtonClicked, win);
        }
        VecAppend(tb->annotationItems, w);
        annotationBox->AddChild(w);
    }

    auto* mainRow = new HBox();
    mainRow->alignCross = CrossAxisAlign::CrossCenter;
    mainRow->gap = DpiScale(kButtonSpacingX);
    mainRow->AddChild(box, 1);

    SetToolbarHoverDropdown(win, CmdSaveAnnotations, MkFunc1(BuildSaveHoverMenu, win));
    // one strip for the two of them, so it doesn't jump when the mouse crosses
    // from one to the other
    SetToolbarHoverDropdown(win, CmdZoomIn, MkFunc1(BuildZoomHoverMenu, win), CmdZoomIn);
    SetToolbarHoverDropdown(win, CmdZoomOut, MkFunc1(BuildZoomHoverMenu, win), CmdZoomIn);

    auto* root = new VBox();
    root->alignCross = CrossAxisAlign::Stretch;
    root->AddChild(new Padding(mainRow, Insets{0, DpiScale(4), 0, DpiScale(4)}));
    tb->annotationRow = new Padding(annotationBox, Insets{0, DpiScale(4), 0, DpiScale(4)});
    tb->annotationRow->SetVisibility(Visibility::Collapse);
    root->AddChild(tb->annotationRow);
    tb->host->SetLayout(root);
}

static void PaintToolbarBackground(MainWindow*, VirtHostPaintEvent* ev) {
    ev->gfx->FillRect(ev->clientRect, TbBgColor());
}

// the default theme separates the toolbar from the canvas with a hairline.
// Use the document background, not ThemeEdgeColor: on Light that is #c0c0c0
// and reads as a dark strip against the page.
static void PaintToolbarEdge(MainWindow*, VirtHostPaintEvent* ev) {
    if (!IsCurrentThemeDefault() || ThemeColorizeControls()) {
        return;
    }
    Color canvasBg;
    ThemeDocumentColors(canvasBg);
    Rect rc = ev->clientRect;
    int y = ToolbarAtBottom() ? rc.y : (rc.Bottom() - 1);
    ev->gfx->FillRect({rc.x, y, rc.dx, 1}, canvasBg);
}

static const WStr kToolbarHostClass = WStrL(L"SUMATRA_VIRT_TOOLBAR");

void CreateToolbar(MainWindow* win) {
    if (win->frameDpi > 0) {
        DpiSet(win->frameDpi, win->frameDpi);
    }
    int iconSize = ToolbarIconSize();
    int yPad = DpiScale(2);

    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = kToolbarHostClass;
    args.initialSize = {100, ToolbarRowDy(iconSize)};
    args.bgColor = TbBgColor();
    args.isRtl = IsUIRtl();
    args.visible = true;
    // the old Win32 toolbar did not take the keyboard focus; a generic child
    // would, and then accelerators (Ctrl+W, …) never reached the frame
    args.noActivate = true;
    // in overlay mode the canvas is a lower-Z sibling and would otherwise
    // paint over the floating toolbar
    args.clipSiblings = true;
    args.userData = win;

    VirtHost* host = VirtHost::Create(args);
    if (!host) {
        return;
    }
    host->onPaintBackground = MkFunc1(PaintToolbarBackground, win);
    host->onPaint = MkFunc1(PaintToolbarEdge, win);
    host->onTimer = MkFunc1(OnToolbarTimer, win);
    host->onMouseMove = MkFunc1(OnToolbarMouseMove, win);
    host->onMouseLeave = MkFunc0(OnToolbarMouseLeave, win);
    ToolbarSetNativeHooks(win, host);

    auto* tb = new ToolbarVirt();
    tb->host = host;
    tb->iconSize = iconSize;
    int newSize = GetAppFontSize();
    int maxFontSize = iconSize - (yPad * 2) - 2;
    if (newSize > maxFontSize) {
        newSize = maxFontSize;
    }
    tb->platformFont = GetDefaultGuiFontOfSize(newSize);
    win->toolbarVirt = tb;
    win->hwndToolbar = host->native;
    host->SetFont(tb->platformFont);

    BuildToolbarLayout(win);

    DocController* ctrl = win->ctrl;
    UpdateToolbarPageText(win, ctrl ? ctrl->PageCount() : -1);
    if (ctrl && win->pageEdit) {
        if (ctrl->HasChapters()) {
            Location cur = ctrl->CurrentLocation();
            win->pageEdit->SetText(fmt("%d", cur.page));
            if (win->chapterEdit) {
                win->chapterEdit->SetText(fmt("%d", cur.chapter));
            }
        } else {
            TempStr label = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
            win->pageEdit->SetText(label);
        }
        EditSetNumbersOnly(win->pageEdit, !ctrl->HasPageLabels());
    }
    UpdateToolbarFindText(win);
    ToolbarUpdateStateForWindow(win, true);
}

void DestroyToolbar(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb) {
        win->hwndToolbar = nullptr;
        return;
    }
    HideToolbarHoverDropdown(win);
    DeleteAnnotFilterToolbar(win);
    win->pageEdit = nullptr;
    win->chapterEdit = nullptr;
    win->toolbarVirt = nullptr;
    win->hwndToolbar = nullptr;
    delete tb->host;
    delete tb;
}

void ReCreateToolbar(MainWindow* win) {
    DestroyToolbar(win);
    CreateToolbar(win);
}
