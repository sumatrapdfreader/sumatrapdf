// Turning UseTabs off while documents are open moves each tab into a window of
// its own, and turning it back on gathers them as tabs of one window. ng used
// to leave the windows as they were until a restart.
//
// Run: bun tests/use-tabs-transition.ts [-ng] [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { makeBlankPdf, pollUntil, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { FRAME_CLASS, killAndWait, launchControlled } from "./win-automation.ts";
import { enumWindows, getClassName, getWindowPid, isWindowVisible } from "./winapi.ts";

const kSettings = "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\n";

function frameCount(pid: number): number {
  let n = 0;
  enumWindows((h) => {
    if (getWindowPid(h) === pid && isWindowVisible(h) && getClassName(h) === FRAME_CLASS) {
      n++;
    }
    return true;
  });
  return n;
}

async function waitFrames(pid: number, want: number, what: string): Promise<void> {
  await pollUntil(
    () => frameCount(pid),
    (n) => n === want,
    { timeoutMs: 10000, error: (n) => `use-tabs-transition: ${what}: ${n} windows, want ${want}` },
  );
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP use-tabs-transition: counts Windows frames");
    return;
  }
  const appdata = writeAppdata("use-tabs-transition", `${kSettings}UseTabs = true\n`);
  const docs = ["a", "b"].map((name) => {
    const path = tmpPath(`use-tabs-transition-${name}.pdf`);
    writeFileSync(path, makeBlankPdf(), "latin1");
    return path;
  });
  // the change is saved and read back, which -for-testing doesn't do
  const { proc, client } = await launchControlled(["-appdata", appdata, ...docs], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await waitFrames(proc.pid!, 1, "with tabs");

    await client.request(ControlCommand.TestSetSetting, ["UseTabs", "false"]);
    await waitFrames(proc.pid!, 2, "after UseTabs = false");

    await client.request(ControlCommand.TestSetSetting, ["UseTabs", "true"]);
    await waitFrames(proc.pid!, 1, "after UseTabs = true");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
