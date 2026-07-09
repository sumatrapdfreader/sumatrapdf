// Ad-hoc repro for: "can open Advanced Settings once; after closing, reopening
// does nothing". Opens the dialog via CmdAdvancedSettings, closes it, and opens
// it again, checking each time that a top-level "Advanced Settings" window
// appears. Run: bun tests/ad-hoc-adv-settings-reopen.ts
import { cmdId } from "./util.ts";
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation.ts";
import { enumWindows, getWindowText, getWindowPid, postMessage, sleep } from "./winapi.ts";

const CmdAdvancedSettings = cmdId("CmdAdvancedSettings");
const WM_CLOSE = 0x0010;
const PDF = "ext/zlib/zlib.3.pdf";

function findDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) !== pid) return true;
    const t = getWindowText(hwnd);
    if (t.includes("Advanced Settings")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitForDialog(pid: number, timeoutMs = 4000): Promise<number> {
  const end = Date.now() + timeoutMs;
  while (Date.now() < end) {
    const h = findDialog(pid);
    if (h) return h;
    await sleep(100);
  }
  return 0;
}

const proc = launchSumatra([PDF]);
const frame = await waitForFrame(proc.pid!);
if (!frame) throw new Error("no frame");
await sleep(500);

async function openCheckClose(round: number): Promise<boolean> {
  sendCommand(frame, CmdAdvancedSettings);
  const dlg = await waitForDialog(proc.pid!);
  console.log(`round ${round}: dialog hwnd = ${dlg ? "0x" + dlg.toString(16) : "NOT FOUND"}`);
  if (!dlg) return false;
  postMessage(dlg, WM_CLOSE, 0, 0);
  // wait until it's gone
  const end = Date.now() + 3000;
  while (Date.now() < end && findDialog(proc.pid!)) await sleep(100);
  const stillThere = findDialog(proc.pid!);
  console.log(`round ${round}: after close, dialog ${stillThere ? "STILL PRESENT" : "closed"}`);
  await sleep(400);
  return true;
}

const r1 = await openCheckClose(1);
const r2 = await openCheckClose(2);
const r3 = await openCheckClose(3);

proc.kill();
console.log(`\nresult: round1=${r1} round2=${r2} round3=${r3}`);
if (r1 && r2 && r3) {
  console.log("PASS: dialog reopened every time");
} else {
  console.log("FAIL: dialog did not reopen (bug reproduced)");
  process.exit(1);
}
