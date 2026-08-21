// Test for issue #1998: "Expand to current page in toc tree".
//
// CmdExpandToCurrentPage expands the table-of-contents tree down to the entry
// matching the current page and selects it (like Explorer's "Expand to current
// folder"). Implemented in src/TableOfContents.cpp (ExpandTocToCurrentPage),
// wired to the TOC context menu and the global command dispatcher.
//
// The test opens a PDF with a nested TOC, navigates to the last page, collapses
// the whole tree, then invokes the command via WM_COMMAND and reads the tree
// state with cross-process TreeView messages. After the command, the path to the
// (deeply nested) last-page entry must be expanded -- so more tree rows are
// visible than when fully collapsed -- and an item must be selected. Reverting
// the fix leaves the tree collapsed with no expansion, so the test fails.
//
// Drives the app from Bun via FFI (tests/winapi.ts). Needs a PDF with a
// multi-level outline; uses one from the local bugs folder and skips cleanly if
// it isn't present (so tests/run-almost-all.ts keeps going).

import { copyFileSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, tmpPath } from "./util";
import { launchControlled, sendCommandSync, killAndWait } from "./win-automation";
import { collapseTreeRoots, countVisibleTreeRows, findChildWindow, sleep, treeGetSelection } from "./winapi";

// a PDF with a nested (multi-level) table of contents
const TOC_PDF = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\bug-1352-merged_manuals-1.4.2.pdf";

// command ids, looked up by name from src/Commands.h (the numeric values shift
// whenever commands are added/removed, so never hardcode them)
const CmdGoToLastPage = cmdId("CmdGoToLastPage");
const CmdExpandToCurrentPage = cmdId("CmdExpandToCurrentPage");

const SETTINGS = [
  `ShowToc = true`,
  `ShowFavorites = false`,
  `DefaultDisplayMode = continuous`,
  `DefaultZoom = fit page`,
  `Scrollbars = windows`,
  `RestoreSession = false`,
  `ShowStartPage = false`,
  `CheckForUpdates = false`,
  ``,
].join("\n");

export async function testit(): Promise<void> {
  if (!existsSync(TOC_PDF)) {
    console.log(`  SKIP: TOC test PDF not found: ${TOC_PDF}`);
    return;
  }

  // copy the PDF out of OneDrive to a local path -- opening a cloud-backed file
  // can block/delay window creation (hydration), which breaks the automation
  const pdf = tmpPath("issue-1998.pdf");
  copyFileSync(TOC_PDF, pdf);

  const appdata = tmpPath("issue-1998-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  // kill stale dev-build instances so reuse-instance can't forward our launch
  // to an old window (which would leave our process window-less)
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();

    // wait for the TOC tree to load with items
    const deadline = Date.now() + 8000;
    let tree = 0;
    while (Date.now() < deadline) {
      tree = findChildWindow(frame, "SysTreeView32");
      if (tree && countVisibleTreeRows(tree) > 0) {
        break;
      }
      await sleep(40);
    }
    if (!tree) {
      throw new Error("could not find the TOC tree window");
    }

    // go to the last page (deep in the document, under nested TOC entries)
    sendCommandSync(frame, CmdGoToLastPage);
    await client.waitForRenderIdle();

    // collapse the whole tree, then measure
    collapseTreeRoots(tree);
    const collapsed = countVisibleTreeRows(tree);

    // invoke the command under test
    sendCommandSync(frame, CmdExpandToCurrentPage);
    const afterDeadline = Date.now() + 3000;
    let after = collapsed;
    while (Date.now() < afterDeadline) {
      after = countVisibleTreeRows(tree);
      if (after > collapsed && treeGetSelection(tree) !== 0n) {
        break;
      }
      await sleep(30);
    }
    const hasSelection = treeGetSelection(tree) !== 0n;

    console.log(`  collapsed rows=${collapsed}, after-expand rows=${after}, hasSelection=${hasSelection}`);

    if (collapsed <= 0) {
      throw new Error(`baseline collapsed tree has no rows (${collapsed}) -- test setup wrong`);
    }
    if (!hasSelection) {
      throw new Error(`CmdExpandToCurrentPage did not select a TOC entry`);
    }
    if (after <= collapsed) {
      throw new Error(
        `CmdExpandToCurrentPage did not expand the tree to the current page ` +
          `(visible rows ${collapsed} -> ${after}; expected an increase)`,
      );
    }
    console.log(`  expanded TOC to current page: ${collapsed} -> ${after} visible rows ✓`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
