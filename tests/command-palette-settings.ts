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
import {
  killAndWait,
  launchControlled,
  paletteSetText,
  sendCommand,
  waitForPalette,
  paletteState,
  type PaletteState,
} from "./win-automation.ts";
import { WM_KEYDOWN, VK_ESCAPE, VK_RETURN, postMessage, sleep } from "./winapi.ts";
const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
`;

// the palette re-filters its list asynchronously, so every step waits for the
// state it expects instead of sleeping a fixed amount
async function waitFor(client: ControlClient, what: string, pred: (p: PaletteState) => boolean): Promise<PaletteState> {
  const deadline = Date.now() + 8000;
  let last: PaletteState;
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

async function openPalette(client: ControlClient, frame: number): Promise<{ palette: number; edit: number }> {
  if (!(await paletteState(client)).open) {
    sendCommand(frame, cmdId("CmdCommandPalette"));
    await waitFor(client, "the palette never opened", (p) => p.open);
  }
  const handles = await waitForPalette(client, frame);
  if (!handles.edit) {
    throw new Error("command-palette-settings: no query edit");
  }
  return handles;
}

// sets the query and waits until the list holds exactly wantItems rows
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

async function enter(client: ControlClient, frame: number, what: string, pred: (p: PaletteState) => boolean) {
  return pressKey(client, frame, VK_RETURN, what, pred);
}

async function escape(client: ControlClient, frame: number, what: string, pred: (p: PaletteState) => boolean) {
  return pressKey(client, frame, VK_ESCAPE, what, pred);
}

// the palette is back in the setting-picking stage: query is "=" and the list
// holds every setting again
const backToSettings = (p: PaletteState) => p.open && p.queryLen === 1 && p.items > 1;

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

    // Esc from the value stage returns to the settings with that setting still
    // selected. It is not the first row: the changed settings come first
    const back = await escape(client, frame, "Esc after Enter did not return to the settings", backToSettings);
    if (back.selText !== "ZoomIncrement" || back.sel === 0) {
      throw new Error(
        `command-palette-settings: after Esc row ${back.sel} '${back.selText}' is selected, not ZoomIncrement`,
      );
    }

    // an enum offers its allowed values: "show", "hide" and "overlay" for Toolbar
    await typeQuery(client, frame, "=Toolbar =", 3);
    await typeQuery(client, frame, "=Toolbar = over", 1);

    // more enums: ThumbnailsType, a nested PageGrid.Units, an optional display mode
    await typeQuery(client, frame, "=ThumbnailsType =", 2);
    await typeQuery(client, frame, "=Units =", 4);
    await typeQuery(client, frame, "=ComicBookUI.DefaultDisplayMode =", 8); // "" is not a row

    // Esc in the value stage goes back to the settings, not out of the palette
    await escape(client, frame, "Esc in the value stage did not return to the settings", backToSettings);

    // apply one of each kind; a leaf name resolves to its full dotted path.
    // The palette stays open, showing the settings again with the changed
    // setting selected
    for (const [q, path] of [
      ["=ZoomIncrement = 25", "ZoomIncrement"],
      ["=ToolbarPosition = bottom", "ToolbarPosition"],
      ["=Units = cm", "FixedPageUI.PageGrid.Units"],
      ["=FixedPageUI.TextColor = #112233", "FixedPageUI.TextColor"],
      ["=SmoothScroll", "SmoothScroll"],
    ]) {
      await typeQuery(client, frame, q, 1);
      const p = await enter(client, frame, `'${q}' did not return to the settings`, backToSettings);
      if (p.selText !== path) {
        throw new Error(`command-palette-settings: after '${q}' row ${p.sel} '${p.selText}' is selected, not ${path}`);
      }
    }
    await escape(client, frame, "Esc in the settings stage did not close the palette", (p) => !p.open);

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
