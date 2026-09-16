// #6206: dragging the scrollbar thumb to the top or bottom of a zoomed-in
// single page must leave the view there. It used to spring back to the
// position before the drag.
//
// Run: bun tests/issue-6206.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import {
  enumWindows,
  getClassName,
  getClientRect,
  getScrollInfo,
  getWindowPid,
  packCoords,
  sendMessage,
  sleep,
  MK_LBUTTON,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, writeAppdata } from "./util.ts";

const WM_VSCROLL = 0x0115;
const SB_TOP = 6;
const SB_BOTTOM = 7;
const OVERLAY_CLASS = "SUMATRA_OVERLAY_SCROLLBAR";

function buildPdf(): Buffer {
  const content = "BT /F1 24 Tf 72 2200 Td (top) Tj ET BT /F1 24 Tf 72 72 Td (bottom) Tj ET";
  return Buffer.from(
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R >>",
      "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 2300] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>",
      "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
      `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    ]),
    "latin1",
  );
}

function maxPos(si: { min: number; max: number; page: number }): number {
  return Math.max(si.min, si.max - (si.page | 0) + 1);
}

async function pageScreenY(client: { request: Function }): Promise<number> {
  const res = await client.request(ControlCommand.TestLayout, ["get"]);
  const raw = String(res[1] ?? "");
  const m = /page n=1 shown=\d+ pos=[^\n]* screen=-?\d+,(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-6206: no page 1 in layout:\n${raw}`);
  }
  return +m[1]!;
}

function findVertOverlay(pid: number): number {
  let found = 0;
  let bestDy = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || getClassName(hwnd) !== OVERLAY_CLASS) {
      return true;
    }
    const rc = getClientRect(hwnd);
    const dy = rc.bottom - rc.top;
    const dx = rc.right - rc.left;
    if (dy > dx && dy > bestDy) {
      bestDy = dy;
      found = hwnd;
    }
    return true;
  });
  return found;
}

async function assertStaysAt(label: string, canvas: number, wantMin: boolean): Promise<void> {
  const read = () => {
    const si = getScrollInfo(canvas);
    return { si, atMin: si.pos <= si.min + 1, atMax: si.pos >= maxPos(si) - 1 };
  };
  const first = read();
  if (wantMin ? !first.atMin : !first.atMax) {
    throw new Error(
      `issue-6206 ${label}: expected ${wantMin ? "top" : "bottom"}, got pos=${first.si.pos} min=${first.si.min} max=${first.si.max} page=${first.si.page}`,
    );
  }
  await sleep(250);
  const later = read();
  if (wantMin ? !later.atMin : !later.atMax) {
    throw new Error(
      `issue-6206 ${label}: sprang back from ${wantMin ? "top" : "bottom"} to pos=${later.si.pos} min=${later.si.min} max=${later.si.max} page=${later.si.page}`,
    );
  }
}

async function runWithScrollbars(mode: string, withOverlayDrag: boolean): Promise<void> {
  const pdfPath = tmpPath(`issue-6206-${mode}.pdf`);
  writeFileSync(pdfPath, buildPdf());
  const appdata = writeAppdata(
    `issue-6206-${mode}-appdata`,
    [
      "RestoreSession = false",
      "ShowStartPage = false",
      "SmoothScroll = true",
      "ScrollEdgeTurnsPage = false",
      `Scrollbars = ${mode}`,
      "",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "200",
    pdfPath,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommandSync(frame, cmdId("CmdZoom200"));
    await client.waitForRenderIdle();

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error(`issue-6206 ${mode}: no canvas`);
    }
    let si = getScrollInfo(canvas);
    if (si.max <= si.page) {
      throw new Error(`issue-6206 ${mode}: page is not vertically scrollable: ${JSON.stringify(si)}`);
    }

    const yTop0 = await pageScreenY(client);
    sendMessage(canvas, WM_VSCROLL, SB_BOTTOM, 0);
    await client.waitForRenderIdle();
    await assertStaysAt(`${mode} SB_BOTTOM`, canvas, false);
    const yBot = await pageScreenY(client);
    if (yBot >= yTop0 - 20) {
      throw new Error(`issue-6206 ${mode}: SB_BOTTOM did not move the page (screenY ${yTop0} -> ${yBot})`);
    }

    sendMessage(canvas, WM_VSCROLL, SB_TOP, 0);
    await client.waitForRenderIdle();
    await assertStaysAt(`${mode} SB_TOP`, canvas, true);
    const yTop = await pageScreenY(client);
    if (yTop <= yBot + 20) {
      throw new Error(`issue-6206 ${mode}: SB_TOP did not move the page (screenY ${yBot} -> ${yTop})`);
    }

    if (!withOverlayDrag || !proc.pid) {
      return;
    }
    const overlay = findVertOverlay(proc.pid);
    if (!overlay) {
      throw new Error("issue-6206 overlay: no vertical overlay scrollbar");
    }
    const rc = getClientRect(overlay);
    const midX = Math.floor((rc.right - rc.left) / 2);
    const topY = 8;
    const botY = rc.bottom - rc.top - 8;
    // thumb starts at the top after SB_TOP; drag it to the bottom
    sendMessage(overlay, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(midX, topY + 20));
    sendMessage(overlay, WM_MOUSEMOVE, MK_LBUTTON, packCoords(midX, botY));
    sendMessage(overlay, WM_LBUTTONUP, 0, packCoords(midX, botY));
    // Must already be at the bottom on mouse-up, not still chasing a
    // SmoothScroll target from the drag start (the 3.6.1 spring-back).
    await assertStaysAt("overlay thumb to bottom", canvas, false);
    await client.waitForRenderIdle();
    const yDragBot = await pageScreenY(client);
    if (yDragBot >= yTop - 20) {
      throw new Error(
        `issue-6206 overlay: thumb drag to bottom did not move the page (screenY ${yTop} -> ${yDragBot})`,
      );
    }

    sendMessage(overlay, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(midX, botY - 20));
    sendMessage(overlay, WM_MOUSEMOVE, MK_LBUTTON, packCoords(midX, topY));
    sendMessage(overlay, WM_LBUTTONUP, 0, packCoords(midX, topY));
    await client.waitForRenderIdle();
    await assertStaysAt("overlay thumb to top", canvas, true);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await runWithScrollbars("windows", false);
  await runWithScrollbars("overlay", true);
}

if (import.meta.main) {
  await runStandalone(testit);
}
