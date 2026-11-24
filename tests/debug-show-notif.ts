// Debug / Show notification shows its notifications. The Debug menu is also in
// pre-release builds, where the command's handler used to be compiled out
// (#if IS_DEBUG), so the menu item did nothing. Run with
// `-exe out/rel64/SumatraPDF.exe` to check a release build.
import { cmdId, runStandalone, USE_NG } from "./util";
import { enumChildWindows, getClassName, isWindowVisible, sleep } from "./winapi";
import { killAndWait, launchControlled, sendCommand } from "./win-automation";

const kNotifClass = "SumatraWgDefaultWinClass";
// two text, a warning, a progress and a custom-content notification
const kNotifCount = 5;

function countNotifs(frame: number): number {
  let n = 0;
  enumChildWindows(frame, (h) => {
    if (getClassName(h) === kNotifClass && isWindowVisible(h)) {
      n++;
    }
    return true;
  });
  return n;
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("debug-show-notif: orig only (ng's notifications are drawn in the frame)");
    return;
  }
  const { proc, frame } = await launchControlled(["tests/issue-1189.pdf"]);
  try {
    const before = countNotifs(frame);
    sendCommand(frame, cmdId("CmdDebugShowNotif"));
    const deadline = Date.now() + 3000;
    let n = before;
    while (n - before < kNotifCount && Date.now() < deadline) {
      await sleep(50);
      n = countNotifs(frame);
    }
    if (n - before < kNotifCount) {
      throw new Error(`debug-show-notif: expected ${kNotifCount} notifications, got ${n - before}`);
    }
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
