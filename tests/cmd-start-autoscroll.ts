// Test for CmdStartAutoScroll: it triggers middle-click-style auto-scroll
// without a middle button (e.g. for laptops / trackpads), by anchoring at the
// current cursor position. Implemented as StartAutoScrollAtCursor in
// src/Canvas.cpp, dispatched from src/SumatraPDF.cpp.
//
// The test puts the real cursor over the canvas, invokes the command via
// WM_COMMAND, moves the cursor offset to set a scroll speed, and checks the
// document scrolls -- then invokes the command again and checks it stops.
//
// Drives the app from Bun via FFI (tests/winapi.ts).

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, EXE, tmpPath } from "./util";
import {
  clientToScreen,
  getClientRect,
  getScrollPos,
  moveWindow,
  packCoords,
  postMessage,
  setCursorPos,
  showWindow,
  sleep,
  SW_RESTORE,
  waitForChildWindow,
  waitForTopWindow,
  WM_COMMAND,
  WM_MOUSEMOVE,
} from "./winapi";

// command id, looked up by name from src/Commands.h (the numeric value shifts
// whenever commands are added/removed, so never hardcode it)
const CmdStartAutoScroll = cmdId("CmdStartAutoScroll");

function makePdf(nPages: number): string {
  const objs: string[] = [];
  objs.push(`<< /Type /Catalog /Pages 2 0 R >>`);
  const kids = Array.from({ length: nPages }, (_, i) => `${3 + i} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Count ${nPages} /Kids [${kids}] >>`);
  for (let i = 0; i < nPages; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`);
  }
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

const SETTINGS = [
  `DefaultDisplayMode = continuous`,
  `DefaultZoom = fit page`,
  `Scrollbars = windows`,
  `RestoreSession = false`,
  `ShowStartPage = false`,
  `CheckForUpdates = false`,
  ``,
].join("\n");

export async function testit(): Promise<void> {
  const pdf = tmpPath("cmd-autoscroll.pdf");
  writeFileSync(pdf, makePdf(12), "latin1");

  const appdata = tmpPath("cmd-autoscroll-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  Bun.spawnSync(["taskkill", "/F", "/IM", "SumatraPDF.exe"]);
  await sleep(300);

  const proc = Bun.spawn([EXE, "-for-testing", "-appdata", appdata, pdf], { stdout: "ignore", stderr: "ignore" });
  try {
    const frame = await waitForTopWindow(proc.pid, "SUMATRA_PDF_FRAME");
    if (!frame) {
      throw new Error("SumatraPDF main window did not appear");
    }
    showWindow(frame, SW_RESTORE);
    moveWindow(frame, 0, 0, 900, 750);
    await sleep(1200);

    const canvas = await waitForChildWindow(frame, "SUMATRA_PDF_CANVAS");
    if (!canvas) {
      throw new Error("could not find the canvas window");
    }

    const rc = getClientRect(canvas);
    const cx = Math.floor((rc.right - rc.left) / 2);
    const cy = Math.floor((rc.bottom - rc.top) / 2);

    // put the real cursor over the canvas center -- the command anchors there
    const scr = clientToScreen(canvas, cx, cy);
    setCursorPos(scr.x, scr.y);
    await sleep(150);

    // invoke CmdStartAutoScroll (no middle button involved)
    postMessage(frame, WM_COMMAND, CmdStartAutoScroll, 0);
    await sleep(100);

    // move the cursor offset down from the anchor to set a scroll speed
    postMessage(canvas, WM_MOUSEMOVE, 0, packCoords(cx, cy + 80));
    await sleep(150);
    const start = getScrollPos(canvas);
    await sleep(800);
    const mid = getScrollPos(canvas);

    // invoke again to stop (toggles off, like a second middle-click)
    postMessage(frame, WM_COMMAND, CmdStartAutoScroll, 0);
    await sleep(300);
    const afterStop = getScrollPos(canvas);

    const scrolled = mid - start;
    const afterStopDelta = afterStop - mid;
    console.log(`  invoked command: scrolled ${scrolled}px (pos ${start}->${mid}), then +${afterStopDelta}px after stop`);

    if (scrolled < 30) {
      throw new Error(`CmdStartAutoScroll did not start auto-scroll (moved only ${scrolled}px)`);
    }
    // after the second invocation the auto-scroll must stop (allow a little timer slack)
    if (afterStopDelta > 15) {
      throw new Error(`CmdStartAutoScroll did not stop on the second invocation (+${afterStopDelta}px after stop)`);
    }
    console.log(`  CmdStartAutoScroll starts and stops auto-scroll ✓`);
  } finally {
    proc.kill();
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util");
  await runStandalone(testit);
}
