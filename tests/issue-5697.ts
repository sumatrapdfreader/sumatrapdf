// Regression test for issue #5697: internationalize error/crash UI strings.
//
// Verifies _TRA resolves runtime error strings through the translation table.

import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

async function requestWithRetry(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestI18nErrorString, []);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5697: _TRA strings not resolved: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5697: app never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), []);
  console.log(`issue-5697: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}