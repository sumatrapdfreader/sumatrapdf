// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4662
//
// Arrow-key line scrolling used to jump by ScrollLineAmount instantly, which
// reads as jitter on zoomed/tall pages. With SmoothScroll (default true) it
// must ease toward the target the same way the mouse wheel does.
//
// Run: bun tests/issue-4662.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { getScrollInfo, sendMessage, sleep } from "./winapi.ts";
import { findCanvas, sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, ROOT, runStandalone } from "./util.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const WM_VSCROLL = 0x0115;
const SB_TOP = 6;

export async function testit(): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      if (!frame) {
        throw new Error("issue-4662: no frame");
      }
      await client.request(ControlCommand.SetNotificationsEnabled, [0]);
      await client.waitForRenderIdle();
      sendCommandSync(frame, cmdId("CmdZoomFitWidthAndContinuous"));
      await client.waitForRenderIdle();

      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-4662: no canvas");
      }
      sendMessage(canvas, WM_VSCROLL, SB_TOP, 0);
      await client.waitForRenderIdle();

      let si = getScrollInfo(canvas);
      if (si.max <= si.page) {
        sendCommandSync(frame, cmdId("CmdZoom200"));
        await client.waitForRenderIdle();
        sendMessage(canvas, WM_VSCROLL, SB_TOP, 0);
        await client.waitForRenderIdle();
        si = getScrollInfo(canvas);
      }
      if (si.max <= si.page) {
        throw new Error(`issue-4662: document is not vertically scrollable: ${JSON.stringify(si)}`);
      }

      const start = getScrollInfo(canvas).pos;
      sendCommandSync(frame, cmdId("CmdScrollDown"));
      const immediate = getScrollInfo(canvas).pos;
      await sleep(40);
      const mid = getScrollInfo(canvas).pos;
      await client.waitForRenderIdle();
      const settled = getScrollInfo(canvas).pos;
      const line = settled - start;
      if (line < 1) {
        throw new Error(`issue-4662: arrow key did not scroll (start=${start} settled=${settled})`);
      }
      // Instant jump would already be at `settled` when SendMessage returns.
      // Smooth scroll leaves the thumb on the old pos until the timer ticks.
      if (immediate === settled && immediate - start === line) {
        throw new Error(
          `issue-4662: arrow key jumped instantly by ${line}px (want SmoothScroll chase; immediate=${immediate} mid=${mid})`,
        );
      }
      if (mid > settled) {
        throw new Error(`issue-4662: overshot during chase: mid=${mid} settled=${settled}`);
      }

      const afterOne = settled;
      for (let i = 0; i < 8; i++) {
        sendCommandSync(frame, cmdId("CmdScrollDown"));
      }
      await client.waitForRenderIdle();
      const afterBurst = getScrollInfo(canvas).pos;
      const burst = afterBurst - afterOne;
      if (burst !== 8 * line) {
        throw new Error(
          `issue-4662: 8 key-repeat arrows should accumulate 8*${line}=${8 * line}px, got ${burst} (pos ${afterOne} -> ${afterBurst})`,
        );
      }
    },
    [PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
