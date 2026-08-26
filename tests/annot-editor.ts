// Annotation editor, one Sumatra session. Covers:
//
//   #3769 list height stays put when the window grows or the type changes
//   #5834 Contents stays 6 lines
//   annot-icon-over-contents: Icon sits below Contents, not at (0,0)
//   #5974 editor HWND fits the work area
//   #5975 Home / End / PageDown / Down
//   #5976 Del removes a multi-selection
//   #6033 click the list after Contents so arrows work again
//   #6036 Tab walks filter → list → Delete → Contents
//   annot-delete-redraw: delete does not auto-select; the page redraws
//
// Isolated on purpose (custom -appdata, two launches, EscToExit, overlay
// scrollbars, placement tools): issue-4494, annotation-editor-reload,
// issue-5934, overlay-scrollbar-annot-zorder, and the *-placement tests.

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, pollUntil, runStandalone, tmpPath, waitForAnnotWindow } from "./util.ts";
import {
  captureWindowToPng,
  clientToScreen,
  enumChildWindows,
  getClassName,
  getClientRect,
  getFocusedHwnd,
  getWindowLong,
  getWindowRect,
  getWorkArea,
  packCoords,
  postMessage,
  sendMessage,
  VK_DOWN,
  VK_END,
  VK_ESCAPE,
  VK_HOME,
  WM_CLOSE,
  WM_COMMAND,
  WM_KEYDOWN,
} from "./winapi.ts";
import {
  clickAt,
  findCanvas,
  killAndWait,
  launchControlled,
  pressTab,
  sendCommand,
  sendCommandSync,
  waitForExit,
} from "./win-automation.ts";

const VK_NEXT = 0x22;
const VK_DELETE = 0x2e;
const GWL_STYLE = -16;
const ES_MULTILINE = 0x0004;
const kAnnotCount = 20;

type Rect = { x: number; y: number; dx: number; dy: number };

type Layout = {
  windowDy: number;
  listDy: number;
  contentsDy: number;
  sel: number;
  n: number;
  selCount: number;
  contents: Rect;
  iconVis: number;
  icon: Rect;
  raw: string;
};

