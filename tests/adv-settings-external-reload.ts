// Editing the settings file while Advanced Settings is open used to crash:
// the file watcher's reload freed gSettings, and the dialog's items still
// pointed into the old struct (crash 2026-09-22-04-47-f29f).
//
// Run: bun tests/adv-settings-external-reload.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { ControlCommand, type ControlClient } from "./control.ts";

const kTag = "adv-settings-external-reload";
// first Bool in the dialog's item order; "toggle 0" reports it, so a
// reordering fails loudly instead of editing some other setting
const kBoolName = "DisableJavaScript";
const kReloadWaitMs = 10000;

async function adv(client: ControlClient, action: string, arg = 0): Promise<string> {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, [action, arg]);
    const exitCode = res[0] as number;
    const out = String(res[1] ?? "");
    if (exitCode === 0) {
      return out;
    }
    if (exitCode !== 2) {
      throw new Error(`${kTag}: TestAdvSettingsRows(${action}) failed: ${out.trim()}`);
    }
    await sleep(50);
  }
  throw new Error(`${kTag}: Advanced Settings never ready for ${action}`);
}

// "A.B" -> "A [\n\tB = v\n]", "A" -> "A = v"
function settingsLine(name: string, value: string): string {
  const parts = name.split(".");
  if (parts.length === 1) {
    return `${name} = ${value}\n`;
  }
  if (parts.length !== 2) {
    throw new Error(`${kTag}: ${name} nests deeper than this test can write`);
  }
  return `${parts[0]} [\n\t${parts[1]} = ${value}\n]\n`;
}

function settingsText(boolValue: string): string {
  return `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n${settingsLine(kBoolName, boolValue)}`;
}

function loadCount(logPath: string): number {
  try {
    return readFileSync(logPath, "utf8").split("LoadSettings(").length - 1;
  } catch {
    return 0;
  }
}

async function waitForLoads(logPath: string, n: number): Promise<void> {
  const deadline = Date.now() + kReloadWaitMs;
  while (Date.now() < deadline) {
    if (loadCount(logPath) >= n) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`${kTag}: settings were not reloaded ${n} times within ${kReloadWaitMs} ms`);
}

export async function testit(): Promise<void> {
  const appdata = tmpPath(kTag);
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  const logPath = join(appdata, "log.txt");
  writeFileSync(settingsPath, settingsText("false"));

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath]);
  try {
    await waitForLoads(logPath, 1);
    sendCommandSync(frame, cmdId("CmdAdvancedSettings"));

    // sanity: the toggled item is the one the file edit below flips
    const first = await adv(client, "toggle", 0);
    if (!first.includes(`toggled=${kBoolName} `) || !first.includes("changed=1")) {
      throw new Error(`${kTag}: unexpected first toggle: ${first.trim()}`);
    }
    const back = await adv(client, "toggle", 0);
    if (!back.includes("changed=0")) {
      throw new Error(`${kTag}: toggling back did not clear the change: ${back.trim()}`);
    }

    // an external edit sets the bool to true; the watcher reloads gSettings
    const before = loadCount(logPath);
    writeFileSync(settingsPath, settingsText("true"));
    await waitForLoads(logPath, before + 1);

    // the pending value now equals what's on disk, so nothing is changed.
    // With the stale pointer this compared against freed memory (or crashed)
    const after = await adv(client, "toggle", 0);
    if (!after.includes("changed=0")) {
      throw new Error(`${kTag}: toggle after external reload: ${after.trim()}`);
    }
    const closed = await adv(client, "esc");
    if (!closed.includes("closed=1")) {
      throw new Error(`${kTag}: esc did not close the dialog: ${closed.trim()}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
