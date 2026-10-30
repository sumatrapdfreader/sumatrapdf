// The password dialog was created as a child window of the frame it disables, so
// it never got a first paint: blank, with no label, field or buttons. Checks both
// ways of opening a file from Explorer: with an instance running, and without.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, USE_NG, makeTextPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, pressEnter, waitForTitle } from "./win-automation.ts";
import {
  captureWindowDCRegionPixels,
  enumWindows,
  findChildWindow,
  getClassName,
  getClientRect,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  sendText,
} from "./winapi.ts";

const kPassword = "pw";
const kDlgTitle = "Enter password";
const kDlgClass = "SumatraWgDefaultWinClass";
const kBytesPerPixel = 4;
const kDarkLevel = 128;
// the filled OK button alone has more; the caret of a blank dialog has far less
const kMinDarkPixels = 100;
// a window's surface is black until its first paint
const kMaxDarkFraction = 0.1;
const kSettleMs = 500;

function findDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (
      getWindowPid(hwnd) === pid &&
      isWindowVisible(hwnd) &&
      getClassName(hwnd) === kDlgClass &&
      getWindowText(hwnd) === kDlgTitle
    ) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// below the caption, whose title is painted either way
function countDarkPixels(hwnd: number): number {
  const rc = getWindowRect(hwnd);
  const dx = rc.right - rc.left;
  const dy = rc.bottom - rc.top;
  const client = getClientRect(hwnd);
  const clientDy = client.bottom - client.top;
  const data = captureWindowDCRegionPixels(hwnd, 0, dy - clientDy, dx, clientDy);
  let n = 0;
  for (let i = 0; data && i < data.length; i += kBytesPerPixel) {
    if (data[i]! < kDarkLevel && data[i + 1]! < kDarkLevel && data[i + 2]! < kDarkLevel) {
      n++;
    }
  }
  return n;
}

function maxDarkPixels(hwnd: number): number {
  const rc = getClientRect(hwnd);
  return (rc.right - rc.left) * (rc.bottom - rc.top) * kMaxDarkFraction;
}

async function waitForPaintedDialog(pid: number, name: string): Promise<number> {
  const dlg = await pollUntil(
    () => findDialog(pid),
    (hwnd) => hwnd !== 0,
    { error: `password-dialog-reuse-instance: ${name}: no password dialog` },
  );
  await pollUntil(
    async () => {
      await Bun.sleep(kSettleMs);
      return countDarkPixels(dlg);
    },
    (n) => n >= kMinDarkPixels && n <= maxDarkPixels(dlg),
    { error: (n) => `password-dialog-reuse-instance: ${name}: the dialog isn't painted (${n} dark pixels)` },
  );
  return dlg;
}

function makeAppdata(dir: string, name: string): string {
  const appdata = join(dir, name);
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), "Theme = Light\nCheckForUpdates = false\n");
  return appdata;
}

export async function testit(): Promise<void> {
  // ng's dialog is a GPUI window
  if (USE_NG) {
    console.log("skip password-dialog-reuse-instance: orig only");
    return;
  }
  const dir = tmpPath("password-dialog-reuse-instance");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const plain = join(dir, "plain.pdf");
  const encrypted = join(dir, "encrypted.pdf");
  writeFileSync(plain, makeTextPdf("x"));
  const enc = Bun.spawnSync([EXE, "clean", "-E", "aes-256", "-U", kPassword, "-O", "owner", plain, encrypted]);
  if (enc.exitCode !== 0) {
    throw new Error(`password-dialog-reuse-instance: encrypting failed: ${enc.stderr.toString()}`);
  }

  // no instance running: no control pipe either, it starts after the startup load
  const fresh = Bun.spawn([EXE, "-appdata", makeAppdata(dir, "fresh"), encrypted], {
    stdout: "ignore",
    stderr: "ignore",
  });
  try {
    await waitForPaintedDialog(fresh.pid, "new instance");
  } finally {
    await killAndWait(fresh);
  }

  // no -for-testing: it would start a new instance instead of reusing this one
  const appdata = makeAppdata(dir, "reuse");
  const app = await launchControlled(["-appdata", appdata], { saveSettings: true });
  try {
    const second = Bun.spawn([EXE, "-appdata", appdata, encrypted], { stdout: "ignore", stderr: "ignore" });
    await second.exited;
    const dlg = await waitForPaintedDialog(app.proc.pid!, "running instance");

    const edit = findChildWindow(dlg, "Edit");
    sendText(edit, kPassword);
    await pressEnter(edit);
    await waitForTitle(app.frame, (t) => t.includes("encrypted.pdf"));
  } finally {
    app.client.close();
    await killAndWait(app.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
