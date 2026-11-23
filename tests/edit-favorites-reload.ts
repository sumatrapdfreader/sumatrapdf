// Edit Favorites keeps rows pointing at FileState / Favorite objects. A
// settings reload (here: an external edit of the settings file) frees them
// while the dialog is open; Delete must then act on the reloaded favorites.
//
// Run: bun tests/edit-favorites-reload.ts [--no-build] [-ng]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { expectFavs, makePdf, ngDriver, origDriver } from "./edit-favorites.ts";
import { runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kTag = "edit-favorites-reload";
const kReloadWaitMs = 10000;

function settingsText(pdf: string, extra: string): string {
  const fav = (page: number) => `\t\t\t[\n\t\t\t\tPageNo = ${page}\n\t\t\t]`;
  return [
    "UiLanguage = en",
    "CheckForUpdates = false",
    "RestoreSession = false",
    extra,
    "FileStates [",
    "\t[",
    `\t\tFilePath = ${pdf}`,
    "\t\tFavorites [",
    fav(1),
    fav(2),
    "\t\t]",
    "\t]",
    "]",
    "",
  ].join("\n");
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
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  const logPath = join(appdata, "log.txt");
  writeFileSync(settingsPath, settingsText(pdf, ""));

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath, pdf]);
  try {
    await client.waitForRenderIdle();
    await expectFavs(client, "OK manual=0 -@1,-@2", "loaded");

    const d = USE_NG ? ngDriver(client) : origDriver(client, frame, proc.pid!, dir);
    await d.open("CmdEditFavorites", "Edit Favorites");
    // rows: doc.pdf, Page 1, Page 2
    await d.selectRow(1);

    // the reload frees every FileState and Favorite the rows point at
    const before = loadCount(logPath);
    writeFileSync(settingsPath, settingsText(pdf, "ZoomIncrement = 25"));
    await waitForLoads(logPath, before + 1);
    await sleep(300);

    await d.click("Delete");
    await expectFavs(client, "OK manual=0 -@2", "Delete after the reload");
    if (proc.exitCode !== null) {
      throw new Error(`${kTag}: app exited (${proc.exitCode})`);
    }
    await d.click("Close");
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
