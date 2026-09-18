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
#include "SumatraDialogs.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Theme.h"
#include "ReadAloud.h"
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
// unicode chars: https://www.compart.com/en/unicode/U+25BC

constexpr int kButtonsCount = dimof(gToolbarButtons);

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
    // the tooltip names the file, see ToolbarUpdateStateForWindow. Hovering it
    // opens a drop-down with the other two ways to end an editing session
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
        case CmdToggleReadAloud:
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

        if (cmdId == CmdToggleReadAloud || cmdId == CmdPauseReadAloud) {
            bool speaking = TtsIsSpeaking();
            SetToolbarButtonImageByIdx(win, i, speaking ? gIconPauseSpeaking : gIconSpeak);
            // tooltip reflects what clicking the button will do
            Str tip = Tr("Read Aloud");
            if (speaking) {
                tip = Tr("Pause Reading");
            } else if (CanContinueReadAloud(win->CurrentTab())) {
                tip = Tr("Continue Reading");
            }
            SetToolbarButtonToolTipByIdx(win, i, cmdId, tip);
        }
    }

    bool showPdfAnnotationsToolbar = win->pdfAnnotationsToolbarEnabled && ctx->isPdf && ctx->supportsAnnots;
    SetPdfAnnotationsToolbarVisible(win, showPdfAnnotationsToolbar);
    // a placement mode (ink, shape, highlighter...) owns the page until it ends
    bool annotButtonsEnabled = showPdfAnnotationsToolbar && !IsPlacingAnnotation(win);
    bool annotVisibilityChanged = false;
    for (int i = 0; i < kPdfAnnotationButtonsCount; i++) {
        const ToolbarButtonInfo& bi = gPdfAnnotationButtons[i];
        if (!HasToolbarButtonContent(bi)) {
            continue;
        }
        CommandVisibility v = GetCommandVisibility(bi.cmdId, *ctx, CommandSurface::Toolbar);
        bool remove = CommandShouldRemove(v);
        annotVisibilityChanged |= SetPdfAnnotationButtonHiddenByIdx(win, i, remove);
        SetPdfAnnotationButtonEnabledByIdx(win, i, annotButtonsEnabled && !CommandShouldDisable(v) && !remove);
        if (bi.cmdId == CmdSaveAnnotations) {
            // name the file it writes to, like the annotation list's Save button
            WindowTab* tab = win->CurrentTab();
            TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
            Str tip = Tr("Save changes to existing PDF");
            if (len(base) > 0) {
                tip = fmt(Tr("Save changes to %s").s, base);
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
        tb->pageLabel->SetText(hasChapters ? Tr("Chapter:") : Tr("Page:"));
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

static bool ShowToolbarButtonDropdown(MainWindow*, int cmdId);
static bool IsAnnotColorCmd(int cmdId);

static void OnToolbarButtonClicked(MainWindow* win, VirtMouseEvent* ev) {
    VirtCtrl* w = ev->target;
    if (!w || !win) {
        return;
    }
    int cmdId = w->id;
    if (cmdId == PageInfoId || cmdId == 0) {
        return;
    }
    if (ToolbarDropdownJustClosed() && (cmdId == CmdToggleReadAloud || cmdId == CmdPauseReadAloud)) {
        ev->didHandle = true;
        return;
    }
    // annotation buttons are disabled while a placement mode is on
    ToolbarVirt* tbv = win->toolbarVirt;
    if (tbv && IsPlacingAnnotation(win) && VecContains(tbv->annotationItems, w)) {
        return;
    }
    // right-click: the drop-down, not the button's command
    if (ev->button == 1) {
        if (ShowToolbarButtonDropdown(win, cmdId)) {
            ev->didHandle = true;
            return;
        }
    }
    if (!w->IsEnabled()) {
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
    // save: the hover menu's rows end the session; they no longer apply.
    // an annotation button: picking the tool is done, its colors are in the way
    if (cmdId == CmdSaveAnnotations || IsAnnotColorCmd(cmdId)) {
        HideToolbarHoverDropdown(win);
        // not again for as long as the mouse stays on the button
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
    if (tb && tb->host && tb->hoverCmdId != 0 && !tb->hoverSticky) {
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
    tb->hoverSticky = false;
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
    ev.cmdId = cmdId;
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

    tb->host->KillTimer(kOpenHoverDropdownTimerId);
    tb->host->KillTimer(kCloseHoverDropdownTimerId);

    TakeHoverButtonTooltip(win, cmdId);
    if (tb->host->vroot) {
        tb->host->vroot->HideTooltip();
    }
    tb->hoverHost = host;
    tb->hoverCmdId = cmdId;
    tb->hoverPendingCmdId = 0;
}

// Open the drop-down this button has, if any. One already up is left as it is.
static bool ShowToolbarButtonDropdown(MainWindow* win, int cmdId) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb) {
        return false;
    }

    if (auto* ib = AsVirtIconButton(ToolbarItemForCmd(win, cmdId))) {
        if (ib->hasDropdown) {
            ShowTtsVoiceMenu(win, GetToolbarButtonScreenRect(win, cmdId));
            return true;
        }
    }

    ToolbarHoverReg* to = FindHoverReg(tb, cmdId);
    if (!to) {
        return false;
    }
    if (tb->hoverCmdId == cmdId) {
        if (tb->host) {
            tb->host->KillTimer(kCloseHoverDropdownTimerId);
        }
        tb->hoverSticky = true;
        return true;
    }
    if (tb->hoverCmdId != 0) {
        ToolbarHoverReg* from = FindHoverReg(tb, tb->hoverCmdId);
        int group = from ? from->groupId : 0;
        if (group != 0 && to->groupId == group) {
            if (tb->host) {
                tb->host->KillTimer(kCloseHoverDropdownTimerId);
            }
            GiveHoverButtonTooltipBack(win);
            tb->hoverCmdId = cmdId;
            TakeHoverButtonTooltip(win, cmdId);
            tb->hoverSticky = true;
            return true;
        }
        HideToolbarHoverDropdown(win);
    }
    OpenHoverDropdown(win, cmdId);
    tb->hoverSticky = true;
    return true;
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
    VirtCtrl* w = nullptr;
    if (clientPt) {
        w = ToolbarItemFromPoint(win, *clientPt);
    } else if (HostHasPoint(tb->host, ptScreen)) {
        // a disabled button still gets its drop-down, the way it still gets its
        // tooltip: the rows say what could be done and why they are greyed
        w = ToolbarItemFromPoint(win, tb->host->FromScreen(ptScreen));
    }
    if (w && FindHoverReg(tb, w->id)) {
        cmdId = w->id;
    }
    // except annotation buttons disabled by a placement mode
    if (w && IsPlacingAnnotation(win) && VecContains(tb->annotationItems, w)) {
        cmdId = 0;
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
        if (tb->hoverSticky) {
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
    if (tb->hoverSticky) {
        return;
    }
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
    Str saveText = Tr("Save changes to existing PDF");
    if (len(base) > 0) {
        saveText = fmt(Tr("Save changes to %s").s, base);
    }

    Vec<ToolbarHoverMenuItem> items;
    VecAppend(items, {Str(gIconSave), saveText, CmdSaveAnnotations, dirty});
    VecAppend(items, {Str(gIconSaveToNewFile), Tr("Save changes to a new PDF"), CmdSaveAnnotationsNewFile, dirty});
    VecAppend(items, {Str(gIconTrash), Tr("Discard changes"), CmdDiscardChanges, dirty});
    ev->layout = NewToolbarHoverMenu(win, items);
}

//--- the annotation buttons' color drop-down

// Every annotation button that creates something with a color offers the colors
// in Annotations.PresetColors: picking one becomes the color of new annotations
// of that type, and the pencil opens the color dialog on the whole set.
constexpr int kAnnotSwatchDx = 22;
// ring space around the circle, where the mark on the color in use goes
constexpr int kAnnotSwatchPad = 5;
constexpr int kAnnotColorsPad = 10;

// the buttons that offer the preset colors
// Redact is left out: its color is the box that covers the text, not a choice
static const int kAnnotColorCmds[] = {
    CmdAnnotationHighlightBrush, CmdCreateAnnotHighlight, CmdCreateAnnotUnderline, CmdCreateAnnotSquiggly,
    CmdCreateAnnotStrikeOut,     CmdCreateAnnotText,      CmdCreateAnnotFreeText,  CmdCreateAnnotLine,
    CmdCreateAnnotPolyLine,      CmdCreateAnnotSquare,    CmdCreateAnnotCircle,    CmdCreateAnnotPolygon,
    CmdCreateAnnotInk,           CmdCreateAnnotStamp,     CmdCreateAnnotCaret,     CmdCreateAnnotFileAttachment,
};

static bool IsAnnotColorCmd(int cmdId) {
    for (int id : kAnnotColorCmds) {
        if (id == cmdId) {
            return true;
        }
    }
    return false;
}

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
static Color AnnotCurrentColor(int cmdId) {
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

static void AnnotPresetColors(int cmdId, Vec<Color>& out) {
    if (Str* list = AnnotPresetColorList(cmdId)) {
        ParseColorList(*list, out, 0);
    }
}

static void SetAnnotPresetColor(int cmdId, Color col) {
    ParsedColor* setting = AnnotPresetColorSetting(cmdId);
    if (!setting) {
        return;
    }
    SetColorText(*setting, SerializeColorTemp(col));
    ScheduleSaveSettings();
}

// A color as a filled circle, the way a highlighter's colors are shown. The one
// in use is ringed, and so is the one under the mouse.
struct ToolbarColorSwatch : VirtCtrl {
    Color col = kColorUnset;
    Str text; // owned; the color as text, for the -dbg-control dump
    bool isCurrent = false;
    bool isNone = false; // no color at all, drawn as an empty circle with a slash

    ToolbarColorSwatch() { cursor = CursorId::Hand; }
    ~ToolbarColorSwatch() override { str::Free(text); }

    Size GetIdealSize() override {
        int dx = DpiScale(kAnnotSwatchDx) + (2 * DpiScale(kAnnotSwatchPad));
        return {dx, dx};
    }

    void Paint(VirtPaintCtx& ctx) override {
        Rect r = ctx.bounds;
        int d = std::min(r.dx, r.dy);
        Rect ring{r.x + ((r.dx - d) / 2), r.y + ((r.dy - d) / 2), d, d};
        int t = DpiScale(1);
        if (isCurrent || HasFlag(vwfHovered)) {
            // a filled disc with a smaller one of the background punched out of
            // it: Gfx fills ellipses but doesn't outline them
            ctx.gfx->FillEllipse(ring, TbTextColor());
            Rect hole = ring;
            hole.Inflate(-t, -t);
            ctx.gfx->FillEllipse(hole, TbBgColor());
        }
        Rect circle = ring;
        int p = DpiScale(kAnnotSwatchPad);
        circle.Inflate(-p, -p);
        ctx.gfx->FillEllipse(circle, TbEdgeColor());
        circle.Inflate(-t, -t);
        if (isNone) {
            ctx.gfx->FillEllipse(circle, TbBgColor());
            // a slash from the lower left to the upper right, inset so it
            // stays inside the circle
            int inset = (int)((float)circle.dx * 0.15f);
            Point p1{circle.x + inset, circle.Bottom() - inset};
            Point p2{circle.Right() - inset, circle.y + inset};
            ctx.gfx->DrawLineAA(p1, p2, TbTextColor(), (float)t + 0.5f);
            return;
        }
        u8 a = GetAlpha(col);
        ctx.gfx->FillEllipse(circle, col & 0xffffff, a == 0 ? 255 : a);
    }
};

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

static void OnAnnotColorClicked(MainWindow* win, VirtMouseEvent* ev) {
    auto* sw = ev ? (ToolbarColorSwatch*)ev->target : nullptr;
    if (!sw) {
        return;
    }
    SetAnnotPresetColor(sw->id, sw->col);
    if (CanCreateAnnotFromSelection(win, sw->id)) {
        // with text selected, picking a color is also a request to mark it up
        ToolbarPostCommand(win, sw->id);
    }
    uitask::Post(MkFunc0(PostedHideHoverDropdown, win), "HideToolbarHoverDropdown");
}

// which button's color the generic color dialog is editing; a valid onPick
// instead means it was opened for something that is not a toolbar button
struct AnnotColorsTarget {
    int cmdId = 0;
    Func1<Color> onPick;
};

static void AnnotColorsPicked(AnnotColorsTarget* target, ChangeColorsArgs* args) {
    Str* list = AnnotPresetColorList(target->cmdId);
    if (args->colorsChanged && list) {
        str::ReplaceWithCopy(list, SerializeColorList(args->colors));
        ScheduleSaveSettings();
    }
    if (args->didSelect && args->color != kColorUnset) {
        if (target->onPick.IsValid()) {
            target->onPick.Call(args->color);
        } else {
            SetAnnotPresetColor(target->cmdId, args->color);
        }
    }
    delete target;
}

static void OnAnnotColorsEditClicked(MainWindow* win, VirtMouseEvent* ev) {
    VirtCtrl* w = ev ? ev->target : nullptr;
    if (!w) {
        return;
    }
    int cmdId = w->id;
    uitask::Post(MkFunc0(PostedHideHoverDropdown, win), "HideToolbarHoverDropdown");

    auto* target = new AnnotColorsTarget();
    target->cmdId = cmdId;

    auto* args = new ChangeColorsArgs();
    args->win = win;
    args->title = Tr("Annotation Colors");
    args->color = AnnotCurrentColor(cmdId);
    args->withOpacity = true;
    AnnotPresetColors(cmdId, args->colors);
    args->onClose = MkFunc1(AnnotColorsPicked, target);
    ShowChangeColorsDialog(args);
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

// The drop-down's content: the preset colors as swatches with the one in use
// ringed, and a button that opens the color dialog on the whole set. cmdId is 0
// when this is not a toolbar button's drop-down, and nothing is recorded then.
// swatchesOut, when given, collects the swatches in the order they are laid
// out, for the -dbg-control dump
static ILayout* MakeAnnotColorsPanel(MainWindow* win, Str label, Color current, int cmdId, bool withNone,
                                     Vec<ToolbarColorSwatch*>* swatchesOut, const Func1<VirtMouseEvent*>& onSwatch,
                                     const Func1<VirtMouseEvent*>& onEdit, ILayout* extra = nullptr, Str title = {}) {
    ToolbarVirt* tb = win->toolbarVirt;
    Vec<Color> colors;
    AnnotPresetColors(cmdId, colors);

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    if (withNone) {
        // for a color that can be left out, like a shape's interior
        auto* sw = new ToolbarColorSwatch();
        sw->id = cmdId;
        sw->col = kColorUnset;
        sw->isNone = true;
        sw->isCurrent = (current == kColorUnset);
        str::ReplaceWithCopy(&sw->text, StrL("none"));
        // the named-color menu calls it that, untranslated like the other color names
        sw->SetTooltip(StrL("Transparent"));
        sw->onClick = onSwatch;
        row->AddChild(sw);
        if (swatchesOut) {
            VecAppend(*swatchesOut, sw);
        }
    }
    for (Color col : colors) {
        auto* sw = new ToolbarColorSwatch();
        sw->id = cmdId;
        sw->col = col;
        sw->isCurrent = SameColorAndAlpha(col, current);
        str::ReplaceWithCopy(&sw->text, SerializeColorTemp(col));
        sw->onClick = onSwatch;
        row->AddChild(sw);
        if (swatchesOut) {
            VecAppend(*swatchesOut, sw);
        }
        if (cmdId != 0) {
            RecordHoverItem(tb, sw, sw->text, {{}, sw->text, cmdId, true, sw->isCurrent});
        }
    }

    auto* edit = new VirtIconButton();
    int iconSize = tb->iconSize;
    int pad = DpiScale(kAnnotSwatchPad);
    edit->id = cmdId;
    edit->padding = {pad, pad, pad, pad};
    edit->pixmap = GetCachedPixmapForSvg(Str(gIconEditAnnotations), iconSize, iconSize, TbTextColor(), TbBgColor());
    edit->SetTooltip(Tr("Edit colors"));
    edit->onClick = onEdit;
    row->AddChild(edit);

    auto* labelText = NewVirtText({
        .s = label,
        .font = tb->platformFont,
        .textColor = TbTextColor(),
        .isRtl = IsUIRtl(),
    });
    // indented by the swatch's padding so the text lines up with the first
    // color, and 0.25rem above the swatches
    Insets labelInsets{.bottom = DpiScale(4)};
    if (IsUIRtl()) {
        labelInsets.right = pad;
    } else {
        labelInsets.left = pad;
    }

    ILayout* labelRow = labelText;
    if (len(title) > 0) {
        // what the button is, as its tooltip says, since the drop-down takes
        // the tooltip's place; on the far end of the label's row
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::SpaceBetween;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->AddChild(labelText);
        Insets gap{};
        if (IsUIRtl()) {
            gap.right = DpiScale(16);
        } else {
            gap.left = DpiScale(16);
        }
        hbox->AddChild(new Padding(NewVirtText({
                                       .s = title,
                                       .font = tb->platformFont,
                                       .textColor = TbTextColor(),
                                       .isRtl = IsUIRtl(),
                                   }),
                                   gap));
        labelRow = hbox;
    }

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(new Padding(labelRow, labelInsets));
    vbox->AddChild(row);
    if (extra) {
        vbox->AddChild(extra);
    }
    int b = DpiScale(kHoverMenuBorder);
    int p = DpiScale(kAnnotColorsPad);
    return new Padding(vbox, Insets{b + p, b + p, b + p, b + p});
}

//--- the ink button's drop-down also sets how thick the stroke is

// Annotations.InkBorderWidth is in PDF points, which is about a pixel at 100%
constexpr int kInkThicknessMin = 1;
constexpr int kInkThicknessMax = 16;
constexpr int kInkPreviewDy = 44;
constexpr int kInkSliderDx = 190;
// how far the preview's wave swings, as a part of the room left by the stroke
constexpr float kInkPreviewWave = 0.42f;

static int InkThickness() {
    int v = gSettings ? gSettings->annotations.inkBorderWidth : kInkThicknessMin;
    return limitValue(v, kInkThicknessMin, kInkThicknessMax);
}

// What the ink button will lay down: the color in use, drawn as thick as the
// slider is set to. It follows the slider while it's being dragged.
struct InkStrokePreview : VirtCtrl {
    Color col = kColRed;
    int thickness = kInkThicknessMin;

    Size GetIdealSize() override { return {DpiScale(kInkSliderDx), DpiScale(kInkPreviewDy)}; }

    void Paint(VirtPaintCtx& ctx) override {
        Rect r = ctx.bounds;
        float w = (float)DpiScale(thickness);
        // the stroke has to fit the preview whatever the thickness
        w = std::min(w, (float)r.dy / 2.f);
        w = std::max(w, 1.f);
        int inset = (int)(w / 2.f) + DpiScale(2);
        int x0 = r.x + inset;
        int x1 = r.Right() - inset;
        if (x1 <= x0) {
            return;
        }
        float midY = (float)r.y + ((float)r.dy / 2.f);
        float amp = (((float)r.dy / 2.f) - (float)inset) * kInkPreviewWave;
        u8 a = GetAlpha(col);
        Color c = col & 0xffffff;
        // one period of a sine, as a run of short anti-aliased segments, with a
        // disc at every joint: the segments are butt-capped and a thick curve
        // would be notched without them
        constexpr int kSegs = 48;
        int d = (int)w;
        u8 alpha = (a == 0) ? 255 : a;
        Point prev{};
        for (int i = 0; i <= kSegs; i++) {
            float u = (float)i / (float)kSegs;
            int x = x0 + (int)(u * (float)(x1 - x0));
            int y = (int)(midY + (amp * sinf(u * 2.f * 3.14159265f)));
            Point pt{x, y};
            if (i > 0) {
                ctx.gfx->DrawLineAA(prev, pt, c, w, alpha);
            }
            if (d > 2) {
                ctx.gfx->FillEllipse(Rect{pt.x - (d / 2), pt.y - (d / 2), d, d}, c, alpha);
            }
            prev = pt;
        }
    }
};

// Dragging it is the width of the next ink annotation, and of the stroke the
// preview shows. The preview is a sibling in the same drop-down, so it lives
// exactly as long as the slider does.
struct InkThicknessSlider : VirtSlider {
    InkStrokePreview* preview = nullptr;
    // the width as a number, under the slider; a sibling like the preview
    VirtText* valueText = nullptr;
    Str text; // owned; what the -dbg-control dump shows for the slider
    // where the width goes when let go. Without one it's the setting the ink
    // button makes its strokes with
    Func1<int> onThickness;
    // committed by letting go of a drag, so the mouse capture is about to be
    // released; the color popup that holds the mouse takes it back
    bool releasingMouse = false;

    ~InkThicknessSlider() override { str::Free(text); }

    void OnChanged() {
        if (preview) {
            preview->thickness = value;
            preview->Invalidate();
        }
        if (valueText) {
            valueText->SetText(fmt("%d", value));
            valueText->Invalidate();
        }
        if (!onThickness.IsValid() && gSettings) {
            gSettings->annotations.inkBorderWidth = value;
        }
    }
    // rewriting an annotation is too slow to do on every step of a drag, so
    // the width lands when the slider is let go
    void OnCommitted() {
        // a mouse-up commits while still adjusting, a wheel step doesn't
        releasingMouse = IsAdjusting();
        OnChanged();
        if (onThickness.IsValid()) {
            onThickness.Call(value);
        } else {
            ScheduleSaveSettings();
        }
    }
};

// sliderOut gets the slider, for the caller to record once the colors are in.
// thickness < 0 starts the slider at Annotations.InkBorderWidth and leaves the
// width there; otherwise it starts there and onThickness gets it
static ILayout* MakeInkThicknessPanel(MainWindow* win, Color current, int thickness, const Func1<int>& onThickness,
                                      Str label, int minThickness, InkThicknessSlider** sliderOut) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (thickness < 0) {
        thickness = InkThickness();
    }
    thickness = limitValue(thickness, minThickness, kInkThicknessMax);

    auto* preview = new InkStrokePreview();
    preview->col = (current == kColorUnset) ? kColRed : current;
    preview->thickness = thickness;

    auto* slider = new InkThicknessSlider();
    slider->minVal = minThickness;
    slider->maxVal = kInkThicknessMax;
    slider->value = thickness;
    slider->idealDx = DpiScale(kInkSliderDx);
    slider->preview = preview;
    slider->onThickness = onThickness;
    slider->onValueChanged = MkMethod0<InkThicknessSlider, &InkThicknessSlider::OnChanged>(slider);
    slider->onValueCommitted = MkMethod0<InkThicknessSlider, &InkThicknessSlider::OnCommitted>(slider);
    str::ReplaceWithCopy(&slider->text, fmt("thickness=%d", thickness));

    auto* ends = new HBox();
    ends->alignMain = MainAxisAlign::SpaceBetween;
    ends->alignCross = CrossAxisAlign::CrossCenter;
    auto mkLabel = [tb](Str s) {
        return NewVirtText({
            .s = s,
            .font = tb->platformFont,
            .textColor = TbDisabledColor(),
            .isRtl = IsUIRtl(),
        });
    };
    // the width as a number, centered between the ends. Padded out to the
    // widest value so it keeps its place when a drag adds a digit.
    TempStr valueStr = fmt("%d", thickness);
    int widestDx = PlatformFontMeasureText(tb->platformFont, fmt("%d", kInkThicknessMax)).dx;
    int extraDx = std::max(widestDx - PlatformFontMeasureText(tb->platformFont, valueStr).dx, 0);
    auto* valueText = NewVirtText({
        .s = valueStr,
        .font = tb->platformFont,
        .textColor = TbTextColor(),
        .align = VirtTextAlign::Center,
        .isRtl = IsUIRtl(),
        .padding = {.right = extraDx - (extraDx / 2), .left = extraDx / 2},
    });
    slider->valueText = valueText;

    ends->AddChild(mkLabel(Tr("Thin")));
    ends->AddChild(valueText);
    ends->AddChild(mkLabel(Tr("Thick")));

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    int gap = DpiScale(6);
    vbox->AddChild(new Padding(preview, Insets{gap, 0, gap, 0}));
    vbox->AddChild(NewVirtText({
        .s = label,
        .font = tb->platformFont,
        .textColor = TbTextColor(),
        .isRtl = IsUIRtl(),
    }));
    vbox->AddChild(slider);
    vbox->AddChild(ends);
    *sliderOut = slider;
    return vbox;
}

static void BuildAnnotColorsHoverMenu(MainWindow* win, ToolbarHoverBuildEvent* ev) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    ParsedColor* setting = AnnotPresetColorSetting(ev->cmdId);
    if (!tb || !setting) {
        return;
    }
    Color current = AnnotCurrentColor(ev->cmdId);
    EnsureAnnotPresetColor(ev->cmdId, current);
    // ink is the one annotation whose width is a choice too
    InkThicknessSlider* slider = nullptr;
    ILayout* extra = (ev->cmdId == CmdCreateAnnotInk)
                         ? MakeInkThicknessPanel(win, current, -1, {}, Tr("Thickness"), kInkThicknessMin, &slider)
                         : nullptr;
    // a note's color fills its icon, behind the note
    Str label = (ev->cmdId == CmdCreateAnnotText) ? Tr("Background Color") : Tr("Color");
    // the button still has its tooltip; it's taken once the drop-down is up
    VirtCtrl* btn = ToolbarItemForCmd(win, ev->cmdId);
    Str title = btn ? btn->tooltip : Str{};
    ev->layout = MakeAnnotColorsPanel(win, label, current, ev->cmdId, false, nullptr, MkFunc1(OnAnnotColorClicked, win),
                                      MkFunc1(OnAnnotColorsEditClicked, win), extra, title);
    if (slider) {
        RecordHoverItem(tb, slider, slider->text, {{}, slider->text, ev->cmdId, true, false});
    }
    ev->centerOnButton = true;
}

//--- the same drop-down, opened from a chip of the annotation edit toolbar

// There is no toolbar button to hover here, so the popup keeps the mouse and
// the first click outside it dismisses it, the way a menu does.
struct AnnotColorPopup {
    VirtHost* host = nullptr;
    MainWindow* win = nullptr;
    Color current = kColorUnset;
    Func1<Color> onPick;
    Color picked = kColorUnset;
    bool hasPick = false;
    bool openDialog = false;
    // non-owning, for tests; the layout tree owns them
    Vec<ToolbarColorSwatch*> swatches;
    InkThicknessSlider* slider = nullptr;
};

static AnnotColorPopup* gAnnotColorPopup = nullptr;

static void ShowAnnotColorsDialog(MainWindow* win, Color current, const Func1<Color>& onPick);
static void ShowAnnotPopupHost(AnnotColorPopup* p, ILayout* layout, Rect anchor);

static void PostedCloseAnnotColorPopup(MainWindow* win) {
    AnnotColorPopup* p = gAnnotColorPopup;
    if (!p) {
        return;
    }
    gAnnotColorPopup = nullptr;
    Func1<Color> onPick = p->onPick;
    bool hasPick = p->hasPick;
    bool openDialog = p->openDialog;
    Color col = p->picked;
    Color current = p->current;
    delete p->host;
    delete p;
    if (hasPick) {
        onPick.Call(col);
    }
    if (openDialog) {
        // only now: destroying the popup activates its owner, which would put
        // the dialog behind the main window if it were already up
        ShowAnnotColorsDialog(win, current, onPick);
    }
}

// the click is handled by the popup's own window, so the window can only be
// torn down once that returns
static void CloseAnnotColorPopup(AnnotColorPopup* p) {
    if (::GetCapture() == p->host->native) {
        ::ReleaseCapture();
    }
    uitask::Post(MkFunc0(PostedCloseAnnotColorPopup, p->win), "CloseAnnotColorPopup");
}

static void OnAnnotColorPopupSwatch(AnnotColorPopup* p, VirtMouseEvent* ev) {
    auto* sw = ev ? (ToolbarColorSwatch*)ev->target : nullptr;
    if (!sw || p != gAnnotColorPopup) {
        return;
    }
    p->picked = sw->col;
    p->hasPick = true;
    CloseAnnotColorPopup(p);
}

static void ShowAnnotColorsDialog(MainWindow* win, Color current, const Func1<Color>& onPick) {
    auto* target = new AnnotColorsTarget();
    target->onPick = onPick;

    auto* args = new ChangeColorsArgs();
    args->win = win;
    args->title = Tr("Annotation Colors");
    args->color = current;
    args->withOpacity = true;
    AnnotPresetColors(0, args->colors);
    args->onClose = MkFunc1(AnnotColorsPicked, target);
    ShowChangeColorsDialog(args);
}

static void OnAnnotColorPopupEdit(AnnotColorPopup* p, VirtMouseEvent*) {
    if (p != gAnnotColorPopup) {
        return;
    }
    p->openDialog = true;
    CloseAnnotColorPopup(p);
}

static void PaintAnnotColorPopupBg(MainWindow*, VirtHostPaintEvent* ev) {
    ev->gfx->FillRect(ev->clientRect, TbBgColor());
    ev->gfx->DrawRect(ev->clientRect, ThemeEdgeColor(), DpiScale(kHoverMenuBorder));
}

static void PostedReclaimAnnotColorPopupCapture(MainWindow*) {
    AnnotColorPopup* p = gAnnotColorPopup;
    if (p && p->host) {
        ::SetCapture(p->host->native);
    }
}

static void AnnotColorPopupNativeMsg(AnnotColorPopup* p, VirtHostNativeMsg* ev) {
    if (p != gAnnotColorPopup) {
        return;
    }
    switch (ev->msg) {
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            // the mouse is captured, so clicks meant for another window come
            // here too: they dismiss the popup and go no further
            Point pt{GET_X_LPARAM(ev->lp), GET_Y_LPARAM(ev->lp)};
            if (p->host->ClientRect().Contains(pt)) {
                return;
            }
            CloseAnnotColorPopup(p);
            ev->didHandle = true;
            ev->res = 0;
            break;
        }
        case WM_CAPTURECHANGED:
            if ((HWND)ev->lp == p->host->native) {
                break;
            }
            // the slider lets go of the mouse when its drag ends; the popup
            // takes it back instead of treating that as a click elsewhere
            if (!ev->lp && p->slider && p->slider->releasingMouse) {
                p->slider->releasingMouse = false;
                uitask::Post(MkFunc0(PostedReclaimAnnotColorPopupCapture, p->win), "ReclaimAnnotColorPopupCapture");
                break;
            }
            CloseAnnotColorPopup(p);
            break;
    }
}

void ShowAnnotColorPopup(MainWindow* win, Rect anchor, Color current, bool withNone, Str label,
                         const Func1<Color>& onPick, int thickness, const Func1<int>& onThickness, Str thicknessLabel,
                         int minThickness) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || gAnnotColorPopup) {
        return;
    }
    auto* p = new AnnotColorPopup();
    p->win = win;
    p->current = current;
    p->onPick = onPick;
    // an ink annotation's stroke is as much a choice as its color, so its
    // popup has the same Thickness slider the ink button's drop-down has
    InkThicknessSlider* slider = nullptr;
    ILayout* extra =
        (thickness >= 0)
            ? MakeInkThicknessPanel(win, current, thickness, onThickness,
                                    len(thicknessLabel) > 0 ? thicknessLabel : Tr("Thickness"), minThickness, &slider)
            : nullptr;
    ILayout* layout =
        MakeAnnotColorsPanel(win, label, current, 0, withNone, &p->swatches, MkFunc1(OnAnnotColorPopupSwatch, p),
                             MkFunc1(OnAnnotColorPopupEdit, p), extra);
    p->slider = slider;
    ShowAnnotPopupHost(p, layout, anchor);
}

// A number picked with a slider: a label, the slider, and the value under it.
// onValue gets the value when the slider is let go.
void ShowAnnotSliderPopup(MainWindow* win, Rect anchor, Str label, int value, int minVal, int maxVal,
                          const Func1<int>& onValue) {
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    if (!tb || gAnnotColorPopup) {
        return;
    }
    auto* p = new AnnotColorPopup();
    p->win = win;
    value = limitValue(value, minVal, maxVal);

    auto* slider = new InkThicknessSlider();
    slider->minVal = minVal;
    slider->maxVal = maxVal;
    slider->value = value;
    slider->idealDx = DpiScale(kInkSliderDx);
    slider->onThickness = onValue;
    slider->onValueChanged = MkMethod0<InkThicknessSlider, &InkThicknessSlider::OnChanged>(slider);
    slider->onValueCommitted = MkMethod0<InkThicknessSlider, &InkThicknessSlider::OnCommitted>(slider);
    str::ReplaceWithCopy(&slider->text, fmt("thickness=%d", value));

    auto mkEnd = [tb](int v) {
        return NewVirtText({
            .s = fmt("%d", v),
            .font = tb->platformFont,
            .textColor = TbDisabledColor(),
            .isRtl = IsUIRtl(),
        });
    };
    // centered between the ends, padded out to the widest value so it keeps
    // its place when a drag adds a digit
    TempStr valueStr = fmt("%d", value);
    int widestDx = PlatformFontMeasureText(tb->platformFont, fmt("%d", maxVal)).dx;
    int extraDx = std::max(widestDx - PlatformFontMeasureText(tb->platformFont, valueStr).dx, 0);
    auto* valueText = NewVirtText({
        .s = valueStr,
        .font = tb->platformFont,
        .textColor = TbTextColor(),
        .align = VirtTextAlign::Center,
        .isRtl = IsUIRtl(),
        .padding = {.right = extraDx - (extraDx / 2), .left = extraDx / 2},
    });
    slider->valueText = valueText;

    auto* ends = new HBox();
    ends->alignMain = MainAxisAlign::SpaceBetween;
    ends->alignCross = CrossAxisAlign::CrossCenter;
    ends->AddChild(mkEnd(minVal));
    ends->AddChild(valueText);
    ends->AddChild(mkEnd(maxVal));

    auto* vbox = new VBox();
    vbox->alignCross = CrossAxisAlign::Stretch;
    vbox->AddChild(new Padding(NewVirtText({
                                   .s = label,
                                   .font = tb->platformFont,
                                   .textColor = TbTextColor(),
                                   .isRtl = IsUIRtl(),
                               }),
                               Insets{.bottom = DpiScale(4)}));
    vbox->AddChild(slider);
    vbox->AddChild(ends);
    int b = DpiScale(kHoverMenuBorder);
    int pad = DpiScale(kAnnotColorsPad);
    ILayout* layout = new Padding(vbox, Insets{b + pad, b + pad, b + pad, b + pad});
    p->slider = slider;
    ShowAnnotPopupHost(p, layout, anchor);
}

static void ShowAnnotPopupHost(AnnotColorPopup* p, ILayout* layout, Rect anchor) {
    MainWindow* win = p->win;
    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStrL(L"SumatraAnnotColorPopup");
    args.isPopup = true;
    args.visible = false;
    args.noActivate = true;
    args.userData = win;
    args.bgColor = TbBgColor();
    args.isRtl = IsUIRtl();
    args.initialSize = {100, 100};
    VirtHost* host = VirtHost::Create(args);
    if (!host) {
        delete layout;
        delete p;
        return;
    }
    p->host = host;
    host->onPaintBackground = MkFunc1(PaintAnnotColorPopupBg, win);
    host->onNativeMsg = MkFunc1(AnnotColorPopupNativeMsg, p);
    Size sz = host->SetLayoutSizedToContent(layout);

    // under the chip, centered on it, kept on the monitor
    Rect r{anchor.x + ((anchor.dx - sz.dx) / 2), anchor.Bottom(), sz.dx, sz.dy};
    r = ShiftRectToWorkArea(r, win->hwndFrame, true);
    host->SetPos(r, true);
    gAnnotColorPopup = p;
    ::SetCapture(host->native);
}

// for tests: the swatches of the drop-down that is up, if any
TempStr AnnotColorPopupStateTemp() {
    AnnotColorPopup* p = gAnnotColorPopup;
    if (!p || !p->host) {
        return fmt("annotColorPopup visible=0 n=0 thickness= swatches=\n");
    }
    Rect r = p->host->ScreenRect();
    str::Builder swatches;
    for (int i = 0; i < len(p->swatches); i++) {
        if (i > 0) {
            swatches.AppendChar(';');
        }
        ToolbarColorSwatch* sw = p->swatches[i];
        Rect sr = sw->BoundsInWindow();
        swatches.Append(
            fmt("%s:%d,%d,%d,%d:%d", sw->text, r.x + sr.x, r.y + sr.y, sr.dx, sr.dy, sw->isCurrent ? 1 : 0));
    }
    Str thickness = StrL("");
    if (p->slider) {
        Rect sr = p->slider->BoundsInWindow();
        thickness = fmt("%d:%d,%d,%d,%d", p->slider->value, r.x + sr.x, r.y + sr.y, sr.dx, sr.dy);
    }
    return fmt("annotColorPopup visible=1 n=%d placed=%d,%d,%d,%d thickness=%s swatches=%s\n", len(p->swatches), r.x,
               r.y, r.dx, r.dy, thickness, ToStrTemp(swatches));
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
            auto* label = new VirtText(Tr("Page:"), tb->platformFont);
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
            auto* label2 = new VirtText(Tr("Page:"), tb->platformFont);
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
            ib->hasDropdown = (bi.cmdId == CmdToggleReadAloud);
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
    // no shared group: each of them shows the color it is set to
    for (int cmdId : kAnnotColorCmds) {
        SetToolbarHoverDropdown(win, cmdId, MkFunc1(BuildAnnotColorsHoverMenu, win));
    }

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

#if OS_WIN

// What the toolbar still needs Win32 for, now that VirtHost owns its window:
// the colors of the native page-number edit, dragging the frame by an empty
// part of the toolbar, eating the click that dismissed a drop-down menu, and
// reaching the frame and canvas windows (which are not VirtHosts yet).

//--- the frame and the canvas are still plain HWNDs

// canvas rectangle in frame-client coordinates
Rect ToolbarCanvasRectInFrame(MainWindow* win) {
    Rect rc = HwndWindowRect(win->hwndCanvas);
    Point tl = HwndScreenToClient(win->hwndFrame, rc.TL());
    return {tl, rc.Size()};
}

// a screen point in frame-client coordinates
Point ToolbarScreenToFrame(MainWindow* win, Point pt) {
    return HwndScreenToClient(win->hwndFrame, pt);
}

// repaint what the overlay toolbar was covering after it hides
void ToolbarRepaintUncovered(MainWindow* win, Rect rInFrame) {
    HwndInvalidate(win->hwndCanvas);
    HwndInvalidateRect(win->hwndFrame, rInFrame, false);
}

void ToolbarFocusFrame(MainWindow* win) {
    HwndSetFocus(win->hwndFrame);
}

bool ToolbarFrameIsVisible(MainWindow* win) {
    return HwndIsVisible(win->hwndFrame);
}

void ToolbarPostCommand(MainWindow* win, int cmdId) {
    LPARAM commandPoint = 0;
    if (cmdId >= CmdCreateAnnotFirst && cmdId <= CmdCreateAnnotLast && !CommandUsesPlacementMode(cmdId)) {
        Rect canvas = HwndClientRect(win->hwndCanvas);
        Point pt{canvas.dx / 2, canvas.dy / 2};
        commandPoint = MAKELPARAM(pt.x, pt.y);
    }
    HwndPostCommand(win->hwndFrame, cmdId, commandPoint);
}

void ToolbarSetHeight(MainWindow* win, int dy) {
    HWND hwnd = win ? win->hwndToolbar : nullptr;
    if (!hwnd || dy <= 0) {
        return;
    }
    Rect r = ChildPosWithinParent(hwnd);
    if (r.dy == dy) {
        return;
    }
    SetWindowPos(hwnd, nullptr, 0, 0, r.dx, dy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

//--- the native page-number edit

// Enter in either the page or chapter edit navigates; chaptered docs read
// both boxes and go by Location, single-chapter docs keep the page-label path
static void OnLocationEditChar(MainWindow* win, Edit::CharEvent* ev) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    switch ((Key)ev->c) {
        case Key::Enter: {
            DocController* ctrl = win->ctrl;
            if (ctrl->HasChapters()) {
                int chapter = win->chapterEdit ? ParseInt(win->chapterEdit->GetTextTemp()) : 1;
                int page = win->pageEdit ? ParseInt(win->pageEdit->GetTextTemp()) : 1;
                Location loc = ctrl->ClampLocation({chapter, page});
                ctrl->GoToLocation(loc, true);
            } else if (win->pageEdit) {
                TempStr s = win->pageEdit->GetTextTemp();
                int newPageNo = ctrl->GetPageByLabel(s);
                if (!ctrl->ValidPageNo(newPageNo)) {
                    ev->didHandle = true;
                    return;
                }
                ctrl->GoToPage(newPageNo, true);
            }
            HwndSetFocus(win->hwndFrame);
            // the overlay toolbar was kept up by the focus; now that
            // it's gone, let it hide again
            UpdateOverlayToolbarForMouse(win);
            ev->didHandle = true;
            return;
        }
        case Key::Escape:
            HwndSetFocus(win->hwndFrame);
            UpdateOverlayToolbarForMouse(win);
            ev->didHandle = true;
            return;
        case Key::Tab:
            AdvanceFocus(win);
            ev->didHandle = true;
            return;
        default:
            return;
    }
}

static int PageEditPadL() {
    return UiEdgeDx();
}

static int PageEditPadR() {
    return PageEditPadL() + DpiScale(4);
}

static Edit* ToolbarCreateLocationEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    Edit::CreateArgs args;
    args.parent = win->hwndToolbar;
    args.font = font;
    args.isRtl = IsUIRtl();
    // no WS_EX_CLIENTEDGE: a themed edit draws a blue bottom accent (Win11)
    args.withFrame = true;
    args.noTheme = true;
    args.numbersOnly = true;
    args.alignRight = true;
    args.selectAllOnFocus = true;
    // the box is as tall as the icons, so without this the digits would sit at
    // its top instead of on the same line as "Page:" and "/ N"
    args.centerTextVert = true;
    args.marginLeft = PageEditPadL();
    args.marginRight = PageEditPadR();
    auto* e = new Edit();
    e->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    e->Create(args);
    // the toolbar tree arranges itself right-to-left (HBox.rtl), so its bounds
    // are offsets from the physical left; don't let the RTL host mirror them
    e->mapRtlX = true;
    // #5949: fixed width, or the box would resize to every page label while
    // scrolling a document with named pages, shifting the icons next to it.
    // ideal == max pins GetIdealSize() to this width
    e->SetIdealWidthChars(6);
    e->SetMaxWidthChars(6);
    e->idealDy = iconDy;
    e->onChar = MkFunc1(OnLocationEditChar, win);
    return e;
}

Edit* ToolbarCreatePageEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    return ToolbarCreateLocationEdit(win, font, iconDy);
}

Edit* ToolbarCreateChapterEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    return ToolbarCreateLocationEdit(win, font, iconDy);
}

// no document: the find edit does nothing, so don't offer a text cursor
void ToolbarUpdateFindEditCursor(MainWindow* win) {
    LPWSTR cursorId = win->IsDocLoaded() ? nullptr : IDC_ARROW;
    if (win->findEdit) {
        win->findEdit->SetCursorId(cursorId);
    }
}

//--- the messages VirtHost doesn't model

// the native edit control asks its parent what colors to draw itself in
static bool OnCtlColor(MainWindow* win, VirtHostNativeMsg* ev) {
    LRESULT reflected = TryReflectMessages(win->hwndToolbar, ev->msg, ev->wp, ev->lp);
    if (reflected) {
        ev->res = reflected;
        return true;
    }
    if (ev->msg == WM_COMMAND) {
        return false;
    }
    HDC hdc = (HDC)ev->wp;
    SetTextColor(hdc, TbTextColor());
    SetBkColor(hdc, ThemeWindowControlBackgroundColor());
    if (IsCurrentThemeDefault() && !ThemeColorizeControls() && !ThemeUsesHighContrastColors()) {
        ev->res = (LRESULT)GetStockObject(WHITE_BRUSH);
    } else {
        ev->res = (LRESULT)win->brControlBgColor;
    }
    return true;
}

// with the tabs in the title bar the toolbar is part of the caption, so
// dragging an empty part of it moves the window and a double click maximizes it
static bool OnCaptionDrag(MainWindow* win, VirtHostNativeMsg* ev) {
    HWND hwnd = win->hwndToolbar;
    Point pt = {GET_X_LPARAM(ev->lp), GET_Y_LPARAM(ev->lp)};
    HWND childAtPoint = ChildWindowFromPoint(hwnd, ToPOINT(pt));
    bool overChild = childAtPoint && childAtPoint != hwnd;
    // layout bounds are physical-left; WM_LBUTTONDOWN x is mirrored on RTL
    Point hitPt = pt;
    UnmirrorRtl(hwnd, hitPt);
    VirtCtrl* hit = ToolbarItemFromPoint(win, hitPt);
    if (overChild || (hit && hit->id != 0 && hit->id != PageInfoId)) {
        return false;
    }
    HWND hwndFrame = GetAncestor(hwnd, GA_ROOT);
    if (ev->msg == WM_LBUTTONDBLCLK) {
        WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
        PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
    } else {
        ReleaseCapture();
        SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    }
    ev->res = 0;
    return true;
}

static void OnToolbarNativeMsg(MainWindow* win, VirtHostNativeMsg* ev) {
    switch (ev->msg) {
        case WM_COMMAND:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            ev->didHandle = OnCtlColor(win, ev);
            return;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            if (win->tabsInTitlebar) {
                ev->didHandle = OnCaptionDrag(win, ev);
            }
            return;
    }
}

void ToolbarSetNativeHooks(MainWindow* win, VirtHost* host) {
    host->onNativeMsg = MkFunc1(OnToolbarNativeMsg, win);
}

#endif
