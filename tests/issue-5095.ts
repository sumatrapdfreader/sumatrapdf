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
// The test checks the standard layout first, then a custom one that reverses
// two buttons and drops two others.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, tmpPath } from "./util";
import {
  enumChildWindows,
  getClassName,
  sleep,
  tbGetButtonIndex,
  tbGetState,
  tbIsButtonVisible,
  waitForTopWindow,
} from "./winapi";

type Button = { visible: boolean; idx: number };

// the standard toolbar written out as a layout; docs/md/Customize-toolbar.md
// gives users this string as the starting point for their own
const DEFAULT_LAYOUT =
  "CmdOpenFile CmdPrint PageInfo CmdGoToPrevPage CmdGoToNextPage | " +
  "CmdNavigateBack CmdNavigateForward | CmdReadAloud | " +
  "CmdZoomFitWidthAndContinuous CmdZoomFitPageAndSinglePage CmdRotateLeft CmdRotateRight " +
  "CmdZoomOut CmdZoomIn | CmdFindFirst";

function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

// state and position in the toolbar of the given commands' buttons
async function readToolbar(appdata: string, pdf: string, cmds: string[]): Promise<Map<string, Button>> {
  const proc = Bun.spawn([EXE, "-for-testing", "-appdata", appdata, pdf], { stdout: "ignore", stderr: "ignore" });
  try {
    const frame = await waitForTopWindow(proc.pid, "SUMATRA_PDF_FRAME");
    if (!frame) {
      throw new Error("SumatraPDF main window did not appear");
    }
    await sleep(2000);
    // the toolbar lives inside a rebar, and there is a second one for the menu
    // bar, so find the one that knows the page-navigation commands
    const toolbars: number[] = [];
    const collect = (parent: number) => {
      enumChildWindows(parent, (hwnd) => {
        if (getClassName(hwnd) === "ToolbarWindow32") {
          toolbars.push(hwnd);
        }
        collect(hwnd);
        return true;
      });
    };
    collect(frame);
    const probeCmd = cmdId("CmdGoToNextPage");
    const toolbar = toolbars.find((hwnd) => tbGetState(hwnd, probeCmd) >= 0);
    if (!toolbar) {
      throw new Error(`could not find the document toolbar (${toolbars.length} toolbar windows)`);
    }
    const res = new Map<string, Button>();
    for (const name of cmds) {
      const id = cmdId(name);
      const visible = tbIsButtonVisible(toolbar, id);
      res.set(name, { visible, idx: tbGetButtonIndex(toolbar, id) });
    }
    return res;
  } finally {
    proc.kill();
    await sleep(400);
  }
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

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);

  const cmds = ["CmdOpenFile", "CmdPrint", "CmdGoToPrevPage", "CmdGoToNextPage", "CmdFindFirst"];

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
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
