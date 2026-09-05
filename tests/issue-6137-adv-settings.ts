// #6137: Advanced Settings in-place value edit must survive OSK focus loss
// (WM_KILLFOCUS with no new window) and the WM_SIZE the keyboard sends when
// it docks. Esc still cancels.
//
// Run: bun tests/issue-6137-adv-settings.ts [--no-build]

import { mkdirSync, rmSync } from "node:fs";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

async function adv(client: ControlClient, action: string): Promise<string> {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, [action]);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      return out;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-6137-adv-settings: ${action} failed: ${out.trim()}`);
    }
    await sleep(50);
  }
  throw new Error(`issue-6137-adv-settings: Advanced Settings never ready for ${action}`);
}

function mustEditing(out: string, want: boolean, what: string): void {
  if (out.includes("editing=1") !== want) {
    throw new Error(`issue-6137-adv-settings: ${what}: ${out.trim()}`);
  }
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("issue-6137-adv-settings");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });

  const { proc, client, frame } = await launchControlled(["-appdata", appdata]);
  try {
    sendCommandSync(frame, cmdId("CmdAdvancedSettings"));
    mustEditing(await adv(client, "edit"), true, "edit did not open");
    await sleep(100);
    mustEditing(await adv(client, "killfocus"), true, "OSK kill-focus closed the editor");
    mustEditing(await adv(client, "resize"), true, "OSK resize closed the editor");
    const esc = await adv(client, "esc");
    if (!esc.includes("closed=0")) {
      throw new Error(`issue-6137-adv-settings: Esc closed the dialog: ${esc.trim()}`);
    }
    mustEditing(await adv(client, "state"), false, "Esc did not cancel the editor");
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
