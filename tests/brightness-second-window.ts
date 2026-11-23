// Brightness / Contrast opened in the second of two windows showing the same
// file must adjust that window's tab. It used to find the first window's tab,
// see it belonged to another window and do nothing.
//
// Run: bun tests/brightness-second-window.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, makeBlankPdf, runStandalone, tmpPath } from "./util.ts";
import { FRAME_CLASS, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import {
  ensureModifierKeysUp,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  postMessage,
  sleep,
  VK_RETURN,
  VK_RIGHT,
  WM_CHAR,
  WM_COMMAND,
  WM_KEYDOWN,
} from "./winapi.ts";

const DIALOG_TITLE = "Brightness / Contrast";

function findWindow(pid: number, match: (hwnd: number) => boolean): number[] {
  const res: number[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && match(hwnd)) {
      res.push(hwnd);
    }
    return true;
  });
  return res;
}

async function waitFor<T>(what: string, get: () => T | null, ms = 8000): Promise<T> {
  const deadline = Date.now() + ms;
  while (Date.now() < deadline) {
    const v = get();
    if (v !== null) {
      return v;
    }
    await sleep(100);
  }
  throw new Error(`brightness-second-window: ${what}`);
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP brightness-second-window: drives the dialog with window messages");
    return;
  }
  const pdf = tmpPath("brightness-second-window.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdDuplicateInNewWindow"));
    const frame2 = await waitFor("no second window", () => {
      const f = findWindow(pid, (h) => getClassName(h) === FRAME_CLASS && h !== frame);
      return f.length > 0 ? f[0]! : null;
    });
    await client.waitForRenderIdle();

    postMessage(frame2, WM_COMMAND, cmdId("CmdAdjustBrightness"), 0);
    const dlg = await waitFor("dialog did not open", () => {
      const d = findWindow(pid, (h) => getWindowText(h) === DIALOG_TITLE);
      return d.length > 0 ? d[0]! : null;
    });

    // &Brightness focuses its slider, 3 x Right sets 3, Enter is OK
    await ensureModifierKeysUp();
    postMessage(dlg, WM_KEYDOWN, "B".charCodeAt(0), 0);
    postMessage(dlg, WM_CHAR, "b".charCodeAt(0), 0);
    for (let i = 0; i < 3; i++) {
      postMessage(dlg, WM_KEYDOWN, VK_RIGHT, 0);
    }
    postMessage(dlg, WM_KEYDOWN, VK_RETURN, 0);
    await waitFor("dialog did not close", () =>
      findWindow(pid, (h) => getWindowText(h) === DIALOG_TITLE).length === 0 ? true : null,
    );

    // OK remembers the values in the file's FileState
    const res = await client.request(ControlCommand.TestPageAdjust, []);
    const state = String(res[1] ?? "").trim();
    console.log(`brightness-second-window: ${state}`);
    if (!state.includes("fs=3,0")) {
      throw new Error(`brightness-second-window: OK in the second window did not apply: ${state}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
