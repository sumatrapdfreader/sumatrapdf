// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6093
//
// Scrollbars = smart drew nothing: the overlay scrollbar was shown, but
// SetWindowPos put it after (i.e. below) the main frame, so the frame painted
// over it. It has to sit above the frame.
//
// Run: bun tests/issue-6093.ts [--no-build]   (or via tests/run-almost-all.ts)

import { join } from "node:path";
import { cmdId, EXE, ROOT, runStandalone, writeAppdata } from "./util.ts";
import {
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowRect,
  isWindowAbove,
  isWindowVisible,
  sleep,
} from "./winapi.ts";
import { withControlledSumatra } from "./control.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";

const SCROLLBAR_CLASS = "SUMATRA_OVERLAY_SCROLLBAR";

function overlayScrollbars(pid: number): number[] {
  const res: number[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === SCROLLBAR_CLASS) {
      res.push(hwnd);
    }
    return true;
  });
  return res;
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata(
    "issue-6093",
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "ShowStartPage = false",
      "Scrollbars = smart",
    ].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      // scrolling shows the thin bar for a few seconds
      sendCommand(frame, cmdId("CmdScrollDown"));
      await sleep(400);

      const bars = overlayScrollbars(proc.pid!).filter((h) => isWindowVisible(h));
      if (bars.length === 0) {
        throw new Error("issue-6093: no visible overlay scrollbar after scrolling");
      }
      for (const bar of bars) {
        if (!isWindowAbove(bar, frame)) {
          const r = getWindowRect(bar);
          throw new Error(
            `issue-6093: overlay scrollbar is behind the main window, so nothing is drawn (${JSON.stringify(r)})`,
          );
        }
      }
    },
    ["-appdata", appdata, "-view", "continuous", "-zoom", "200", join(ROOT, "tests", "issue-5871.pdf")],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
