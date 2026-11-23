// Escape during an annotation resize drag must put the rectangle back.
// The rectangle is written on every mouse move; Escape used to keep the
// new size (and the stale page bitmap) instead of reverting.
//
// Run: bun tests/annot-resize-escape.ts [--no-build] [-ng]

import { ControlClient, ControlCommand } from "./control";
import { ROOT, USE_NG, cmdId, runStandalone } from "./util";
import { join } from "node:path";
import {
  clientToScreen,
  packCoords,
  sendMessage,
  setCursorPos,
  MK_LBUTTON,
  VK_ESCAPE,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi";
import { findCanvas, killAndWait, launchControlled, ngKey } from "./win-automation";

type R = { x: number; y: number; dx: number; dy: number };

async function stampRect(client: ControlClient): Promise<R> {
  const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
  const m = /type=Stamp [^\n]*screen=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`annot-resize-escape: no stamp\n${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleEditPDF"]);
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(80, 80));
    await client.waitForRenderIdle();
    const r0 = await stampRect(client);

    // drag the right edge's handle 40 pixels right, then Escape
    const x = r0.x + r0.dx;
    const y = r0.y + Math.floor(r0.dy / 2);
    const end = clientToScreen(canvas, x + 40, y);
    setCursorPos(end.x, end.y);
    sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x + 40, y));
    sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
    let r1 = r0;
    const deadline = Date.now() + 2000;
    while (r1.dx < r0.dx + 30 && Date.now() < deadline) {
      sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x + 40, y));
      r1 = await stampRect(client);
    }
    if (r1.dx < r0.dx + 30) {
      throw new Error(`annot-resize-escape: resize didn't start: ${JSON.stringify({ r0, r1 })}`);
    }
    if (USE_NG) {
      await ngKey(client, VK_ESCAPE);
    } else {
      sendMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
    }
    sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(x + 40, y));
    await client.waitForRenderIdle();

    const r2 = await stampRect(client);
    if (r2.x !== r0.x || r2.y !== r0.y || r2.dx !== r0.dx || r2.dy !== r0.dy) {
      throw new Error(`annot-resize-escape: Escape kept the new size: ${JSON.stringify({ r0, r1, r2 })}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
