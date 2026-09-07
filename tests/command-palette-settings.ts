// The command palette's "=" mode lists every scalar setting, not just the
// booleans. A bool still toggles on Enter; a number, string or enum opens a
// second stage ("=<name> = <value>") where the value is typed or picked from
// the allowed ones, and Enter applies it right away.
//
// The advanced settings dialog reads gSettings live, so its "nondefault" rows
// are how we check that a change actually landed. It needs a build that saves
// settings: -for-testing skips the save, and applying re-reads the file.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { WM_KEYDOWN, VK_RETURN, getFocusedHwnd, postMessage, sendText, sleep } from "./winapi.ts";

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
`;

type Palette = { open: boolean; items: number; queryLen: number };

async function paletteState(client: ControlClient): Promise<Palette> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const out = String(res[1] ?? "").trim();
  if (res[0] === 2) {
    return { open: false, items: 0, queryLen: 0 };
  }
  const m = /items=(\d+) querySel=-?\d+,-?\d+ queryLen=(\d+)/.exec(out);
  if (res[0] !== 0 || !m) {
    throw new Error(`command-palette-settings: TestCommandPalette failed: ${out}`);
  }
  return { open: true, items: +m[1]!, queryLen: +m[2]! };
}

// the palette re-filters its list asynchronously, so every step waits for the
// state it expects instead of sleeping a fixed amount
async function waitFor(client: ControlClient, what: string, pred: (p: Palette) => boolean): Promise<Palette> {
  const deadline = Date.now() + 8000;
  let last: Palette;
  for (;;) {
    last = await paletteState(client);
    if (pred(last)) {
      return last;
    }
    if (Date.now() > deadline) {
      throw new Error(`command-palette-settings: ${what}; palette is ${JSON.stringify(last)}`);
    }
    await sleep(100);
  }
}

async function openPalette(client: ControlClient, frame: number): Promise<number> {
  if (!(await paletteState(client)).open) {
    sendCommand(frame, cmdId("CmdCommandPalette"));
    await waitFor(client, "the palette never opened", (p) => p.open);
  }
  const edit = getFocusedHwnd(frame);
  if (!edit) {
    throw new Error("command-palette-settings: no query edit");
  }
  return edit;
}

// sets the query and waits until the list holds exactly wantItems rows
async function typeQuery(client: ControlClient, frame: number, query: string, wantItems: number): Promise<Palette> {
  const edit = await openPalette(client, frame);
  sendText(edit, query);
  return waitFor(
    client,
    `'${query}' did not produce ${wantItems} row(s)`,
    (p) => p.open && p.items === wantItems && p.queryLen === query.length,
  );
}

async function enter(client: ControlClient, frame: number, what: string, pred: (p: Palette) => boolean) {
  const edit = await openPalette(client, frame);
  postMessage(edit, WM_KEYDOWN, VK_RETURN, 0);
  return waitFor(client, what, pred);
}

// "name=value" for every setting that differs from its default
async function nonDefaultSettings(client: ControlClient, frame: number): Promise<Map<string, string>> {
  sendCommand(frame, cmdId("CmdAdvancedOptions"));
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
      await sleep(400);
      return rows;
    }
    if (Date.now() > deadline) {
      throw new Error("command-palette-settings: advanced settings dialog never opened");
    }
    await sleep(150);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("command-palette-settings");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  // saveSettings: applying a value re-reads the settings file, so the app must
  // be allowed to write it first
  const { proc, client, frame } = await launchControlled(["-appdata", appdata], { saveSettings: true });
  try {
    // a non-bool setting is in the list at all, and Enter on it asks for a
    // value instead of closing
    const name = "=ZoomIncrement";
    await typeQuery(client, frame, name, 1);
    await enter(
      client,
      frame,
      "Enter on a number setting did not ask for a value",
      (p) => p.open && p.queryLen > name.length,
    );

    // an enum offers its allowed values: "show", "hide" and "overlay" for Toolbar
    await typeQuery(client, frame, "=Toolbar =", 3);
    await typeQuery(client, frame, "=Toolbar = over", 1);

    // apply one of each kind; a leaf name resolves to its full dotted path
    for (const q of [
      "=ZoomIncrement = 25",
      "=ToolbarPosition = bottom",
      "=Units = cm",
      "=FixedPageUI.TextColor = #112233",
      "=SmoothScroll",
    ]) {
      await typeQuery(client, frame, q, 1);
      await enter(client, frame, `'${q}' did not close the palette`, (p) => !p.open);
    }

    const want: Record<string, string> = {
      ZoomIncrement: "25",
      ToolbarPosition: "bottom",
      "FixedPageUI.PageGrid.Units": "cm",
      "FixedPageUI.TextColor": "#112233",
      SmoothScroll: "false",
    };
    const got = await nonDefaultSettings(client, frame);
    for (const [name, value] of Object.entries(want)) {
      if (got.get(name) !== value) {
        throw new Error(`command-palette-settings: ${name} is '${got.get(name) ?? "(default)"}', expected '${value}'`);
      }
    }
    console.log("command-palette-settings: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
