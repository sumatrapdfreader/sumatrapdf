// Delete puts a setting back to its default: in the command palette's
// "= settings" list and in the Advanced Settings dialog (Cmd+Backspace on
// macOS, which has no Delete key on most keyboards).
//
// Run: bun tests/settings-restore-default.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  killAndWait,
  launchControlled,
  paletteSetText,
  sendCommand,
  waitForPalette,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";
import { WM_KEYDOWN, VK_DELETE, VK_ESCAPE, VK_RETURN, postMessage, sleep } from "./winapi.ts";
const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
`;

async function waitFor(client: ControlClient, what: string, pred: (p: PaletteState) => boolean): Promise<PaletteState> {
  const deadline = Date.now() + 8000;
  let last = await paletteState(client);
  while (!pred(last)) {
    if (Date.now() > deadline) {
      throw new Error(`settings-restore-default: ${what}; palette is ${JSON.stringify(last)}`);
    }
    await sleep(40);
    last = await paletteState(client);
  }
  return last;
}

async function openPalette(client: ControlClient, frame: number): Promise<{ palette: number; edit: number }> {
  if (!(await paletteState(client)).open) {
    sendCommand(frame, cmdId("CmdCommandPalette"));
    await waitFor(client, "the palette never opened", (p) => p.open);
  }
  const handles = await waitForPalette(client, frame);
  if (!handles.edit) {
    throw new Error("settings-restore-default: no query edit");
  }
  return handles;
}

async function typeQuery(
  client: ControlClient,
  frame: number,
  query: string,
  wantItems: number,
): Promise<PaletteState> {
  const handles = await openPalette(client, frame);
  await paletteSetText(client, handles, query);
  return waitFor(
    client,
    `'${query}' did not produce ${wantItems} row(s)`,
    (p) => p.open && p.items === wantItems && p.queryLen === query.length,
  );
}

async function pressKey(
  client: ControlClient,
  frame: number,
  vk: number,
  what: string,
  pred: (p: PaletteState) => boolean,
) {
  const { edit } = await openPalette(client, frame);
  postMessage(edit, WM_KEYDOWN, vk, 0);
  return waitFor(client, what, pred);
}

// back in the setting-picking stage with `path` selected
const backOn = (path: string) => (p: PaletteState) => p.open && p.queryLen === 1 && p.items > 1 && p.selText === path;

// "name=value" for every setting that differs from its default
async function nonDefaultSettings(client: ControlClient, frame: number): Promise<Map<string, string>> {
  sendCommand(frame, cmdId("CmdAdvancedSettings"));
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, ["nondefault", 0]);
    if (res[0] === 0) {
      const rows = new Map<string, string>();
      for (const line of String(res[1] ?? "").split("\n")) {
        const m = /^(\S+)=(.*) default=/.exec(line.trim());
        if (m) {
          rows.set(m[1]!, m[2]!);
        }
      }
      await client.request(ControlCommand.TestAdvSettingsRows, ["esc", 0]);
      return rows;
    }
    if (Date.now() > deadline) {
      throw new Error("settings-restore-default: advanced settings dialog never opened");
    }
    await sleep(150);
  }
}

async function advRows(client: ControlClient, action: string, arg = 0): Promise<string> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestAdvSettingsRows, [action, arg]);
    const out = String(res[1] ?? "").trim();
    if (res[0] === 0) {
      return out;
    }
    if (res[0] !== 2 || Date.now() > deadline) {
      throw new Error(`settings-restore-default: advanced settings '${action}': ${out}`);
    }
    await sleep(150);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("settings-restore-default");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  // saveSettings: applying a value re-reads the settings file, so the app must
  // be allowed to write it first
  const { proc, client, frame } = await launchControlled(["-appdata", appdata], { saveSettings: true });
  try {
    // palette: a toggled bool and a typed number, each put back with Delete
    await typeQuery(client, frame, "=SmoothScroll", 1);
    let p = await pressKey(client, frame, VK_RETURN, "Enter did not toggle SmoothScroll", backOn("SmoothScroll"));
    if (p.selValue !== "false") {
      throw new Error(`settings-restore-default: SmoothScroll is '${p.selValue}' after Enter, expected 'false'`);
    }
    p = await pressKey(
      client,
      frame,
      VK_DELETE,
      "Delete did not restore SmoothScroll",
      (q) => backOn("SmoothScroll")(q) && q.selValue === "true",
    );

    await typeQuery(client, frame, "=ZoomIncrement = 25", 1);
    p = await pressKey(client, frame, VK_RETURN, "Enter did not apply ZoomIncrement", backOn("ZoomIncrement"));
    if (p.selValue !== "25") {
      throw new Error(`settings-restore-default: ZoomIncrement is '${p.selValue}' after Enter, expected '25'`);
    }
    // Delete in the value stage restores the setting being edited
    await typeQuery(client, frame, "=ZoomIncrement = 33", 1);
    p = await pressKey(
      client,
      frame,
      VK_DELETE,
      "Delete in the value stage did not restore ZoomIncrement",
      (q) => backOn("ZoomIncrement")(q) && q.selValue !== "25" && q.selValue !== "33",
    );
    const zoomDefault = p.selValue;
    await pressKey(client, frame, VK_ESCAPE, "Esc did not close the palette", (q) => !q.open);

    const got = await nonDefaultSettings(client, frame);
    for (const name of ["SmoothScroll", "ZoomIncrement"]) {
      if (got.has(name)) {
        throw new Error(`settings-restore-default: ${name} is still '${got.get(name)}' after Delete`);
      }
    }

    // the dialog: a toggled bool goes back with Delete on its row, which leaves
    // nothing unsaved so Esc closes the dialog
    sendCommand(frame, cmdId("CmdAdvancedSettings"));
    let out = await advRows(client, "toggle", 0);
    if (!/changed=1/.test(out)) {
      throw new Error(`settings-restore-default: toggle: ${out}`);
    }
    out = await advRows(client, "restore", 0);
    if (!/restored=\w+ changed=0/.test(out)) {
      throw new Error(`settings-restore-default: restore: ${out}`);
    }
    out = await advRows(client, "esc", 0);
    if (!/closed=1/.test(out)) {
      throw new Error(`settings-restore-default: Esc after restoring did not close the dialog: ${out}`);
    }
    console.log(`settings-restore-default: OK (ZoomIncrement default ${zoomDefault})`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
