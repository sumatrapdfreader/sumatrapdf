// Regression test for issue #5546: annotation type names and toolbar command
// descriptions must be translated (German UI).

import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone } from "./util.ts";

async function requestAnnotNames(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestAnnotReadableNames, []);
  const exitCode = res[0] as number;
  const raw = (res[1] as string) ?? "";
  if (exitCode !== 0) {
    throw new Error(`issue-5546: annotation names not translated: ${raw.trim()}`);
  }
  return raw.trim();
}

export async function testit(): Promise<void> {
  const result = await withControlledSumatra(EXE, (client) => requestAnnotNames(client), []);
  console.log(`issue-5546: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}