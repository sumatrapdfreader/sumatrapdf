// Closing a main window while one of its dialogs is up. ng ran the dialog's
// close callback from the ui task queue after the MainWindow was deleted, and
// the callback read the window (use after free).
//
// ng only: orig's dialogs are modal win32 dialogs owned by the frame.
// Run: bun tests/close-window-with-dialog.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { EXE, makePdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import {
  enumWindows,
  getWindowOwner,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  postMessage,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";

const kDlgTitle = "Enter password";

function findDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && getWindowText(hwnd) === kDlgTitle) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function windowCount(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
  const m = /\bwindows=(\d+)/.exec(s);
  return m ? +m[1]! : -1;
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP close-window-with-dialog: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("close-window-with-dialog");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const plain = join(dir, "plain.pdf");
  const encrypted = join(dir, "encrypted.pdf");
  writeFileSync(plain, makePdf(1, 400, 0, 0), "latin1");
  const enc = Bun.spawnSync([EXE, "clean", "-E", "aes-256", "-U", "pw", "-O", "owner", plain, encrypted]);
  if (enc.exitCode !== 0) {
    throw new Error(`close-window-with-dialog: encrypting failed: ${enc.stderr.toString()}`);
  }

  const { proc, client } = await launchControlled([encrypted]);
  try {
    const dlg = await pollUntil(
      async () => findDialog(proc.pid!),
      (h) => h !== 0,
      { error: () => "close-window-with-dialog: no password dialog" },
    );
    const frame = getWindowOwner(dlg);
    if (!frame) {
      throw new Error("close-window-with-dialog: password dialog has no owner");
    }

    // a second window keeps the app running once the first one closes
    await client.request(ControlCommand.TestInvokeCommand, ["CmdNewWindow"]);
    await pollUntil(
      async () => windowCount(client),
      (n) => n === 2,
      { error: (n) => `close-window-with-dialog: ${n} windows, want 2` },
    );

    postMessage(frame, WM_CLOSE, 0, 0);
    await pollUntil(
      async () => windowCount(client),
      (n) => n === 1,
      { error: (n) => `close-window-with-dialog: ${n} windows after closing one, want 1` },
    );
    // the dialog's deferred close runs from the ui task queue
    await sleep(500);
    if (proc.exitCode !== null) {
      throw new Error(`close-window-with-dialog: app exited (${proc.exitCode})`);
    }
    if (findDialog(proc.pid!)) {
      throw new Error("close-window-with-dialog: password dialog outlived its window");
    }
    await client.request(ControlCommand.Ping, []);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("close-window-with-dialog: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
