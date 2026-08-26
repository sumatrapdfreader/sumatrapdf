// #6033: clicking the annotations list after focusing Contents must move
// Win32 focus back to the list so arrows navigate rows again.
//
// Run: bun tests/issue-6033.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, cmdId, runStandalone, tmpPath, waitForAnnotWindow } from "./util";
import {
  clientToScreen,
  enumChildWindows,
  getClassName,
  getFocusedHwnd,
  getWindowLong,
  getWindowRect,
  postMessage,
  sleep,
  WM_CLOSE,
  WM_KEYDOWN,
} from "./winapi";
import { clickAt, killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation";

const VK_DOWN = 0x28;
const GWL_STYLE = -16;
const ES_MULTILINE = 0x0004;
const kAnnotCount = 6;

function makePdf(): string {
  const annots: string[] = [];
  for (let i = 0; i < kAnnotCount; i++) {
    const y = 700 - i * 20;
    annots.push(`<< /Type /Annot /Subtype /Text /Rect [50 ${y} 70 ${y + 18}] /T (t${i}) /Contents (note ${i + 1}) >>`);
  }
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots.join(" ")}] >>`,
  ];
  return assemblePdf(objs);
}

function contentsEdit(annotWin: number): number {
  let found = 0;
  enumChildWindows(annotWin, (hwnd) => {
    if (getClassName(hwnd) === "Edit" && (getWindowLong(hwnd, GWL_STYLE) & ES_MULTILINE) !== 0) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function filterEdit(annotWin: number, contents: number): number {
  let found = 0;
  enumChildWindows(annotWin, (hwnd) => {
    if (hwnd !== contents && getClassName(hwnd) === "Edit" && (getWindowLong(hwnd, GWL_STYLE) & ES_MULTILINE) === 0) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

type Layout = {
  sel: number;
  n: number;
  contents: { x: number; y: number; dx: number; dy: number };
  raw: string;
};

function parseRect(s: string): { x: number; y: number; dx: number; dy: number } {
  const p = s.split(",").map((n) => parseInt(n, 10));
  return { x: p[0]!, y: p[1]!, dx: p[2]!, dy: p[3]! };
}

async function layout(client: ControlClient): Promise<Layout> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /sel=(-?\d+) n=(\d+) selCount=\d+ contents=(-?\d+,-?\d+,-?\d+,-?\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-6033: could not parse: ${raw}`);
      }
      return { sel: +m[1]!, n: +m[2]!, contents: parseRect(m[3]!), raw };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-6033: TestAnnotEditorLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6033: editor never ready: ${raw}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6033.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdEditAnnotations"));
    const annotWin = await waitForAnnotWindow(proc.pid!, frame);
    let st = await layout(client);
    if (st.n < 3) {
      throw new Error(`issue-6033: expected at least 3 annotations, got n=${st.n} (${st.raw})`);
    }
    const startSel = st.sel;
    if (startSel < 0) {
      throw new Error(`issue-6033: no list selection: ${st.raw}`);
    }

    const contents = contentsEdit(annotWin);
    if (!contents) {
      throw new Error("issue-6033: Contents edit not found");
    }
    await clickAt(contents, 12, 12, 200);
    if (getFocusedHwnd(annotWin) !== contents) {
      throw new Error(
        `issue-6033: click in Contents did not focus it (focus=${getFocusedHwnd(annotWin)}, contents=${contents})`,
      );
    }

    // list sits between the filter and Contents. Click the selected row there.
    const filter = filterEdit(annotWin, contents);
    const origin = clientToScreen(annotWin, 0, 0);
    const filterR = filter ? getWindowRect(filter) : { top: origin.y, bottom: origin.y + 8 };
    const listTop = filterR.bottom - origin.y;
    const listBot = st.contents.y;
    if (listBot - listTop < 16) {
      throw new Error(`issue-6033: no list hit area between filter and Contents (${listTop}..${listBot})`);
    }
    const listX = st.contents.x + Math.max(12, Math.floor(st.contents.dx / 4));
    const listY = listTop + 10;
    await clickAt(annotWin, listX, listY, 200);
    const focus = getFocusedHwnd(annotWin);
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
    const deadline = Date.now() + 5_000;
    for (;;) {
      st = await layout(client);
      if (st.sel !== startSel) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`issue-6033: Down after list click did not move selection (sel=${st.sel})`);
      }
      await sleep(40);
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-6033: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6033: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
