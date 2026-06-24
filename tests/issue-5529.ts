// Regression test for issue #5529: WindowState must not be downgraded to normal
// while a document is still loading (slow opens with WindowState = maximized).

import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
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
  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), []);
  console.log(`issue-5529: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}