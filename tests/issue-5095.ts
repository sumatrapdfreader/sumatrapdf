// Test for issue #5095: "Option to remove existing buttons from the toolbar".
//
// The toolbar could be given custom buttons but its built-in ones were fixed,
// in a fixed order. The `ToolbarCustomLayout` advanced setting now says which
// built-in buttons the toolbar has and in what order, e.g.
//
//     ToolbarCustomLayout = CmdFindFirst | CmdGoToPrevPage CmdGoToNextPage PageInfo
//
// Leaving a button out is how you hide it; empty (the default) is the standard
// layout.
//
// The toolbar is a Virt* tree (no ToolbarWindow32 HWND), so the test reads
// button order and visibility through -dbg-control TestToolbarButtons.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { cmdId, EXE, runStandalone, tmpPath, assemblePdf } from "./util.ts";

type Button = { visible: boolean; idx: number };

// the standard toolbar written out as a layout; docs/md/Customize-toolbar.md
// gives users this string as the starting point for their own
const DEFAULT_LAYOUT =
  "CmdOpenFile CmdPrint | PageInfo CmdGoToPrevPage CmdGoToNextPage | " +
  "CmdNavigateBack CmdNavigateForward | CmdReadAloud | " +
  "CmdZoomFitWidthAndContinuous CmdZoomFitPageAndSinglePage CmdRotateLeft CmdRotateRight " +
  "CmdZoomOut CmdZoomIn | CmdFindFirst | CmdToggleEditPDF";

function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
  ];
  return assemblePdf(objs);
}

type DumpBtn = { idx: number; cmd: number; hidden: boolean };

function parseButtons(dump: string): DumpBtn[] {
  const buttons: DumpBtn[] = [];
  for (const line of dump.split("\n")) {
    const m = /^idx=(\d+) cmd=(-?\d+) hidden=(\d)/.exec(line);
    if (m) {
      buttons.push({ idx: +m[1]!, cmd: +m[2]!, hidden: m[3] === "1" });
    }
  }
  return buttons;
}

function buttonsByName(dump: string, cmds: string[]): Map<string, Button> {
  const parsed = parseButtons(dump);
  const res = new Map<string, Button>();
  for (const name of cmds) {
    const id = cmdId(name);
    const b = parsed.find((x) => x.cmd === id);
    res.set(name, b ? { visible: !b.hidden, idx: b.idx } : { visible: false, idx: -1 });
  }
  return res;
}

async function readToolbar(appdata: string, pdf: string, cmds: string[]): Promise<Map<string, Button>> {
  return withControlledSumatra(
    EXE,
    async (client) => {
      const deadline = Date.now() + 15_000;
      for (;;) {
        const res = await client.request(ControlCommand.TestToolbarButtons, []);
        const exitCode = res[0] as number;
        const dump = String(res[1] ?? "");
        if (exitCode === 0 && parseButtons(dump).length > 0) {
          return buttonsByName(dump, cmds);
        }
        if (exitCode !== 0 && exitCode !== 1 && exitCode !== 2) {
          throw new Error(`issue-5095: TestToolbarButtons failed: ${dump.trim()}`);
        }
        if (Date.now() > deadline) {
          throw new Error(`issue-5095: could not read the document toolbar: ${dump.trim() || "empty"}`);
        }
        await new Promise((r) => setTimeout(r, 100));
      }
    },
    ["-appdata", appdata, pdf],
  );
}

function writeSettings(dir: string, layout: string) {
  const lines = [
    `RestoreSession = false`,
    `ShowStartPage = false`,
    `CheckForUpdates = false`,
    layout ? `ToolbarCustomLayout = ${layout}` : ``,
    ``,
  ];
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), lines.join("\n"));
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5095.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const appdata = tmpPath("issue-5095-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  const cmds = ["CmdOpenFile", "CmdPrint", "CmdGoToPrevPage", "CmdGoToNextPage", "CmdFindFirst", "CmdToggleEditPDF"];

  // the standard layout: everything is there, Open comes before Find
  writeSettings(appdata, "");
  const std = await readToolbar(appdata, pdf, cmds);
  for (const name of cmds) {
    if (!std.get(name)!.visible) {
      throw new Error(`${name} has no visible toolbar button in the standard layout`);
    }
  }
  if (std.get("CmdOpenFile")!.idx >= std.get("CmdFindFirst")!.idx) {
    throw new Error("expected Open to come before Find in the standard layout");
  }

  // a custom layout: Find first, then the page buttons; no Open, no Print
  writeSettings(appdata, "CmdFindFirst | CmdGoToNextPage CmdGoToPrevPage");
  const custom = await readToolbar(appdata, pdf, cmds);
  for (const name of ["CmdOpenFile", "CmdPrint"]) {
    if (custom.get(name)!.visible) {
      throw new Error(`${name} is on the toolbar although the layout leaves it out`);
    }
  }
  for (const name of ["CmdFindFirst", "CmdGoToNextPage", "CmdGoToPrevPage"]) {
    if (!custom.get(name)!.visible) {
      throw new Error(`${name} is missing although the layout lists it`);
    }
  }
  const find = custom.get("CmdFindFirst")!.idx;
  const next = custom.get("CmdGoToNextPage")!.idx;
  const prev = custom.get("CmdGoToPrevPage")!.idx;
  if (!(find < next && next < prev)) {
    throw new Error(`the buttons are not in the order the layout asks for: Find=${find}, Next=${next}, Prev=${prev}`);
  }
  // the layout documented as "the standard one, written out" must really be it
  writeSettings(appdata, DEFAULT_LAYOUT);
  const explicit = await readToolbar(appdata, pdf, cmds);
  for (const name of cmds) {
    const a = std.get(name)!;
    const b = explicit.get(name)!;
    if (a.visible !== b.visible || a.idx !== b.idx) {
      throw new Error(
        `the documented default layout doesn't reproduce the standard one: ${name} is ` +
          `${a.visible ? `at ${a.idx}` : "hidden"} by default but ${b.visible ? `at ${b.idx}` : "hidden"} with it`,
      );
    }
  }
  console.log(`  ToolbarCustomLayout orders and hides buttons: Find=${find} < Next=${next} < Prev=${prev} ✓`);
  console.log(`  the documented default layout reproduces the standard toolbar ✓`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
