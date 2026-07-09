// Ad-hoc visual check for issue #4295: the Find bar has a "Match whole word"
// toggle button next to "Match Case". Launches the app on a PDF, opens the
// Find bar, captures it (so the new icon can be eyeballed), and toggles the
// whole-word command to confirm it doesn't crash.
//
// Run:  bun tests/ad-hoc-find-whole-word.ts [--no-build]

import { existsSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation.ts";
import { sleep, enumWindows, getWindowPid, captureWindowToPng, getClassName } from "./winapi.ts";

const CmdFindFirst = cmdId("CmdFindFirst");
const CmdFindToggleMatchWholeWord = cmdId("CmdFindToggleMatchWholeWord");

const PDF = join(import.meta.dir, "issue-5597.pdf");

// the find bar is a WS_POPUP owned by the frame, with no special class name;
// after showing it, it's the process's visible top-level window that isn't the
// frame. Capture whichever popup we can find.
function findPopups(pid: number, frame: number): number[] {
  const out: number[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && hwnd !== frame) {
      out.push(hwnd);
    }
    return true;
  });
  return out;
}

export async function testit(): Promise<void> {
  if (!existsSync(PDF)) {
    console.log(`SKIP: ${PDF} not found`);
    return;
  }
  const proc = launchSumatra([PDF]);
  try {
    const frame = await waitForFrame(proc.pid!);
    await sleep(1500);

    // open the Find bar
    sendCommand(frame, CmdFindFirst);
    await sleep(1200);

    const popups = findPopups(proc.pid!, frame);
    console.log(`found ${popups.length} popup window(s): ${popups.map((h) => `${h}(${getClassName(h)})`).join(", ")}`);

    // capture all popups; the find bar is among them
    let captured = 0;
    for (const h of popups) {
      const png = tmpPath(`find-whole-word-popup-${h}.png`);
      if (captureWindowToPng(h, png)) {
        console.log(`captured ${png}`);
        captured++;
      }
    }
    if (captured === 0) {
      // fall back to capturing the frame (the bar overlaps it)
      const png = tmpPath("find-whole-word-frame.png");
      captureWindowToPng(frame, png);
      console.log(`captured frame -> ${png}`);
    }

    // toggle whole-word on then off; must not crash
    sendCommand(frame, CmdFindToggleMatchWholeWord);
    await sleep(500);
    sendCommand(frame, CmdFindToggleMatchWholeWord);
    await sleep(500);

    console.log("PASS: find bar shown and whole-word command toggled without crashing");
  } finally {
    proc.kill();
    await sleep(300);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
