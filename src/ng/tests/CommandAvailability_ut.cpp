/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: new in this port. Orig has no test for the command visibility policy;
// here it is the one half of CommandAvailability that is live before the gpui
// menus exist (step 6), so it is worth pinning down.

#include "base/Base.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "Commands.h"
#include "CommandAvailability.h"

// must be last to over-write assert()
#include "base/tests/UtAssert.h"

static CommandVisibility MenuVis(int cmdId, const AppCommandCtx& ctx) {
    return GetCommandVisibility(cmdId, ctx, CommandSurface::Menu);
}

static CommandVisibility PaletteVis(int cmdId, const AppCommandCtx& ctx) {
    return GetCommandVisibility(cmdId, ctx, CommandSurface::Palette);
}

void CommandAvailability_UnitTests() {
    utassert(CmdWorksWithoutDocument(CmdOpenFile));
    utassert(CmdWorksWithoutDocument(CmdExit));
    utassert(CmdWorksWithoutDocument(CmdGoToHomePage));
    utassert(!CmdWorksWithoutDocument(CmdPrint));

    // no document: only the whitelist shows
    AppCommandCtx noDoc;
    utassert(MenuVis(CmdNone, noDoc) == CommandVisibility::Hide);
    utassert(MenuVis(CmdOpenFile, noDoc) == CommandVisibility::Show);
    utassert(MenuVis(CmdGoToHomePage, noDoc) == CommandVisibility::Show);
    utassert(PaletteVis(CmdGoToHomePage, noDoc) == CommandVisibility::Hide);
    utassert(MenuVis(CmdPrint, noDoc) == CommandVisibility::Hide);

    bool remove = false, disable = false;
    GetCommandIdState(&noDoc, CmdPrint, &remove, &disable);
    utassert(remove && !disable);

    AppCommandCtx doc;
    doc.isDocLoaded = true;

    // a command that needs a selection is disabled in a menu and dropped from
    // the palette, which has no room for dead entries
    utassert(MenuVis(CmdCopySelection, doc) == CommandVisibility::Disable);
    utassert(PaletteVis(CmdCopySelection, doc) == CommandVisibility::Hide);
    doc.hasSelection = true;
    utassert(MenuVis(CmdCopySelection, doc) == CommandVisibility::Show);
    utassert(MenuVis(CmdPrintSelection, doc) == CommandVisibility::Show);
    doc.hasTextSelection = true;
    utassert(MenuVis(CmdPrintSelection, doc) == CommandVisibility::Hide);
    doc.hasTextSelection = false;
    doc.allowsPrinting = false;
    utassert(MenuVis(CmdPrintSelection, doc) == CommandVisibility::Disable);
    doc.allowsPrinting = true;
    doc.hasSelection = false;

    // the palette doesn't list itself
    utassert(PaletteVis(CmdCommandPalette, doc) == CommandVisibility::Hide);
    utassert(PaletteVis(CmdInsertTextSnippet, doc) == CommandVisibility::Hide);

    // a table of contents decides the two commands that show it
    utassert(MenuVis(CmdToggleBookmarks, doc) == CommandVisibility::Hide);
    doc.hasToc = true;
    utassert(MenuVis(CmdToggleBookmarks, doc) == CommandVisibility::Show);
    utassert(MenuVis(CmdToggleTableOfContents, doc) == CommandVisibility::Show);

    utassert(PaletteVis(CmdAutoGenerateTOC, doc) == CommandVisibility::Hide);
    doc.engineKind = kindEngineMupdf;
    utassert(PaletteVis(CmdAutoGenerateTOC, doc) == CommandVisibility::Show);

    // chm / markdown have no page layout to change
    utassert(MenuVis(CmdRotateLeft, doc) == CommandVisibility::Show);
    doc.isChm = true;
    utassert(MenuVis(CmdRotateLeft, doc) == CommandVisibility::Hide);
    utassert(MenuVis(CmdZoomFitPage, doc) == CommandVisibility::Hide);
    doc.isChm = false;

    // free pan only applies to the fixed-page display model
    utassert(MenuVis(CmdToggleFreePan, doc) == CommandVisibility::Hide);
    doc.isFixedPage = true;
    utassert(MenuVis(CmdToggleFreePan, doc) == CommandVisibility::Show);

    utassert(PaletteVis(CmdMergePDF, doc) == CommandVisibility::Hide);
    doc.isPdf = true;
    utassert(PaletteVis(CmdMergePDF, doc) == CommandVisibility::Show);

    // read from cursor follows the other text-to-speech commands
    utassert(PaletteVis(CmdReadAloudFromCursorPosition, doc) == CommandVisibility::Hide);
    doc.ttsAvailable = true;
    doc.engineKind = kindEngineMupdf;
    utassert(PaletteVis(CmdReadAloudFromCursorPosition, doc) == CommandVisibility::Show);
    doc.isImageCollection = true;
    utassert(PaletteVis(CmdReadAloudFromCursorPosition, doc) == CommandVisibility::Hide);
    doc.isImageCollection = false;

    // annotations need an engine that supports them
    utassert(MenuVis(CmdSaveAnnotations, doc) == CommandVisibility::Hide);
    utassert(PaletteVis(CmdSignWithImage, doc) == CommandVisibility::Hide);
    CommandArg* snippetArg = NewStringArg(kCmdArgText, StrL("One\nTwo"));
    CustomCommand* snippet = CreateCustomCommand({}, CmdInsertTextSnippet, snippetArg, StrL("Two lines"));
    utassert(str::Eq(GetCommandStringArg(snippet, kCmdArgText, {}), StrL("One\nTwo")));
    utassert(PaletteVis(snippet->id, doc) == CommandVisibility::Hide);
    doc.supportsAnnots = true;
    utassert(MenuVis(CmdCreateAnnotImageFromClipboard, doc) == CommandVisibility::Disable);
    doc.clipboardReadAsync = true;
    utassert(MenuVis(CmdCreateAnnotImageFromClipboard, doc) == CommandVisibility::Show);
    doc.clipboardReadAsync = false;
    doc.clipboardHasImage = true;
    utassert(MenuVis(CmdCreateAnnotImageFromClipboard, doc) == CommandVisibility::Show);
    utassert(PaletteVis(CmdSignWithImage, doc) == CommandVisibility::Show);
    utassert(PaletteVis(snippet->id, doc) == CommandVisibility::Show);
    utassert(MenuVis(CmdSaveAnnotations, doc) == CommandVisibility::Disable);
    doc.hasUnsavedAnnotations = true;
    utassert(MenuVis(CmdSaveAnnotations, doc) == CommandVisibility::Show);

    ReportIf(gFirstCustomCommand != snippet);
    gFirstCustomCommand = snippet->next;
    snippet->next = nullptr;
    FreeCustomCommand(snippet);
}
