// #6025: installer SUMATRAPDF letters overflowed the window at 150% on
// Windows 7. GDI+ Font UnitPoint used a different DPI than DpiScale() of the
// already-sized window. 75% is the revert detector on a 96-DPI machine: GDI+
// stays at 96 while our layout DPI is 72, so UnitPoint letters stay large and
// clip; UnitPixel + DpiGet() shrinks them with the window.
//
// Run: bun tests/issue-6025.ts [--no-build]
// Needs the regular (non-static) exe: IDR_DLL_PAK is not in SumatraPDF-static.

import { mkdirSync, rmSync } from "node:fs";
import { basename } from "node:path";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait } from "./win-automation.ts";
import {
  captureWindowDCToPng,
  captureWindowDCRegionPixels,
  clientToScreen,
  getClientRect,
  getWindowRect,
  postMessage,
  sleep,
  waitForTopWindow,
  WM_CLOSE,
} from "./winapi.ts";

const INSTALLER_CLASS = "SUMATRA_PDF_INSTALLER_FRAME";

function isInstallerYellow(b: number, g: number, r: number): boolean {
  return r > 230 && g > 210 && b < 50;
}

function pixelAt(data: Uint8Array, w: number, x: number, y: number): [number, number, number] {
  const i = (y * w + x) * 4;
  return [data[i]!, data[i + 1]!, data[i + 2]!];
}

function assertLettersFit(label: string, data: Uint8Array, w: number, h: number): void {
  let letterRow = -1;
  let left = w;
  let right = -1;
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const [b, g, r] = pixelAt(data, w, x, y);
      if (isInstallerYellow(b, g, r)) {
        continue;
      }
      if (letterRow < 0) {
        letterRow = y;
      }
      if (y === letterRow) {
        if (x < left) {
          left = x;
        }
        if (x > right) {
          right = x;
        }
      }
    }
    if (letterRow >= 0 && y > letterRow) {
      break;
    }
  }
  if (letterRow < 0) {
    throw new Error(`issue-6025 ${label}: no installer letters painted in the logo band`);
  }
  const margin = Math.max(4, Math.round(w * 0.015));
  if (left < margin || right > w - 1 - margin) {
    throw new Error(
      `issue-6025 ${label}: letters overflow the window (left=${left} right=${right} w=${w} margin=${margin})`,
    );
  }
}

async function captureLogoBand(hwnd: number): Promise<{ w: number; h: number; data: Uint8Array }> {
  const wr = getWindowRect(hwnd);
  const cr = getClientRect(hwnd);
  const origin = clientToScreen(hwnd, 0, 0);
  const ncLeft = origin.x - wr.left;
  const ncTop = origin.y - wr.top;
  const w = cr.right - cr.left;
  const bandH = Math.max(40, Math.round((cr.bottom - cr.top) * 0.35));
  const data = captureWindowDCRegionPixels(hwnd, ncLeft, ncTop, w, bandH);
  if (!data) {
    throw new Error("issue-6025: failed to capture installer logo band");
  }
  return { w, h: bandH, data };
}

async function runAtDpi(dpiPercent: number, installDir: string, legacy = false): Promise<number> {
  const env = { ...process.env };
  if (dpiPercent !== 100) {
    env.SUMATRA_DPI_OVERRIDE = legacy ? `legacy:${dpiPercent}` : String(dpiPercent);
  }
  const proc = Bun.spawn([EXE, "-for-testing", "-lang", "en", "-install", "-d", installDir], {
    stdout: "ignore",
    stderr: "ignore",
    env,
  });
  try {
    const hwnd = await waitForTopWindow(proc.pid!, INSTALLER_CLASS, 12000);
    if (!hwnd) {
      throw new Error(`issue-6025: installer window did not appear at ${dpiPercent}%`);
    }
    await sleep(2500 * SLOW_BUILD_FACTOR);
    const png = tmpPath(`issue-6025-${legacy ? "legacy-" : ""}${dpiPercent}.png`);
    captureWindowDCToPng(hwnd, png);
    const band = await captureLogoBand(hwnd);
    assertLettersFit(`${dpiPercent}%`, band.data, band.w, band.h);
    const clientWidth = getClientRect(hwnd).right;
    postMessage(hwnd, WM_CLOSE, 0, 0);
    await killAndWait(proc);
    return clientWidth;
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
}

export async function testit(): Promise<void> {
  if (/static/i.test(basename(EXE))) {
    console.log("skip issue-6025: static exe has no installer payload (IDR_DLL_PAK)");
    return;
  }

  const installDir = tmpPath("issue-6025-install");
  rmSync(installDir, { recursive: true, force: true });
  mkdirSync(installDir, { recursive: true });

  const width100 = await runAtDpi(100, installDir);
  // 75% overflows with the old UnitPoint fonts on a 96-DPI machine.
  await runAtDpi(75, installDir);
  // 150% is the scale reported in the issue; letters must still fit.
  await runAtDpi(150, installDir);
  // Windows 7 has neither GetDpiForMonitor nor GetDpiForWindow. Its system DPI
  // must be read from the desktop DC instead of falling back to 96.
  const widthLegacy150 = await runAtDpi(150, installDir, true);
  const expectedLegacyWidth = Math.round(width100 * 1.5);
  if (Math.abs(widthLegacy150 - expectedLegacyWidth) > 1) {
    throw new Error(
      `issue-6025 legacy 150%: installer width ${widthLegacy150}, expected ${expectedLegacyWidth} from ${width100}`,
    );
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
