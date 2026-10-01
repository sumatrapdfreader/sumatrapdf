// #6113: notification toasts on RTL UI (Hebrew) drew English text backwards.
// WS_EX_LAYOUTRTL plus BitBlt of an LTR Gfx buffer mirrored every glyph.
// "Zoom: N%" has a long bar on the Z; that bar must stay in the left half.
//
// Run: bun tests/issue-6113.ts [--no-build]

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { inflateSync } from "node:zlib";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { captureWindowToPng, enumChildWindows, getClassName, getWindowText, isWindowVisible, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const NOTIF_CLASS = "SumatraWgDefaultWinClass";

type Shot = { w: number; h: number; data: Uint8Array };

function findNotif(canvas: number, expected = ""): number {
  let found = 0;
  enumChildWindows(canvas, (hwnd) => {
    if (
      getClassName(hwnd) === NOTIF_CLASS &&
      isWindowVisible(hwnd) &&
      (!expected || getWindowText(hwnd) === expected)
    ) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitNotif(canvas: number, want: boolean, expected = "", timeoutMs = 4000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const hwnd = findNotif(canvas, expected);
    if (want ? hwnd !== 0 : hwnd === 0) {
      return hwnd;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6113: notification ${want ? "did not appear" : "did not dismiss"}`);
    }
    await sleep(40);
  }
}

function paeth(a: number, b: number, c: number): number {
  const p = a + b - c;
  const pa = Math.abs(p - a);
  const pb = Math.abs(p - b);
  const pc = Math.abs(p - c);
  if (pa <= pb && pa <= pc) {
    return a;
  }
  if (pb <= pc) {
    return b;
  }
  return c;
}

// PrintWindow into an LTR DIB misses the RTL blit; captureWindowToPng does not.
function loadPng(path: string): Shot {
  const buf = readFileSync(path);
  let off = 8;
  let w = 0;
  let h = 0;
  let colorType = 0;
  const idat: Buffer[] = [];
  while (off + 8 <= buf.length) {
    const n = buf.readUInt32BE(off);
    const type = buf.toString("latin1", off + 4, off + 8);
    const data = buf.subarray(off + 8, off + 8 + n);
    if (type === "IHDR") {
      w = data.readUInt32BE(0);
      h = data.readUInt32BE(4);
      colorType = data[9]!;
    } else if (type === "IDAT") {
      idat.push(data);
    } else if (type === "IEND") {
      break;
    }
    off += 12 + n;
  }
  if (colorType !== 6 || w <= 0 || h <= 0) {
    throw new Error(`issue-6113: unsupported png ${path} ct=${colorType} ${w}x${h}`);
  }
  const raw = inflateSync(Buffer.concat(idat));
  const stride = w * 4;
  const data = new Uint8Array(h * stride);
  let src = 0;
  for (let y = 0; y < h; y++) {
    const filter = raw[src++]!;
    const row = y * stride;
    const prev = (y - 1) * stride;
    for (let x = 0; x < stride; x++) {
      const left = x >= 4 ? data[row + x - 4]! : 0;
      const up = y > 0 ? data[prev + x]! : 0;
      const ul = y > 0 && x >= 4 ? data[prev + x - 4]! : 0;
      const v = raw[src++]!;
      let recon = v;
      if (filter === 1) {
        recon = (v + left) & 255;
      } else if (filter === 2) {
        recon = (v + up) & 255;
      } else if (filter === 3) {
        recon = (v + ((left + up) >> 1)) & 255;
      } else if (filter === 4) {
        recon = (v + paeth(left, up, ul)) & 255;
      } else if (filter !== 0) {
        throw new Error(`issue-6113: png filter ${filter}`);
      }
      data[row + x] = recon;
    }
  }
  return { w, h, data };
}

function isDark(shot: Shot, x: number, y: number): boolean {
  const bg = shot.data[0]! + shot.data[1]! + shot.data[2]!;
  const i = (y * shot.w + x) * 4;
  return shot.data[i]! + shot.data[i + 1]! + shot.data[i + 2]! < bg - 80;
}

type Bar = { y: number; x: number; dx: number };

// Longest solid dark run on the ink-densest row. For "Zoom: N%" that run is
// the Z's bar. dx is 0 when the shot has no ink yet.
function longestBar(shot: Shot): Bar {
  let bestY = 0;
  let bestN = 0;
  for (let y = 0; y < shot.h; y++) {
    let n = 0;
    for (let x = 0; x < shot.w; x++) {
      if (isDark(shot, x, y)) {
        n++;
      }
    }
    if (n > bestN) {
      bestN = n;
      bestY = y;
    }
  }
  let x0 = 0;
  let dx = 0;
  let x = 0;
  while (x < shot.w) {
    if (!isDark(shot, x, bestY)) {
      x++;
      continue;
    }
    let j = x;
    while (j < shot.w && isDark(shot, j, bestY)) {
      j++;
    }
    if (j - x > dx) {
      x0 = x;
      dx = j - x;
    }
    x = j;
  }
  return { y: bestY, x: x0, dx };
}

function longestBarDx(shot: Shot): number {
  return longestBar(shot).dx;
}

// Midpoint of the Z's bar: left half when glyphs are upright, right half when
// the toast was BitBlt-mirrored.
function longestBarMid(shot: Shot): { y: number; x: number; dx: number; mid: number } {
  const bar = longestBar(shot);
  if (bar.dx < MIN_BAR_DX) {
    throw new Error(`issue-6113: no long ink bar (y=${bar.y} dx=${bar.dx} ${shot.w}x${shot.h})`);
  }
  return { ...bar, mid: bar.x + bar.dx / 2 };
}

// a capture can land before the toast painted its text (blank 122x35 shot with
// no ink): re-capture until a bar shows up
const TOAST_PAINT_TIMEOUT_MS = 3000;
const MIN_BAR_DX = 6;

function hasInk(shot: Shot): boolean {
  return longestBarDx(shot) >= MIN_BAR_DX;
}

async function grabZoomToast(
  frame: number,
  canvas: number,
  command: string,
  message: string,
  label: string,
): Promise<Shot> {
  sendCommandSync(frame, cmdId(command));
  const hwnd = await waitNotif(canvas, true, message);
  const png = tmpPath(`6113-${label}.png`);
  const deadline = Date.now() + TOAST_PAINT_TIMEOUT_MS;
  for (;;) {
    if (!captureWindowToPng(hwnd, png)) {
      throw new Error(`issue-6113: capture failed (${label})`);
    }
    const shot = loadPng(png);
    if (hasInk(shot) || Date.now() > deadline) {
      return shot;
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6113: canvas not found");
    }

    const ltrShot = await grabZoomToast(frame, canvas, "CmdZoom125", "Zoom: 125%", "ltr");
    const ltr = longestBarMid(ltrShot);
    if (ltr.mid > ltrShot.w / 2) {
      throw new Error(`issue-6113: LTR Z-bar should be on the left (mid=${ltr.mid} w=${ltrShot.w})`);
    }

    await client.setNotificationsEnabled(false);
    await waitNotif(canvas, false);
    await client.setNotificationsEnabled(true);

    sendCommandSync(frame, cmdId("CmdDebugToggleRtl"));
    await client.waitForRenderIdle();

    const rtlShot = await grabZoomToast(frame, canvas, "CmdZoom150", "Zoom: 150%", "rtl");
    const rtl = longestBarMid(rtlShot);
    if (rtl.mid > rtlShot.w / 2) {
      throw new Error(
        `issue-6113: notification text is mirrored in RTL (bar mid=${rtl.mid} w=${rtlShot.w} y=${rtl.y} dx=${rtl.dx})`,
      );
    }
  } finally {
    await client.quit();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
