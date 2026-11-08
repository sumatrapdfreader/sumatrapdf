// Starting keyboard text selection (F7) while placing a pointer annotation
// (line, ink, shape) ends the placement, as starting a placement ends keyboard
// selection. Both used to stay on, splitting the keyboard between them.
//
// ng only: orig has no UI state dump to read the modes from.
// Run: bun tests/f7-ends-placement.ts [--no-build] -ng

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { pollUntil, ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const PDF = join(ROOT, "tests", "issue-2423.pdf");

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

async function cmd(client: ControlClient, name: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [name]);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP f7-ends-placement: ng only (run with -ng)");
    return;
  }
  const { proc, client } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    await cmd(client, "CmdCreateAnnotLine");
    await pollUntil(
      async () => uiState(client),
      (s) => /\bplacing=1\b/.test(s),
      { error: (s) => `f7-ends-placement: line placement didn't start: ${s}` },
    );
    await cmd(client, "CmdSelectTextViaKeyboard");
    await pollUntil(
      async () => uiState(client),
      (s) => /\bplacing=0\b/.test(s) && /\btextSelectMode=1\b/.test(s),
      { error: (s) => `f7-ends-placement: want placing=0 textSelectMode=1: ${s}` },
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("f7-ends-placement: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
