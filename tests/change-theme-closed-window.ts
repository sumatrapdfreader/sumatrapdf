// Change Theme is modeless and outlives the window it was opened from. Closing
// that window, then cancelling the dialog, re-rendered the freed MainWindow.
//
// orig only: ng closes the dialog with its window.
// Run: bun tests/change-theme-closed-window.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { makePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { FRAME_CLASS, killAndWait, launchControlled } from "./win-automation.ts";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  postMessage,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

const kTag = "change-theme-closed-window";

function findWindows(pid: number, pred: (hwnd: number) => boolean): number[] {
  const res: number[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && pred(hwnd)) {
      res.push(hwnd);
    }
    return true;
  });
  return res;
}

const frames = (pid: number) => findWindows(pid, (h) => getClassName(h) === FRAME_CLASS);
const themeDialogs = (pid: number) => findWindows(pid, (h) => getWindowText(h) === "Change Theme");

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log(`SKIP ${kTag}: orig only`);
    return;
  }
  const dir = tmpPath(kTag);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(1, 400, 0, 0), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  const pid = proc.pid!;
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestInvokeCommand, ["CmdChangeTheme"]);
    await pollUntil(
      async () => themeDialogs(pid).length,
      (n) => n === 1,
      { error: () => `${kTag}: Change Theme did not open` },
    );

    // a second window keeps the app running once the first one closes
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNewWindow"]);
    await pollUntil(
      async () => frames(pid).length,
      (n) => n === 2,
      { error: (n) => `${kTag}: ${n} windows, want 2` },
    );
    postMessage(frame, WM_CLOSE, 0, 0);
    await pollUntil(
      async () => frames(pid).length,
      (n) => n === 1,
      { error: (n) => `${kTag}: ${n} windows after closing one, want 1` },
    );
    await sleep(300);

    // Cancel reverts the document colors and re-renders the window it came from
    const dlg = themeDialogs(pid)[0];
    if (!dlg) {
      throw new Error(`${kTag}: Change Theme closed with its window`);
    }
    postMessage(dlg, WM_CLOSE, 0, 0);
    await pollUntil(
      async () => (proc.exitCode !== null ? -1 : themeDialogs(pid).length),
      (n) => n <= 0,
      { error: () => `${kTag}: Change Theme did not close` },
    );
    await sleep(300);
    if (proc.exitCode !== null) {
      throw new Error(`${kTag}: app exited (${proc.exitCode}) on cancelling Change Theme`);
    }
    await client.request(ControlCommand.Ping, []);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log(`${kTag}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
