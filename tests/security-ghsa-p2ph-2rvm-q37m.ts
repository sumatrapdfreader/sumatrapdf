// Regression test for GHSA-p2ph-2rvm-q37m. CmdExec is reachable through
// WM_COPYDATA while the start page has no current document tab. It must ignore
// the command instead of passing a null WindowTab to RunWithExe and crashing.

import { EXE, runStandalone } from "./util.ts";
import { getWindowPid, sendCopyDataW, sleep } from "./winapi.ts";
import { launchSumatra, waitForFrame, killAndWait } from "./win-automation.ts";

const kCopyDataDdeW = 0x44646557;

export async function testit(): Promise<void> {
  const proc = launchSumatra([]);
  try {
    const frame = await waitForFrame(proc.pid);
    if (!frame) {
      throw new Error("SumatraPDF start-page window did not appear");
    }

    const missingExe = `${EXE}.ghsa-p2ph-2rvm-q37m-missing`;
    sendCopyDataW(frame, kCopyDataDdeW, `[CmdExec "${missingExe}"]`);
    // SendMessage already ran CmdExec; a crash still takes a beat to tear the
    // process down, so poll briefly instead of sleeping a fixed 500ms
    const deadline = Date.now() + 250;
    while (Date.now() < deadline) {
      if (proc.exitCode !== null || getWindowPid(frame) !== proc.pid) {
        throw new Error("CmdExec with no document tab terminated SumatraPDF");
      }
      await sleep(20);
    }
  } finally {
    if (proc.exitCode === null) {
      await killAndWait(proc);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
