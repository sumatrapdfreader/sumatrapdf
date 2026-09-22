// Editing the settings file while the command palette lists settings must not
// leave its rows pointing into the freed gSettings: the file watcher's reload
// re-creates the struct, and the rows show the values it now holds.
//
// Run: bun tests/command-palette-external-reload.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { WM_KEYDOWN, VK_ESCAPE, getFocusedHwnd, postMessage, sendText, sleep } from "./winapi.ts";

const kTag = "command-palette-external-reload";
const kBoolName = "DisableJavaScript";
const kReloadWaitMs = 10000;

function settingsText(boolValue: string): string {
  return `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n${kBoolName} = ${boolValue}\n`;
}

type Palette = { open: boolean; items: number; queryLen: number; sel: number; selValue: string; raw: string };

async function paletteState(client: ControlClient): Promise<Palette> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const out = String(res[1] ?? "").trim();
  if (res[0] === 2) {
    return { open: false, items: 0, queryLen: 0, sel: -1, selValue: "", raw: out };
  }
  const m = /sel=(-?\d+) items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+)/.exec(out);
  if (res[0] !== 0 || !m) {
    throw new Error(`${kTag}: TestCommandPalette failed: ${out}`);
  }
  const selValue = /selValue=(.*?) selText=/.exec(out)?.[1] ?? "";
  return { open: true, items: +m[2]!, queryLen: +m[3]!, sel: +m[1]!, selValue, raw: out };
}

async function waitFor(client: ControlClient, what: string, pred: (p: Palette) => boolean): Promise<Palette> {
  const deadline = Date.now() + 8000;
  for (;;) {
    const p = await paletteState(client);
    if (pred(p)) {
      return p;
    }
    if (Date.now() > deadline) {
      throw new Error(`${kTag}: ${what}; palette is ${JSON.stringify(p)}`);
    }
    await sleep(100);
  }
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
  writeFileSync(settingsPath, settingsText("true"));

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath]);
  try {
    await waitForLoads(logPath, 1);
    sendCommand(frame, cmdId("CmdCommandPalette"));
    await waitFor(client, "the palette never opened", (p) => p.open);
    const edit = getFocusedHwnd(frame);
    if (!edit) {
      throw new Error(`${kTag}: no query edit`);
    }
    const query = `=${kBoolName}`;
    sendText(edit, query);
    const row = await waitFor(
      client,
      `'${query}' did not produce one row`,
      (p) => p.open && p.items === 1 && p.sel === 0 && p.queryLen === query.length,
    );
    if (row.selValue !== "true") {
      throw new Error(`${kTag}: row shows '${row.selValue}', expected 'true' (${row.raw})`);
    }

    // an external edit flips the bool; the watcher reloads gSettings and the
    // row reads the new value through its offset. A stale pointer would read
    // freed memory (or crash under ASan)
    const before = loadCount(logPath);
    writeFileSync(settingsPath, settingsText("false"));
    await waitForLoads(logPath, before + 1);
    await waitFor(client, "row did not pick up the reloaded value", (p) => p.open && p.selValue === "false");

    postMessage(edit, WM_KEYDOWN, VK_ESCAPE, 0);
    await waitFor(client, "the palette did not close", (p) => !p.open);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(appdata, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
