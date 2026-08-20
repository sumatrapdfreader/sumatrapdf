// Dark theme: combo corners, combo drop-down list, and tooltips must not stay
// white (issue #6000). Visual styles ignore TTM_SETTIP* / WM_CTLCOLOR* unless
// stripped; Win11 rounded combo frames left unpainted corner pixels.
//
// Run: bun tests/issue-6000.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  captureWindowDCRegionPixels,
  captureWindowToPng,
  enumWindows,
  findChildWindow,
  getClassName,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowVisible,
  postMessage,
  sendMessage,
  setCursorPos,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

const CB_SHOWDROPDOWN = 0x014f;
const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

function lum(bgra: Uint8Array, i: number): number {
  return (bgra[i]! + bgra[i + 1]! + bgra[i + 2]!) / 3;
}

function tooLight(bgra: Uint8Array, x: number, y: number, w: number): boolean {
  const i = (y * w + x) * 4;
  return lum(bgra, i) > 180;
}

function grab(hwnd: number): { w: number; h: number; data: Uint8Array } {
  const r = getWindowRect(hwnd);
  const w = r.right - r.left;
  const h = r.bottom - r.top;
  const data = captureWindowDCRegionPixels(hwnd, 0, 0, w, h);
  if (!data) {
    throw new Error("issue-6000: capture failed");
  }
  return { w, h, data };
}

function findFindCombo(pid: number, frame: number): number {
  let combo = 0;
  enumWindows((hwnd) => {
    if (hwnd === frame || getWindowPid(hwnd) !== pid || !isWindowVisible(hwnd)) {
      return true;
    }
    const c = findChildWindow(hwnd, "ComboBox");
    if (c) {
      combo = c;
      return false;
    }
    return true;
  });
  return combo;
}

function findComboList(pid: number): number {
  let list = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || !isWindowVisible(hwnd)) {
      return true;
    }
    if (getClassName(hwnd) !== "ComboLBox") {
      return true;
    }
    const r = getWindowRect(hwnd);
    if (r.bottom - r.top < 16) {
      return true;
    }
    list = hwnd;
    return false;
  });
  return list;
}

function findDialogCombo(pid: number, title: string): { dlg: number; combo: number } {
  let found = { dlg: 0, combo: 0 };
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || !isWindowVisible(hwnd)) {
      return true;
    }
    if (getWindowText(hwnd) !== title) {
      return true;
    }
    const c = findChildWindow(hwnd, "ComboBox");
    if (c) {
      found = { dlg: hwnd, combo: c };
      return false;
    }
    return true;
  });
  return found;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6000");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `Theme = Dark
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`,
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, PDF]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdFindFirst"));
    let combo = 0;
    const deadline = Date.now() + 5_000;
    while (Date.now() < deadline) {
      combo = findFindCombo(proc.pid!, frame);
      if (combo) {
        break;
      }
      await sleep(50);
    }
    if (!combo) {
      throw new Error("issue-6000: find bar ComboBox not found");
    }

    await sleep(80);
    captureWindowToPng(combo, tmpPath("issue-6000-combo.png"));
    const box = grab(combo);
    if (box.w < 4 || box.h < 4) {
      throw new Error(`issue-6000: combo too small ${box.w}x${box.h}`);
    }
    const corners: [number, number][] = [
      [0, 0],
      [box.w - 1, 0],
      [0, box.h - 1],
      [box.w - 1, box.h - 1],
    ];
    for (const [x, y] of corners) {
      if (tooLight(box.data, x, y, box.w)) {
        throw new Error(`issue-6000: combo corner (${x},${y}) is white in Dark theme`);
      }
    }

    const tb = findChildWindow(frame, "SUMATRA_VIRT_TOOLBAR");
    if (tb) {
      const r = getWindowRect(tb);
      setCursorPos(r.left + 40, r.top + 12);
      sendMessage(tb, 0x0020, tb, 1);
      let tip = 0;
      const tipDeadline = Date.now() + 2_000;
      while (Date.now() < tipDeadline) {
        enumWindows((hwnd) => {
          if (getWindowPid(hwnd) !== proc.pid! || !isWindowVisible(hwnd)) {
            return true;
          }
          if (getClassName(hwnd) === "tooltips_class32") {
            const tr = getWindowRect(hwnd);
            if (tr.right - tr.left > 8 && tr.bottom - tr.top > 8) {
              tip = hwnd;
              return false;
            }
          }
          return true;
        });
        if (tip) {
          break;
        }
        await sleep(40);
      }
      if (!tip) {
        throw new Error("issue-6000: toolbar tooltip did not appear");
      }
      captureWindowToPng(tip, tmpPath("issue-6000-tip.png"));
      const bubble = grab(tip);
      const mid = ((bubble.h >> 1) * bubble.w + (bubble.w >> 1)) * 4;
      if (lum(bubble.data, mid) > 180) {
        throw new Error("issue-6000: toolbar tooltip is white in Dark theme");
      }
    }

    // Custom Zoom runs a modal loop; PostMessage so we don't block on it.
    sendCommand(frame, cmdId("CmdZoomCustom"));
    let zoom = { dlg: 0, combo: 0 };
    const zoomDeadline = Date.now() + 5_000;
    while (Date.now() < zoomDeadline) {
      zoom = findDialogCombo(proc.pid!, "Zoom factor");
      if (zoom.combo) {
        break;
      }
      await sleep(50);
    }
    if (!zoom.combo) {
      throw new Error("issue-6000: Custom Zoom ComboBox not found");
    }
    sendMessage(zoom.combo, CB_SHOWDROPDOWN, 1, 0);
    let list = 0;
    const listDeadline = Date.now() + 3_000;
    while (Date.now() < listDeadline) {
      list = findComboList(proc.pid!);
      if (list) {
        break;
      }
      await sleep(40);
    }
    if (!list) {
      throw new Error("issue-6000: ComboLBox drop-down not found");
    }
    await sleep(50);
    captureWindowToPng(list, tmpPath("issue-6000-list.png"));
    const drop = grab(list);
    let light = 0;
    let n = 0;
    for (let y = 2; y < drop.h - 2; y += 3) {
      for (let x = 2; x < drop.w - 2; x += 3) {
        n++;
        if (tooLight(drop.data, x, y, drop.w)) {
          light++;
        }
      }
    }
    if (n === 0 || light / n > 0.4) {
      throw new Error(`issue-6000: drop-down list is white in Dark theme (${light}/${n} light samples)`);
    }
    if (zoom.dlg) {
      postMessage(zoom.dlg, WM_CLOSE, 0, 0);
      await sleep(50);
    }
  } finally {
    await client.quit();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
