// #6036: Tab in the annotations window must walk the layout top-to-bottom
// (filter → list → Delete → Contents → …), not the other way.
//
// Run: bun tests/issue-6036.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control";
import { cmdId, runStandalone, tmpPath } from "./util";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getFocusedHwnd,
  getWindowLong,
  getWindowPid,
  getWindowText,
  postMessage,
  sleep,
  VK_TAB,
  WM_CLOSE,
  WM_KEYDOWN,
} from "./winapi";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation";

const GWL_STYLE = -16;
const ES_MULTILINE = 0x0004;

function makePdf(): string {
  const annot = `<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (t) /Contents (note) >>`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annot}] >>`,
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

function findAnnotWindow(pid: number, frame: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (hwnd === frame || getWindowPid(hwnd) !== pid) {
      return true;
    }
    if (getWindowText(hwnd).startsWith("Annotations")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitAnnotWindow(pid: number, frame: number): Promise<number> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const hwnd = findAnnotWindow(pid, frame);
    if (hwnd) {
      return hwnd;
    }
    if (Date.now() > deadline) {
      throw new Error("issue-6036: Annotations window did not open");
    }
    await sleep(50);
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

async function waitLayoutReady(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
    if (Number(res[0]) === 0) {
      return;
    }
    if (Number(res[0]) !== 2) {
      throw new Error(`issue-6036: TestAnnotEditorLayout failed: ${String(res[1])}`);
    }
    if (Date.now() > deadline) {
      throw new Error("issue-6036: editor never ready");
    }
    await sleep(50);
  }
}

async function pressTab(hwnd: number): Promise<void> {
  postMessage(hwnd, WM_KEYDOWN, VK_TAB, 0);
  await sleep(200);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6036.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdEditAnnotations"));
    const annotWin = await waitAnnotWindow(proc.pid!, frame);
    await waitLayoutReady(client);

    const { filter, contents } = findEdits(annotWin);
    if (!filter || !contents) {
      throw new Error(`issue-6036: missing edits filter=${filter} contents=${contents}`);
    }

    // the window opens with virtual focus on the list (host HWND)
    if (getFocusedHwnd(annotWin) !== annotWin) {
      throw new Error(`issue-6036: expected list/host focus on open, got ${getFocusedHwnd(annotWin)}`);
    }

    await pressTab(annotWin);
    let focus = getFocusedHwnd(annotWin);
    if (focus === filter) {
      throw new Error("issue-6036: Tab from the list went to the filter (order is inverted)");
    }
    // Delete is a virtual button, so HWND stays on the host; next Tab is Contents
    if (focus === annotWin) {
      await pressTab(annotWin);
      focus = getFocusedHwnd(annotWin);
    }
    if (focus !== contents) {
      throw new Error(`issue-6036: Tab should reach Contents going down, got ${focus} class=${getClassName(focus)}`);
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-6036: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6036: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
