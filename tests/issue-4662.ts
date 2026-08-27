// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4662
//
// Arrow-key line scrolling used to jump by ScrollLineAmount instantly, which
// reads as jitter on zoomed/tall pages. With SmoothScroll (default true) it
// must ease toward the target the same way the mouse wheel does.
//
// Run: bun tests/issue-4662.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { getScrollInfo, sendMessage, sleep } from "./winapi.ts";
import { findCanvas, sendCommandSync, waitForFrame } from "./win-automation.ts";
import { cmdId, EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const WM_VSCROLL = 0x0115;
const SB_TOP = 6;

export async function testit(): Promise<void> {
  // SmoothScroll is what this test is about, so don't inherit whoever's
  // settings the default appdata holds - with it off, every line scroll is an
  // instant jump and the test fails for a reason that isn't a regression.
  const dir = tmpPath("issue-4662");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `SmoothScroll = true
UiLanguage = en
RestoreSession = false
ShowStartPage = false
CheckForUpdates = false
`,
  );

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

      // The chase takes ~230ms, and reading it means sampling from another
      // process: on a loaded machine this one can be starved past the end of
      // the animation, and the sample then looks exactly like an instant jump.
      // A broken SmoothScroll jumps on every try, a scheduling hiccup doesn't.
      const kTries = 3;
      let line = 0;
      let chased = false;
      let lastTry = "";
      let afterOne = 0;
      for (let i = 0; i < kTries && !chased; i++) {
        const start = getScrollInfo(canvas).pos;
        sendCommandSync(frame, cmdId("CmdScrollDown"));
        const immediate = getScrollInfo(canvas).pos;
        await sleep(40);
        const mid = getScrollInfo(canvas).pos;
        await client.waitForRenderIdle();
        const settled = getScrollInfo(canvas).pos;
        line = settled - start;
        afterOne = settled;
        if (line < 1) {
          throw new Error(`issue-4662: arrow key did not scroll (start=${start} settled=${settled})`);
        }
        if (mid > settled) {
          throw new Error(`issue-4662: overshot during chase: mid=${mid} settled=${settled}`);
        }
        // Instant jump would already be at `settled` when SendMessage returns.
        // Smooth scroll leaves the thumb on the old pos until the timer ticks.
        chased = immediate !== settled || immediate - start !== line;
        lastTry = `immediate=${immediate} mid=${mid} settled=${settled}`;
      }
      if (!chased) {
        throw new Error(
          `issue-4662: arrow key jumped instantly by ${line}px on all ${kTries} tries (want SmoothScroll chase; ${lastTry})`,
        );
      }

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
    ["-appdata", dir, PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
