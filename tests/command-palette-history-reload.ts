// A settings reload frees the FileState objects the palette's file history
// rows point at. Opening a history row after an external settings edit must
// open its file, not read the freed path.
//
// Run: bun tests/command-palette-history-reload.ts [--no-build] [-ng]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  killAndWait,
  launchControlled,
  ngKey,
  paletteSetText,
  paletteState,
  sendCommand,
  waitForPalette,
  waitForTitle,
  type PaletteState,
} from "./win-automation.ts";
import { postMessage, sleep, VK_RETURN, WM_KEYDOWN } from "./winapi.ts";

const kTag = "command-palette-history-reload";
const kReloadWaitMs = 10000;

function settingsText(paths: string[]): string {
  const states = paths.map((p) => `\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = 5\n\t]`).join("\n");
  return `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nFileStates [\n${states}\n]\n`;
}

async function waitFor(client: ControlClient, what: string, pred: (p: PaletteState) => boolean): Promise<PaletteState> {
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
  const dir = tmpPath(kTag);
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const doc = join(dir, "historydoc.pdf");
  const other = join(dir, "other.pdf");
  writeFileSync(doc, makePdf(1, 400, 0, 0), "latin1");
  writeFileSync(other, makePdf(1, 400, 0, 0), "latin1");
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  const logPath = join(appdata, "log.txt");
  writeFileSync(settingsPath, settingsText([doc]));

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath]);
  try {
    await waitForLoads(logPath, 1);
    sendCommand(frame, cmdId("CmdCommandPalette"));
    const handles = await waitForPalette(client, frame);
    if (!handles.edit) {
      throw new Error(`${kTag}: the palette never opened`);
    }
    const query = "#historydoc";
    await paletteSetText(client, handles, query);
    await waitFor(
      client,
      `'${query}' did not produce one row`,
      (p) => p.open && p.items === 1 && p.sel === 0 && p.queryLen === query.length,
    );

    // an external edit reloads gSettings, freeing the FileState of the row
    const before = loadCount(logPath);
    writeFileSync(settingsPath, settingsText([other, doc]));
    await waitForLoads(logPath, before + 1);
    await sleep(300);
    await waitFor(client, "the row is gone after the reload", (p) => p.open && p.items === 1 && p.sel === 0);

    if (USE_NG) {
      await ngKey(client, VK_RETURN);
    } else {
      postMessage(handles.edit, WM_KEYDOWN, VK_RETURN, 0);
    }
    await waitForTitle(frame, (t) => t.includes("historydoc.pdf")).catch((e) => {
      throw new Error(`${kTag}: the history row did not open its file (${e})`);
    });
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log(`${kTag}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
