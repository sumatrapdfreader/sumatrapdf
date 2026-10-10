// Regression test for issue #5736.
//
// In the floating find window, match-highlight backgrounds must not bleed into
// the fixed page-number column when the window is narrow.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, makeOneLinePdf } from "./util.ts";

async function requestWithRetry(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFindResultPageColumnClip, []);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5736: highlight bled into page column: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5736: app never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-5736.pdf");
  writeFileSync(pdfPath, makeOneLinePdf("hello world"));

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client), [pdfPath]);
  console.log(`issue-5736: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
