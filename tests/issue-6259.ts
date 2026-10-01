// Windows places a maximized window ~8px past each monitor edge. Our WM_NCPAINT
// painted the frame's non-client area, which turned off DWM frame rendering:
// DWM then showed the overhang (on a neighbor monitor, in Alt+PrtScn captures,
// in the min/max animation). The frame must keep DWM's invisible resize
// borders: what DWM shows is the work area when maximized, and the window minus
// the side / bottom borders when restored, with our caption at the very top.
//
// Run: bun tests/issue-6259.ts [--no-build]

import { ControlClient, ControlCommand } from "./control.ts";
import { makeOnePagePdf, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getExtendedFrameBounds,
  getWindowRect,
  getWorkArea,
  postMessage,
  sleep,
  WM_SYSCOMMAND,
} from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { writeFileSync } from "node:fs";

const SC_MAXIMIZE = 0xf030;

async function isZoomed(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestFrameNcStrips, []);
  const raw = String(res[1] ?? "");
  const m = /zoomed=(\d)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6259: TestFrameNcStrips: ${raw}`);
  }
  return m[1] === "1";
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6259.pdf");
  writeFileSync(pdf, makeOnePagePdf(), "latin1");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const wr = getWindowRect(frame);
    const vis = getExtendedFrameBounds(frame);
    const clientTop = clientToScreen(frame, 0, 0).y;
    const state = `window ${JSON.stringify(wr)} visible ${JSON.stringify(vis)} client top ${clientTop}`;
    if (vis.left <= wr.left || vis.right >= wr.right || vis.bottom >= wr.bottom) {
      throw new Error(`issue-6259: restored, the frame has no invisible resize borders: ${state}`);
    }
    if (clientTop !== wr.top || vis.top !== wr.top) {
      throw new Error(`issue-6259: restored, the system caption shows above ours: ${state}`);
    }

    postMessage(frame, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    const deadline = Date.now() + 5000;
    while (!(await isZoomed(client))) {
      if (Date.now() > deadline) {
        throw new Error("issue-6259: the window didn't maximize");
      }
      await sleep(50);
    }
    const maxVis = JSON.stringify(getExtendedFrameBounds(frame));
    const wa = JSON.stringify(getWorkArea());
    if (maxVis !== wa) {
      throw new Error(`issue-6259: maximized, DWM shows ${maxVis} instead of the work area ${wa}`);
    }
    console.log("issue-6259: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
