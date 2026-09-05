// Issue #6053: Stop Reading must be available even when the playback bar is
// not on screen. It is disabled when nothing is being read.

import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { ROOT, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const PDF = join(ROOT, "tests", "issue-1189.pdf");

export async function testit(): Promise<void> {
  const { proc, client } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    const res = await client.request(ControlCommand.TestCommandVisibility, ["CmdStopReadAloud", "menu"]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`issue-6053: ${raw.trim()}`);
    }
    if (!/vis=disable/.test(raw)) {
      throw new Error(`issue-6053: Stop Reading should be disabled when idle: ${raw}`);
    }
    const stop = await client.request(ControlCommand.TestInvokeCommand, ["CmdStopReadAloud"]);
    if (stop[0] !== 0) {
      throw new Error(`issue-6053: CmdStopReadAloud: ${String(stop[1] ?? "")}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
