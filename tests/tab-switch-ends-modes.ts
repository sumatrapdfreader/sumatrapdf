// Switching tabs ends the interaction modes of the tab being left: annotation
// placement and keyboard text selection hold page positions of that tab's
// document. Orig ends them in LoadModelIntoTab; ng kept them, so finishing a
// line or ink annotation in the new tab used the old document's points.
//
// ng only: orig has no UI state dump to read the modes from.
// Run: bun tests/tab-switch-ends-modes.ts [--no-build] -ng

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF1 = join(ROOT, "tests", "issue-2423.pdf");
const PDF2 = join(ROOT, "tests", "issue-1189.pdf");

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

async function mode(client: ControlClient, name: string): Promise<number> {
  const m = new RegExp(`\\b${name}=(\\d+)`).exec(await uiState(client));
  if (!m) {
    throw new Error(`tab-switch-ends-modes: no ${name} in UI state`);
  }
  return +m[1]!;
}

async function waitMode(client: ControlClient, name: string, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 5000;
  for (;;) {
    if ((await mode(client, name)) === want) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`tab-switch-ends-modes: ${what} (${name} should be ${want}): ${await uiState(client)}`);
    }
    await sleep(50);
  }
}

async function cmd(client: ControlClient, name: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [name]);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP tab-switch-ends-modes: ng only (run with -ng)");
    return;
  }
  const { proc, client } = await launchControlled([PDF1, PDF2]);
  try {
    await client.waitForRenderIdle();

    // tabs: Home, PDF1, PDF2 (current); stay between the two documents
    await cmd(client, "CmdCreateAnnotLine");
    await waitMode(client, "placing", 1, "line placement didn't start");
    await cmd(client, "CmdPrevTab");
    await waitMode(client, "placing", 0, "line placement survived the tab switch");

    await cmd(client, "CmdSelectTextViaKeyboard");
    await waitMode(client, "textSelectMode", 1, "keyboard selection didn't start");
    await cmd(client, "CmdNextTab");
    await waitMode(client, "textSelectMode", 0, "keyboard selection survived the tab switch");
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("tab-switch-ends-modes: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
