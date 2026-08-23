// #5933: a newly created annotation is selected (resize handles up). A click
// on empty page must leave that edit-size mode. Mouse-up used to re-select
// whatever annotationUnderCursor last saw, so a click away after creating a
// stamp (hover still on the stamp) kept the handles up.
import { mkdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import {
  captureWindowToPng,
  clientToScreen,
  getClientRect,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  MK_LBUTTON,
  VK_ESCAPE,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi";
import { findCanvas, killAndWait, launchControlled } from "./win-automation";

function hover(canvas: number, x: number, y: number) {
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x, y));
}

function clickAt(canvas: number, x: number, y: number) {
  const lp = packCoords(x, y);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  sendMessage(canvas, WM_LBUTTONUP, 0, lp);
}

// A real click usually moves a few pixels. That used to become a page pan
// (SM_CXDRAG) and skip the mouse-up deselect, so size-edit stuck.
function clickAwayWithJitter(canvas: number, x: number, y: number) {
  const x1 = x + 16;
  const y1 = y + 16;
  const p0 = clientToScreen(canvas, x, y);
  const p1 = clientToScreen(canvas, x1, y1);
  setCursorPos(p0.x, p0.y);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
  setCursorPos(p1.x, p1.y);
  sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x1, y1));
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(x1, y1));
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5933");
  mkdirSync(dir, { recursive: true });
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    const cr = getClientRect(canvas);
    const stampX = 80;
    const stampY = 80;
    const awayX = Math.max(40, cr.right - 40);
    const awayY = Math.max(40, cr.bottom - 40);

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(stampX, stampY));
    await sleep(400);
    await client.waitForRenderIdle();
    hover(canvas, stampX + 20, stampY + 20);
    await sleep(80);
    const selectedPng = join(dir, "selected.png");
    if (!captureWindowToPng(canvas, selectedPng)) {
      throw new Error("issue-5933: capture selected failed");
    }

    // click empty page without a hover update first — that's the regression
    clickAt(canvas, awayX, awayY);
    await sleep(400);
    await client.waitForRenderIdle();
    const afterClickPng = join(dir, "after-click.png");
    if (!captureWindowToPng(canvas, afterClickPng)) {
      throw new Error("issue-5933: capture after click failed");
    }

    sendMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    await sleep(200);
    await client.waitForRenderIdle();
    const afterEscPng = join(dir, "after-esc.png");
    if (!captureWindowToPng(canvas, afterEscPng)) {
      throw new Error("issue-5933: capture after Esc failed");
    }

    const selected = readFileSync(selectedPng);
    const afterClick = readFileSync(afterClickPng);
    const afterEsc = readFileSync(afterEscPng);
    if (selected.equals(afterClick)) {
      throw new Error("issue-5933: click away did not leave stamp size-edit mode");
    }
    if (!afterClick.equals(afterEsc)) {
      throw new Error("issue-5933: click away left a different selection than Esc");
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(stampX, stampY));
    await sleep(400);
    await client.waitForRenderIdle();
    const selected2Png = join(dir, "selected2.png");
    if (!captureWindowToPng(canvas, selected2Png)) {
      throw new Error("issue-5933: capture selected2 failed");
    }
    clickAwayWithJitter(canvas, awayX, awayY);
    await sleep(400);
    await client.waitForRenderIdle();
    const afterJitterPng = join(dir, "after-jitter.png");
    if (!captureWindowToPng(canvas, afterJitterPng)) {
      throw new Error("issue-5933: capture after jitter click failed");
    }
    sendMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    await sleep(200);
    await client.waitForRenderIdle();
    const afterEsc2Png = join(dir, "after-esc2.png");
    if (!captureWindowToPng(canvas, afterEsc2Png)) {
      throw new Error("issue-5933: capture after Esc 2 failed");
    }
    const selected2 = readFileSync(selected2Png);
    const afterJitter = readFileSync(afterJitterPng);
    const afterEsc2 = readFileSync(afterEsc2Png);
    if (selected2.equals(afterJitter)) {
      throw new Error("issue-5933: jittered click away did not leave stamp size-edit mode");
    }
    if (!afterJitter.equals(afterEsc2)) {
      throw new Error("issue-5933: jittered click away left a different selection than Esc");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-5933: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