function makePdf(): string {
  const contents = Array.from({ length: 12 }, (_, i) => `annotation line ${i + 1}`).join("\\n");
  const annots: string[] = [];
  annots.push(`<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (t1) /Contents (${contents}) /Name /Comment >>`);
  annots.push(
    `<< /Type /Annot /Subtype /FreeText /Rect [80 650 220 690] /Contents (free text) /DA (/Helv 12 Tf 0 0 0 rg) >>`,
  );
  for (let i = 2; i < kAnnotCount; i++) {
    const y = 640 - i * 12;
    annots.push(`<< /Type /Annot /Subtype /Highlight /Rect [50 ${y} 200 ${y + 10}] /Contents (hl ${i}) >>`);
  }
  return assemblePdf([
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots.join(" ")}] >>`,
  ]);
}

function parseRect(s: string): Rect {
  const p = s.split(",").map((n) => parseInt(n, 10));
  return { x: p[0]!, y: p[1]!, dx: p[2]!, dy: p[3]! };
}

function intersects(a: Rect, b: Rect): boolean {
  return a.x < b.x + b.dx && a.x + a.dx > b.x && a.y < b.y + b.dy && a.y + a.dy > b.y;
}

function parseLayout(raw: string): Layout {
  const m =
    /windowDy=(\d+) listDy=(\d+) contentsDy=(\d+).*sel=(-?\d+) n=(\d+) selCount=(\d+) contents=(-?\d+,-?\d+,-?\d+,-?\d+) iconVis=(\d+) icon=(-?\d+,-?\d+,-?\d+,-?\d+)/.exec(
      raw,
    );
  if (!m) {
    throw new Error(`annot-editor: could not parse layout: ${raw}`);
  }
  return {
    windowDy: +m[1]!,
    listDy: +m[2]!,
    contentsDy: +m[3]!,
    sel: +m[4]!,
    n: +m[5]!,
    selCount: +m[6]!,
    contents: parseRect(m[7]!),
    iconVis: +m[8]!,
    icon: parseRect(m[9]!),
    raw,
  };
}

async function annotLayout(client: ControlClient, clientDy = 0, selectItem = 0, selectLast = 0): Promise<Layout> {
  const deadline = Date.now() + 15_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [clientDy, selectItem, selectLast]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      return parseLayout(raw);
    }
    if (exitCode !== 2) {
      throw new Error(`annot-editor: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-editor: editor never ready: ${raw}`);
    }
    await Bun.sleep(50);
  }
}

async function waitSel(client: ControlClient, want: number, what: string): Promise<Layout> {
  return pollUntil(
    () => annotLayout(client),
    (st) => st.sel === want,
    {
      timeoutMs: 5_000,
      intervalMs: 40,
      error: (st) => `annot-editor: ${what} (sel=${st.sel}, want=${want}, n=${st.n})`,
    },
  );
}

async function waitN(client: ControlClient, want: number, what: string): Promise<Layout> {
  return pollUntil(
    () => annotLayout(client),
    (st) => st.n === want,
    {
      timeoutMs: 5_000,
      intervalMs: 40,
      error: (st) => `annot-editor: ${what} (n=${st.n}, want=${want})`,
    },
  );
}

function assertFitsWorkArea(hwnd: number, what: string): void {
  const wa = getWorkArea();
  const r = getWindowRect(hwnd);
  const winDx = r.right - r.left;
  const winDy = r.bottom - r.top;
  const workDx = wa.right - wa.left;
  const workDy = wa.bottom - wa.top;
  if (winDx > workDx || winDy > workDy) {
    throw new Error(
      `issue-5974: ${what} ${winDx}x${winDy} is larger than the work area ${workDx}x${workDy} ` +
        `(win ${r.left},${r.top} work ${wa.left},${wa.top}-${wa.right},${wa.bottom})`,
    );
  }
}

function findEdits(annotWin: number): { filter: number; contents: number } {
  let filter = 0;
  let contents = 0;
  enumChildWindows(annotWin, (hwnd) => {
    if (getClassName(hwnd) !== "Edit") {
      return true;
    }
    if ((getWindowLong(hwnd, GWL_STYLE) & ES_MULTILINE) !== 0) {
      contents = hwnd;
    } else {
      filter = hwnd;
    }
    return true;
  });
  return { filter, contents };
}

function parseStampScreens(dump: string): Rect[] {
  const out: Rect[] = [];
  const seen = new Set<string>();
  for (const line of dump.split("\n")) {
    const m = /type=Stamp .* screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(line);
    if (!m) {
      continue;
    }
    const r = { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
    const k = `${r.x},${r.y},${r.dx},${r.dy}`;
    if (seen.has(k)) {
      continue;
    }
    seen.add(k);
    out.push(r);
  }
  return out;
}

function rectContains(r: Rect, x: number, y: number): boolean {
  return x >= r.x && x < r.x + r.dx && y >= r.y && y < r.y + r.dy;
}

function stampHit(r: Rect): { x: number; y: number } {
  return { x: r.x + Math.floor(r.dx / 2), y: r.y + Math.floor(r.dy / 2) };
}

function countRedPixels(png: string): number {
  const p = png.split("\\").join("\\\\");
  const ps = `Add-Type -AssemblyName System.Drawing; $b=[System.Drawing.Bitmap]::FromFile('${p}'); $n=0; for($y=0;$y -lt $b.Height;$y+=2){for($x=0;$x -lt $b.Width;$x+=2){$c=$b.GetPixel($x,$y); if($c.R -gt 180 -and $c.G -lt 80 -and $c.B -lt 80){$n++}}}; $b.Dispose(); Write-Output $n`;
  const r = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  return parseInt(r.stdout.toString().trim(), 10) || 0;
}

function assertNoSelection(st: Layout, action: string): void {
  if (st.sel !== -1 || st.selCount !== 0) {
    throw new Error(`annot-delete-redraw: ${action} selected another annotation (sel=${st.sel} count=${st.selCount})`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-editor");
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "annots.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdEditAnnotations"));
    const annotWin = await waitForAnnotWindow(proc.pid!, frame);
    let st = await annotLayout(client);
    if (st.n !== kAnnotCount) {
      throw new Error(`annot-editor: expected ${kAnnotCount} annotations, got n=${st.n} (${st.raw})`);
    }

    // #5974: clamp to the work area. Do this before TestAnnotEditorLayout
    // forces a 1000px client height (that size is the test, not the app).
    assertFitsWorkArea(annotWin, "initial Annotations window");
    await annotLayout(client, 0, 1);
    assertFitsWorkArea(annotWin, "Annotations window after selecting an annotation");
    console.log("  #5974 editor fits the work area ✓");

    // #5975: keys must work as soon as the window is open (no click).
    postMessage(annotWin, WM_KEYDOWN, VK_HOME, 0);
    st = await waitSel(client, 0, "issue-5975 Home on open");
    postMessage(annotWin, WM_KEYDOWN, VK_DOWN, 0);
    st = await waitSel(client, 1, "issue-5975 Down");
    postMessage(annotWin, WM_KEYDOWN, VK_END, 0);
    st = await waitSel(client, st.n - 1, "issue-5975 End");
    postMessage(annotWin, WM_KEYDOWN, VK_HOME, 0);
    st = await waitSel(client, 0, "issue-5975 Home");
    postMessage(annotWin, WM_KEYDOWN, VK_NEXT, 0);
    st = await annotLayout(client);
    if (st.sel <= 0) {
      throw new Error(`issue-5975: PageDown did not move down: ${st.raw}`);
    }
    console.log("  #5975 Home/Down/End/PageDown ✓");

    // #6036: Tab walks down (list → Delete → Contents), not up to the filter.
    postMessage(annotWin, WM_KEYDOWN, VK_HOME, 0);
    await waitSel(client, 0, "issue-6036 Home before Tab");
    if (getFocusedHwnd(annotWin) !== annotWin) {
      throw new Error(`issue-6036: expected list/host focus, got ${getFocusedHwnd(annotWin)}`);
    }
    const edits = findEdits(annotWin);
    if (!edits.filter || !edits.contents) {
      throw new Error(`issue-6036: missing edits filter=${edits.filter} contents=${edits.contents}`);
    }
    await pressTab(annotWin);
    let focus = getFocusedHwnd(annotWin);
    if (focus === edits.filter) {
      throw new Error("issue-6036: Tab from the list went to the filter (order is inverted)");
    }
    if (focus === annotWin) {
      await pressTab(annotWin);
      focus = getFocusedHwnd(annotWin);
    }
    if (focus !== edits.contents) {
      throw new Error(`issue-6036: Tab should reach Contents going down, got ${focus} class=${getClassName(focus)}`);
    }
    console.log("  #6036 Tab order list → Delete → Contents ✓");

    // #3769 / #5834: list and Contents heights are fixed; extra window is gap.
    const short = await annotLayout(client, 560, 0);
    const tall = await annotLayout(client, 1000, 0);
    if (Math.abs(tall.listDy - short.listDy) > 8) {
      throw new Error(
        `issue-3769: list height should not follow the window: ${short.listDy}px at ${short.windowDy} vs ` +
          `${tall.listDy}px at ${tall.windowDy} (${short.raw} / ${tall.raw})`,
      );
    }
    if (short.contentsDy < 60) {
      throw new Error(`issue-5834: Contents box is too short for 6 lines: ${short.contentsDy}px (${short.raw})`);
    }
    if (Math.abs(tall.contentsDy - short.contentsDy) > 8) {
      throw new Error(
        `issue-5834: Contents height should not follow the window: ${short.contentsDy}px at 560 vs ` +
          `${tall.contentsDy}px at 1000 (${short.raw} / ${tall.raw})`,
      );
    }
    const text = await annotLayout(client, 720, 1);
    const freeText = await annotLayout(client, 720, 2);
    if (Math.abs(text.listDy - freeText.listDy) > 4) {
      throw new Error(
        `issue-3769: list height changed when navigating: Text ${text.listDy}px vs FreeText ${freeText.listDy}px ` +
          `(${text.raw} / ${freeText.raw})`,
      );
    }
    if (Math.abs(text.contentsDy - freeText.contentsDy) > 8) {
      throw new Error(
        `issue-3769: Contents height changed when navigating: Text ${text.contentsDy}px vs FreeText ${freeText.contentsDy}px ` +
          `(${text.raw} / ${freeText.raw})`,
      );
    }
    console.log(
      `  #3769/#5834 list ${short.listDy}px Contents ${short.contentsDy}px at ${short.windowDy}/${tall.windowDy} ✓`,
    );

    // Icon dropdown for a Text annot sits below Contents, not at the origin.
    const hl = await annotLayout(client, 0, 3);
    if (hl.iconVis !== 0) {
      throw new Error(`annot-icon-over-contents: Highlight should hide Icon: ${hl.raw}`);
    }
    const textIcon = await annotLayout(client, 0, 1);
    if (textIcon.iconVis !== 1) {
      throw new Error(`annot-icon-over-contents: Text should show Icon: ${textIcon.raw}`);
    }
    if (textIcon.icon.dx <= 0 || textIcon.icon.dy <= 0) {
      throw new Error(`annot-icon-over-contents: Icon dropdown has no size: ${textIcon.raw}`);
    }
    if (textIcon.icon.x < 8 && textIcon.icon.y < 8) {
      throw new Error(`annot-icon-over-contents: Icon dropdown still at origin over Contents: ${textIcon.raw}`);
    }
    if (intersects(textIcon.icon, textIcon.contents)) {
      throw new Error(`annot-icon-over-contents: Icon dropdown overlaps Contents: ${textIcon.raw}`);
    }
    if (textIcon.icon.y < textIcon.contents.y + textIcon.contents.dy) {
      throw new Error(`annot-icon-over-contents: Icon dropdown is not below Contents: ${textIcon.raw}`);
    }
    console.log("  Icon dropdown below Contents ✓");

    // #6033: click the list after Contents so Down moves rows again.
    st = await annotLayout(client);
    const startSel = st.sel;
    if (startSel < 0) {
      throw new Error(`issue-6033: no list selection: ${st.raw}`);
    }
    const { filter, contents } = findEdits(annotWin);
    if (!contents) {
      throw new Error("issue-6033: Contents edit not found");
    }
    await clickAt(contents, 12, 12, 200);
    if (getFocusedHwnd(annotWin) !== contents) {
      throw new Error(
        `issue-6033: click in Contents did not focus it (focus=${getFocusedHwnd(annotWin)}, contents=${contents})`,
      );
    }
    const origin = clientToScreen(annotWin, 0, 0);
    const filterR = filter ? getWindowRect(filter) : { top: origin.y, bottom: origin.y + 8 };
    const listTop = filterR.bottom - origin.y;
    const listBot = st.contents.y;
    if (listBot - listTop < 16) {
      throw new Error(`issue-6033: no list hit area between filter and Contents (${listTop}..${listBot})`);
    }
    const listX = st.contents.x + Math.max(12, Math.floor(st.contents.dx / 4));
    await clickAt(annotWin, listX, listTop + 10, 200);
    focus = getFocusedHwnd(annotWin);
    if (focus === contents) {
      throw new Error("issue-6033: click on the list left focus in Contents");
    }
    if (getClassName(focus) === "Edit") {
      throw new Error(`issue-6033: click on the list left focus in an Edit (${focus})`);
    }
    if (focus !== annotWin) {
      throw new Error(
        `issue-6033: expected list/host focus after list click, got ${focus} class=${getClassName(focus)}`,
      );
    }
    postMessage(annotWin, WM_KEYDOWN, VK_DOWN, 0);
    await pollUntil(
      () => annotLayout(client),
      (next) => next.sel !== startSel,
      {
        timeoutMs: 5_000,
        intervalMs: 40,
        error: (next) => `issue-6033: Down after list click did not move selection (sel=${next.sel})`,
      },
    );
    console.log("  #6033 list click restores arrow navigation ✓");

    // #5976: Del removes a Shift-range, then every selected row.
    st = await annotLayout(client, 0, 1, 3);
    if (st.selCount !== 3) {
      throw new Error(`issue-5976: range select 1-3: selCount=${st.selCount} (${st.raw})`);
    }
    postMessage(annotWin, WM_KEYDOWN, VK_DELETE, 0);
    st = await waitN(client, kAnnotCount - 3, "issue-5976 Del after range select");
    st = await annotLayout(client, 0, -1);
    if (st.selCount !== st.n || st.n === 0) {
      throw new Error(`issue-5976: Ctrl+A equivalent: selCount=${st.selCount} n=${st.n} (${st.raw})`);
    }
    postMessage(annotWin, WM_KEYDOWN, VK_DELETE, 0);
    st = await waitN(client, 0, "issue-5976 Del after select all");
    console.log("  #5976 Del removes a range and then every selected annotation ✓");

    // annot-delete-redraw: three stamps; cursor-delete keeps the list
    // selection; command-delete and editor Del do not pick a replacement.
    // Place in the middle of the canvas so fit-page letterboxing cannot
    // drop the points in the margin (80,80 did on a quadrant window).
    const canvas = findCanvas(frame);
    const cr = getClientRect(canvas);
    const stampPts = [
      { x: Math.floor(cr.right * 0.35), y: Math.floor(cr.bottom * 0.3) },
      { x: Math.floor(cr.right * 0.35), y: Math.floor(cr.bottom * 0.65) },
      { x: Math.floor(cr.right * 0.65), y: Math.floor(cr.bottom * 0.3) },
    ];
    for (const pt of stampPts) {
      sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(pt.x, pt.y));
      await Bun.sleep(200);
    }
    await client.waitForRenderIdle();
    const nBefore = (await waitN(client, 3, "annot-delete-redraw stamps appeared")).n;

    const dumpBefore = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    const stampsBefore = parseStampScreens(dumpBefore);
    if (stampsBefore.length < 3) {
      throw new Error(`annot-delete-redraw: expected 3 stamp rects, got ${stampsBefore.length}\n${dumpBefore}`);
    }
    // Last created stamp is selected; delete a different one under the cursor.
    const cursorStamp = stampsBefore[0]!;
    const cursorHit = stampHit(cursorStamp);
    sendMessage(frame, WM_COMMAND, cmdId("CmdDeleteAnnotation"), packCoords(cursorHit.x, cursorHit.y));
    const afterCursorDelete = await waitN(client, nBefore - 1, "annot-delete-redraw cursor delete");
    const dumpAfter = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    if (parseStampScreens(dumpAfter).some((r) => rectContains(r, cursorHit.x, cursorHit.y))) {
      throw new Error(`annot-delete-redraw: stamp under cursor was not deleted\n${dumpAfter}`);
    }
    if (afterCursorDelete.sel < 0) {
      throw new Error("annot-delete-redraw: deleting another annot cleared the list selection");
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdDeleteAnnotation"), 0);
    const afterCommandDelete = await waitN(client, nBefore - 2, "annot-delete-redraw command delete");
    assertNoSelection(afterCommandDelete, "command deletion");

    const beforeEditorDelete = await annotLayout(client, 0, 1);
    if (beforeEditorDelete.sel < 0 || beforeEditorDelete.selCount !== 1) {
      throw new Error(`annot-delete-redraw: failed to select an annotation (${beforeEditorDelete.raw})`);
    }
    await client.waitForRenderIdle();
    const beforePng = join(dir, "before-delete.png");
    if (!captureWindowToPng(canvas, beforePng)) {
      throw new Error("annot-delete-redraw: capture before delete failed");
    }
    const redBefore = countRedPixels(beforePng);
    if (redBefore < 20) {
      throw new Error(`annot-delete-redraw: stamps not visible before delete (red=${redBefore})`);
    }
    postMessage(annotWin, WM_KEYDOWN, VK_DELETE, 0);
    const afterDelete = await waitN(client, nBefore - 3, "annot-delete-redraw editor Del");
    assertNoSelection(afterDelete, "editor deletion");
    await client.waitForRenderIdle();
    const afterPng = join(dir, "after-delete.png");
    if (!captureWindowToPng(canvas, afterPng)) {
      throw new Error("annot-delete-redraw: capture after delete failed");
    }
    const redAfter = countRedPixels(afterPng);
    if (redAfter >= redBefore * 0.5) {
      throw new Error(
        `annot-delete-redraw: page still shows deleted stamp (red before=${redBefore} after=${redAfter})`,
      );
    }
    console.log("  delete does not auto-select; page redraws ✓");

    postMessage(annotWin, WM_KEYDOWN, VK_ESCAPE, 0);
    sendCommandSync(frame, cmdId("CmdDiscardChanges"));
    await client.waitForRenderIdle();
    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("annot-editor: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("annot-editor: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
