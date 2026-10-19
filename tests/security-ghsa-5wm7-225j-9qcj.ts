// Regression test for GHSA-5wm7-225j-9qcj. Commands that launch programs
// (CmdExec etc.) must not be accepted from other processes over DDE /
// WM_COPYDATA: [CmdExec cmd.exe /c ...] must not run cmd.exe.

import { existsSync, rmSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { sendCopyDataW, sleep } from "./winapi.ts";
import { launchSumatra, waitForFrame, killAndWait } from "./win-automation.ts";

const kCopyDataDdeW = 0x44646557;

export async function testit(): Promise<void> {
  // WM_COPYDATA is Windows-only
  if (!IS_WIN) {
    return;
  }
  const marker = tmpPath("ghsa-5wm7-225j-9qcj.marker");
  rmSync(marker, { force: true });

  const pdf = join(import.meta.dir, "issue-1189.pdf");
  const proc = launchSumatra([pdf]);
  try {
    const frame = await waitForFrame(proc.pid);
    if (!frame) {
      throw new Error("SumatraPDF window did not appear");
    }

    const cmdExe = join(process.env.SystemRoot ?? "C:\\Windows", "System32", "cmd.exe");
    sendCopyDataW(frame, kCopyDataDdeW, `[CmdExec ${cmdExe} /c echo x> ${marker}]`);

    // ShellExecute is async; give cmd.exe time to write the marker
    const deadline = Date.now() + 2000;
    while (Date.now() < deadline) {
      if (existsSync(marker)) {
        throw new Error("CmdExec over WM_COPYDATA launched a program");
      }
      await sleep(50);
    }
  } finally {
    if (proc.exitCode === null) {
      await killAndWait(proc);
    }
    rmSync(marker, { force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
