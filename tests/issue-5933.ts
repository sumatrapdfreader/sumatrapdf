// #5933: a newly created annotation is selected (resize handles up). A click
// on empty page must leave that edit-size mode. Mouse-up used to re-select
// whatever annotationUnderCursor last saw, so a click away after creating a
// stamp (hover still on the stamp) kept the handles up.
import { mkdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import {
  captureWindowToPng,
  getClientRect,
  packCoords,
  sendMessage,
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
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-5933: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
