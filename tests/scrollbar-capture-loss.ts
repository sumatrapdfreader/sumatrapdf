// Holding the mouse on the overlay scrollbar's track auto-repeats page scrolls
// until the button goes up. When the capture is taken away instead (Alt+Tab,
// a dialog), no button-up comes, so the repeat must end on WM_CAPTURECHANGED.
//
// orig only: ng draws its scrollbars in the frame, without a capture.
// Run: bun tests/scrollbar-capture-loss.ts [--no-build]

import { join } from "node:path";
import { ControlCommand, type ControlClient, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, USE_NG, writeAppdata } from "./util.ts";
import { waitForFrame } from "./win-automation.ts";
import {
  enumWindows,
  getClassName,
  getClientRect,
  getWindowPid,
  isWindowVisible,
  postMessage,
  sendMessage,
  sleep,
  WM_LBUTTONDOWN,
} from "./winapi.ts";

const SCROLLBAR_CLASS = "SUMATRA_OVERLAY_SCROLLBAR";
const WM_CAPTURECHANGED = 0x0215;
const MK_LBUTTON = 0x0001;

function verticalScrollbar(pid: number): number {
  let res = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid || getClassName(hwnd) !== SCROLLBAR_CLASS || !isWindowVisible(hwnd)) {
      return true;
    }
    const r = getClientRect(hwnd);
    if (r.bottom - r.top > r.right - r.left) {
      res = hwnd;
      return false;
    }
    return true;
  });
  return res;
}

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const m = /page=(\d+)/.exec(String(res[1] ?? ""));
  if (!m) {
    throw new Error(`scrollbar-capture-loss: no current page (${String(res[1])})`);
  }
  return +m[1]!;
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP scrollbar-capture-loss: orig only");
    return;
  }
  const appdata = writeAppdata(
    "scrollbar-capture-loss",
    ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", "Scrollbars = overlay"].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      const bar = verticalScrollbar(proc.pid!);
      if (!bar) {
        throw new Error("scrollbar-capture-loss: no vertical overlay scrollbar");
      }

      // press on the track below the thumb, then lose the capture before
      // the auto-repeat's initial delay ends
      const r = getClientRect(bar);
      const x = Math.floor((r.right - r.left) / 2);
      const y = r.bottom - r.top - 30;
      postMessage(bar, WM_LBUTTONDOWN, MK_LBUTTON, (y << 16) | x);
      await sleep(50);
      sendMessage(bar, WM_CAPTURECHANGED, 0, 0);

      const before = await currentPage(client);
      await sleep(2500);
      const after = await currentPage(client);
      if (after !== before) {
        throw new Error(`scrollbar-capture-loss: kept scrolling after losing capture (page ${before} -> ${after})`);
      }
    },
    ["-appdata", appdata, "-view", "continuous", "-zoom", "1600", join(ROOT, "tests", "issue-6132.pdf")],
  );
  console.log("scrollbar-capture-loss: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
