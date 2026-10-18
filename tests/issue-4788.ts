// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4788
//
// Brightness / contrast of rendered pages, per file. CmdAdjustBrightness and
// CmdAdjustContrast open the same dialog. The page is two grays (0.4 over
// 0.6), read off the canvas: brightness raises both, contrast spreads them
// apart. The values are remembered for the file: closing and reopening it in
// the same session renders it adjusted again.
//
// Run: bun tests/issue-4788.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import {
  captureWindowPixels,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  postMessage,
  sendCopyDataW,
  sendMessage,
  sleep,
  VK_END,
  VK_RIGHT,
  WM_CHAR,
  WM_COMMAND,
  WM_KEYDOWN,
} from "./winapi.ts";

const PAGE_DX = 612;
const PAGE_DY = 792;
const DIALOG_TITLE = "Brightness / Contrast";
const WM_CLOSE = 0x0010;

// top half 40% gray, bottom half 60% gray
function makePdf(): string {
  const stream = [
    `0.4 g 0 ${PAGE_DY / 2} ${PAGE_DX} ${PAGE_DY / 2} re f`,
    `0.6 g 0 0 ${PAGE_DX} ${PAGE_DY / 2} re f`,
  ].join("\n");
  return assemblePdf([
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 1 /Kids [3 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${PAGE_DX} ${PAGE_DY}] /Contents 4 0 R >>`,
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
  ]);
}

type Grays = { top: number; bottom: number };

// at fit page the page fills the canvas height around its center, so a
// quarter of the height above and below the center is inside each half
function sampleGrays(canvas: number): Grays {
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error("issue-4788: could not capture the canvas");
  }
  const { w, h, data } = cap;
  const at = (y: number) => {
    const i = (y * w + Math.floor(w / 2)) * 4;
    return Math.round((data[i]! + data[i + 1]! + data[i + 2]!) / 3);
  };
  return { top: at(Math.floor(h / 4)), bottom: at(Math.floor((h * 3) / 4)) };
}

async function setAdjust(client: ControlClient, brightness: number, contrast: number): Promise<string> {
  const res = await client.request(ControlCommand.TestPageAdjust, [brightness, contrast]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`issue-4788: TestPageAdjust failed: ${raw}`);
  }
  return raw;
}

async function readAdjust(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestPageAdjust, []);
  return String(res[1] ?? "").trim();
}

function findDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && isWindowVisible(hwnd) && getWindowText(hwnd) === DIALOG_TITLE) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitForDialog(pid: number, timeoutMs = 5000): Promise<number> {
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    const hwnd = findDialog(pid);
    if (hwnd) {
      return hwnd;
    }
    await sleep(100);
  }
  return 0;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-4788.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.setNotificationsEnabled(false);
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdZoomFitPage")), 0n);
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);

    const plain = sampleGrays(canvas);
    console.log(`issue-4788: plain ${JSON.stringify(plain)}`);
    if (Math.abs(plain.top - 102) > 8 || Math.abs(plain.bottom - 153) > 8) {
      throw new Error(`issue-4788: unexpected page grays ${JSON.stringify(plain)} (canvas layout?)`);
    }

    await setAdjust(client, 50, 0);
    await client.waitForRenderIdle();
    const bright = sampleGrays(canvas);
    console.log(`issue-4788: brightness 50 ${JSON.stringify(bright)}`);
    if (bright.top < plain.top + 40 || bright.bottom < plain.bottom + 40) {
      throw new Error(`issue-4788: brightness did not brighten the page: ${JSON.stringify(bright)}`);
    }

    await setAdjust(client, 0, 60);
    await client.waitForRenderIdle();
    const contrasty = sampleGrays(canvas);
    console.log(`issue-4788: contrast 60 ${JSON.stringify(contrasty)}`);
    if (contrasty.top > plain.top - 20 || contrasty.bottom < plain.bottom + 20) {
      throw new Error(`issue-4788: contrast did not spread the grays: ${JSON.stringify(contrasty)}`);
    }

    // remembered for the file: close it, open it again
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdClose")), 0n);
    await sleep(300);
    if (!sendCopyDataW(frame, 0x44646557, `[Open("${pdf}", 0, 1, 0)]`)) {
      throw new Error("issue-4788: could not reopen the document");
    }
    await client.waitForRenderIdle();
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdZoomFitPage")), 0n);
    await client.waitForRenderIdle();
    const state = await readAdjust(client);
    const reopened = sampleGrays(canvas);
    console.log(`issue-4788: reopened ${state} ${JSON.stringify(reopened)}`);
    if (!state.includes("brightness=0 contrast=60")) {
      throw new Error(`issue-4788: values not remembered for the file: ${state}`);
    }
    if (reopened.top > plain.top - 20 || reopened.bottom < plain.bottom + 20) {
      throw new Error(`issue-4788: reopened file not rendered adjusted: ${JSON.stringify(reopened)}`);
    }

    // keyboard: &Brightness / &Contrast mnemonics focus the sliders, arrows / End change them
    postMessage(frame, WM_COMMAND, cmdId("CmdAdjustBrightness"), 0);
    const kbdDlg = await waitForDialog(proc.pid!);
    if (!kbdDlg) {
      throw new Error(`issue-4788: CmdAdjustBrightness did not open the "${DIALOG_TITLE}" dialog`);
    }
    postMessage(kbdDlg, WM_CHAR, "b".charCodeAt(0), 0);
    for (let i = 0; i < 3; i++) {
      postMessage(kbdDlg, WM_KEYDOWN, VK_RIGHT, 0);
    }
    postMessage(kbdDlg, WM_CHAR, "c".charCodeAt(0), 0);
    postMessage(kbdDlg, WM_KEYDOWN, VK_END, 0);
    await sleep(300);
    const kbd = await readAdjust(client);
    console.log(`issue-4788: keyboard ${kbd}`);
    if (!kbd.includes("brightness=3 contrast=100")) {
      throw new Error(`issue-4788: keyboard did not change the sliders: ${kbd}`);
    }
    postMessage(kbdDlg, WM_CLOSE, 0, 0);
    await sleep(300);

    // both commands open the one dialog
    for (const name of ["CmdAdjustBrightness", "CmdAdjustContrast"]) {
      postMessage(frame, WM_COMMAND, cmdId(name), 0);
      const dlg = await waitForDialog(proc.pid!);
      if (!dlg) {
        throw new Error(`issue-4788: ${name} did not open the "${DIALOG_TITLE}" dialog`);
      }
      console.log(`issue-4788: ${name} opened ${getClassName(dlg)}`);
      postMessage(dlg, WM_CLOSE, 0, 0);
      await sleep(300);
      if (findDialog(proc.pid!)) {
        throw new Error(`issue-4788: the dialog did not close`);
      }
    }
    // closing the dialog (cancel) keeps the remembered values
    const after = await readAdjust(client);
    if (!after.includes("brightness=0 contrast=60")) {
      throw new Error(`issue-4788: closing the dialog changed the values: ${after}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
