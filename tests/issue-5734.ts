// Regression test for issue #5734.
//
// In the image resize dialog, arrow keys must adjust the resize handles even when
// keyboard focus is on the destination path edit (where Tab lands after the
// modeless-dialog focus changes in 3.7).

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// 1x1 red PNG
const PNG_1X1 = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==",
  "base64",
);

async function requestWithRetry(client: ControlClient, imagePath: string): Promise<string> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestImageResizeArrowKey, [imagePath]);
    const exitCode = res[0] as number;
    const raw = (res[1] as string) ?? "";
    if (!raw.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`issue-5734: arrow key did not resize: ${raw.trim()}`);
      }
      return raw.trim();
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5734: app never became ready: ${raw.trim()}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

export async function testit(): Promise<void> {
  const imagePath = tmpPath("issue-5734.png");
  writeFileSync(imagePath, PNG_1X1);

  const result = await withControlledSumatra(EXE, (client) => requestWithRetry(client, imagePath), [imagePath]);
  console.log(`issue-5734: ${result}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}