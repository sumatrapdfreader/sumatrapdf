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
// Needs a PDF with a multi-level outline; uses one from the local bugs folder
// and skips cleanly if it isn't present (so tests/all.ts keeps going).

import { spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, tmpPath } from "./util";

// a PDF with a nested (multi-level) table of contents
const TOC_PDF = "C:\\Users\\kjk\\OneDrive\\!sumatra\\bugs\\bug-1352-merged_manuals-1.4.2.pdf";

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

  const ps1 = join(ROOT, "tests", "issue-1998.verify.ps1");
  const r = spawnSync(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", ps1, "-Exe", EXE, "-Pdf", pdf, "-AppData", appdata],
    { encoding: "utf8", timeout: 90_000 },
  );
  const out = (r.stdout || "") + (r.stderr || "");
  const m = out.match(/RESULT collapsed=(\d+) after=(\d+) hasSelection=(\d+)/);
  if (!m) {
    throw new Error(`could not read TOC tree state; output:\n${out}`);
  }
  const collapsed = parseInt(m[1], 10);
  const after = parseInt(m[2], 10);
  const hasSelection = m[3] === "1";
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
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
