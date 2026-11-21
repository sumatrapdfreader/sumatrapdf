// Explorer's Space preview (-quicklook) is handed to a SumatraPDF already running
// with the same exe and settings, and the launched process exits, as for
// ReuseInstance. ng used to open a window of its own instead.
//
// Run: bun tests/quicklook-forward.ts [-ng] [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { EXE, makeBlankPdf, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kExitWaitMs = 10000;

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP quicklook-forward: Explorer quick look is Windows only");
    return;
  }
  const dir = tmpPath("quicklook-forward");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    // not ReuseInstance: that forwards a plain open, not the preview
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nReuseInstance = false\n",
  );
  const pdf = join(dir, "preview.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");

  // forwarding is skipped for -for-testing runs
  const { proc, client } = await launchControlled(["-appdata", dir], { saveSettings: true });
  let second: Bun.Subprocess | undefined;
  try {
    await client.request(ControlCommand.Ping, []);
    second = Bun.spawn([EXE, "-appdata", dir, "-quicklook", pdf], { stdout: "ignore", stderr: "ignore" });
    const deadline = Date.now() + kExitWaitMs;
    while (second.exitCode === null && Date.now() < deadline) {
      await sleep(50);
    }
    if (second.exitCode === null) {
      throw new Error("quicklook-forward: -quicklook opened its own window instead of using the running instance");
    }
  } finally {
    if (second) {
      await killAndWait(second);
    }
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
