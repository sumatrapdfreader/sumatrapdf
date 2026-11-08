// Alt alone puts the keyboard in the menu bar (a title highlighted, no menu
// open). Win32 lets go of it when the window is deactivated; ng kept it, so
// the keys typed after switching back went to the menu bar.
//
// ng only: orig's menu bar is win32's.
// Run: bun tests/menu-bar-armed-deactivate.ts [--no-build] -ng

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { pollUntil, ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sendMessage } from "./winapi.ts";

const PDF = join(ROOT, "tests", "issue-2423.pdf");
const VK_MENU = 0x12;
const kModAlt = 4;
const WM_ACTIVATE = 0x0006;
const WA_INACTIVE = 0;

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

async function input(client: ControlClient, args: (string | number)[]): Promise<void> {
  const res = String((await client.request(ControlCommand.TestInput, args))[1] ?? "");
  if (!res.startsWith("OK")) {
    throw new Error(`menu-bar-armed-deactivate: input ${JSON.stringify(args)}: ${res}`);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP menu-bar-armed-deactivate: ng only (run with -ng)");
    return;
  }
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    await input(client, ["key", VK_MENU, kModAlt]);
    await input(client, ["keyup", VK_MENU, 0]);
    await pollUntil(
      async () => uiState(client),
      (s) => /menuArmed=0\b/.test(s),
      { error: (s) => `menu-bar-armed-deactivate: Alt didn't arm the menu bar: ${s}` },
    );

    sendMessage(frame, WM_ACTIVATE, WA_INACTIVE, 0);
    await pollUntil(
      async () => uiState(client),
      (s) => /menuArmed=-1\b/.test(s),
      { error: (s) => `menu-bar-armed-deactivate: still armed after deactivation: ${s}` },
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("menu-bar-armed-deactivate: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
