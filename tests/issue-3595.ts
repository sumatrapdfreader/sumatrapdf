// Regression test for issue #3595: a file that fails to load should keep a tab
// so Open in external viewer and Show in Folder remain available.

import { writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

async function requestWithRetry(client: ControlClient, badPath: string): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFailedLoadTab, [badPath]);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-3595: failed-load tab missing or commands hidden: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-3595: app never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const badPath = resolve(tmpPath("issue-3595-bad.pdf"));
  writeFileSync(badPath, "not a valid pdf");

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client, badPath), []);
  console.log(`issue-3595: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}