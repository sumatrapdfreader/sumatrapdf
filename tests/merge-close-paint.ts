// Closing Merge PDF frees its files; the dialog's window lives a little longer
// and can still be asked to paint (activating the frame pumps messages when an
// IME is active). That paint read the freed files.
//
// Run: bun tests/merge-close-paint.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

async function merge(client: ControlClient, action: string, arg = ""): Promise<string> {
  const res = await client.request(ControlCommand.TestMergePdf, [action, arg, 0]);
  return String(res[1] ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("merge-close-paint");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  const other = join(dir, "other.pdf");
  writeFileSync(pdf, makePdf(3, 601, 0, 0), "latin1");
  writeFileSync(other, makePdf(2, 602, 0, 0), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommand(frame, cmdId("CmdMergePDF"));

    const deadline = Date.now() + 5000;
    while (!(await merge(client, "")).includes("items=0:1")) {
      if (Date.now() > deadline) {
        throw new Error("merge-close-paint: the dialog didn't open");
      }
      await sleep(100);
    }
    // two files: the legend is painted too
    const added = await merge(client, "add", other);
    if (!added.includes("added=1")) {
      throw new Error(`merge-close-paint: add: ${added}`);
    }

    const res = await merge(client, "closepaint");
    if (res !== "OK closed") {
      throw new Error(`merge-close-paint: closepaint: ${res}`);
    }
    // still running, the dialog gone
    await sleep(300);
    const after = await merge(client, "");
    if (!after.includes("no-dialog")) {
      throw new Error(`merge-close-paint: want no dialog after closing, got '${after}'`);
    }
    console.log("merge-close-paint: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
