// Command Palette list, mode switches and hints follow RTL when the UI
// language is Hebrew (issue #5956). The palette hwnd stays LTR so hit-testing
// is not mirrored; drawing and HBox layout use IsUIRtl().
//
// The document page is never mirrored (issue #5326). Toggling RTL and back
// must restore the same page pixels — a leftover LAYOUT_RTL on the canvas
// double-buffer used to keep the page flipped after leaving RTL.
//
// Run: bun tests/issue-5956.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  captureWindowPixels,
  captureWindowToPng,
  getClassName,
  getFocusedHwnd,
  getRootWindow,
  postMessage,
  sleep,
  WM_CLOSE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type Shot = { w: number; h: number; data: Uint8Array };

async function paletteRtl(client: ControlClient): Promise<number | null> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const exitCode = res[0] as number;
  const out = String(res[1] ?? "");
  if (exitCode === 2) {
    return null;
  }
  if (exitCode !== 0) {
    throw new Error(`issue-5956: TestCommandPalette failed: ${out.trim()}`);
  }
  const m = /rtl=(\d+)/.exec(out);
  if (!m) {
    throw new Error(`issue-5956: no rtl= in: ${out.trim()}`);
  }
  return Number(m[1]);
}

async function waitRtl(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 8_000;
  for (;;) {
    const rtl = await paletteRtl(client);
    if (rtl !== null) {
      return rtl;
    }
    if (Date.now() > deadline) {
      throw new Error("issue-5956: command palette did not open");
    }
    await sleep(50);
  }
}

function findPalette(frame: number): number {
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return 0;
  }
  const palette = getRootWindow(edit);
  return palette === frame ? 0 : palette;
}

async function closePalette(client: ControlClient, frame: number): Promise<void> {
  const palette = findPalette(frame);
  if (palette) {
    postMessage(palette, WM_CLOSE, 0, 0);
  }
  const deadline = Date.now() + 3_000;
  while (Date.now() < deadline) {
    if ((await paletteRtl(client)) === null) {
      return;
    }
    await sleep(50);
  }
}

function grabCanvas(canvas: number): Shot {
  const shot = captureWindowPixels(canvas);
  if (!shot) {
    throw new Error("issue-5956: canvas capture failed");
  }
  return shot;
}

function countDiff(a: Shot, b: Shot): number {
  if (a.w !== b.w || a.h !== b.h) {
    return Math.max(a.w * a.h, b.w * b.h);
  }
  const da = a.data;
  const db = b.data;
  let n = 0;
  for (let i = 0; i < da.length; i += 4) {
    if (da[i] !== db[i] || da[i + 1] !== db[i + 1] || da[i + 2] !== db[i + 2]) {
      n++;
    }
  }
  return n;
}

function hMirror(src: Shot): Shot {
  const { w, h, data } = src;
  const out = new Uint8Array(data.length);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      const si = (y * w + x) * 4;
      const di = (y * w + (w - 1 - x)) * 4;
      out[di] = data[si];
      out[di + 1] = data[si + 1];
      out[di + 2] = data[si + 2];
      out[di + 3] = data[si + 3];
    }
  }
  return { w, h, data: out };
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdCommandPalette"));
    const ltr = await waitRtl(client);
    if (ltr !== 0) {
      throw new Error(`issue-5956: expected rtl=0 in LTR UI, got ${ltr}`);
    }
    await closePalette(client, frame);

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-5956: canvas not found");
    }
    await client.waitForRenderIdle();
    captureWindowToPng(canvas, tmpPath("issue-5956-ltr.png"));
    const before = grabCanvas(canvas);

    // -lang he hits a missing-translation debug abort under -for-testing.
    // CmdDebugToggleRtl is the same IsUIRtl() path the palette uses.
    sendCommandSync(frame, cmdId("CmdDebugToggleRtl"));
    await client.waitForRenderIdle();
    captureWindowToPng(canvas, tmpPath("issue-5956-rtl.png"));
    const rtlShot = grabCanvas(canvas);

    sendCommandSync(frame, cmdId("CmdCommandPalette"));
    const rtl = await waitRtl(client);
    if (rtl !== 1) {
      throw new Error(`issue-5956: expected rtl=1 after CmdDebugToggleRtl, got ${rtl}`);
    }
    await closePalette(client, frame);

    sendCommandSync(frame, cmdId("CmdDebugToggleRtl"));
    await client.waitForRenderIdle();
    captureWindowToPng(canvas, tmpPath("issue-5956-ltr-after.png"));
    const after = grabCanvas(canvas);

    const pixels = before.w * before.h;
    const vsRtl = countDiff(before, rtlShot);
    const vsMirror = countDiff(before, hMirror(rtlShot));
    const vsAfter = countDiff(before, after);
    // A mirrored page is much closer to the horizontal flip of LTR than to LTR.
    if (vsRtl > pixels * 0.08 && vsMirror < vsRtl * 0.5) {
      throw new Error(
        `issue-5956: page is mirrored in RTL (ltr-vs-rtl=${vsRtl} ltr-vs-mirror(rtl)=${vsMirror} of ${pixels})`,
      );
    }
    // Leaving RTL used to keep the flipped buffer. After must match before.
    if (vsAfter > pixels * 0.08) {
      throw new Error(`issue-5956: page still flipped after leaving RTL (diff=${vsAfter} of ${pixels})`);
    }
  } finally {
    try {
      await client.quit();
    } catch {
      /* process already gone */
    }
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
