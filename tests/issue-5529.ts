// Regression test for issue #5529: WindowState must not be downgraded to normal
// while a document is still loading (slow opens with WindowState = maximized).
// Also checks the follow-up: a plain empty/home window (no loading tab) must
// still be allowed to record WIN_STATE_NORMAL so leftover maximized prefs are
// not sticky after closing a non-maximized home window.
//
// Launches Sumatra with no file (empty home, IsDocLoaded == false). The control
// handler simulates mid-load via a temporary Loading document tab (empty launch
// has no tabs otherwise), calls RememberDefaultWindowPosition with maximized
// prefs on a restored frame, then repeats without Loading for the empty case.
//
// Run:  bun tests/issue-5529.ts [--no-build]   (or via tests/run-almost-all.ts)

import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone } from "./util.ts";

async function requestWithRetry(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestWindowStateDuringLoad, []);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5529: window state downgraded during load: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5529: app never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  // no file args → about/home page, document not loaded
  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), []);
  console.log(`issue-5529: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
