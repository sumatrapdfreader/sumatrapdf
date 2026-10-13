// Reloading a large settings file while a comic book is open crashed
// (crash 2026-10-13-13-57-ce12): ReloadSettings frees gSettings, the read of a
// file over 1 MB then asks the caches to trim, and the render cache's
// "is this page visible nearby" check read gSettings->comicBookUI.

import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { ROOT, pollUntil, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kTag = "reload-settings-memory-trim";
// kLargeAllocationSize in Base.h: smaller reads don't trim
const kLargeAllocationSize = 1024 * 1024;

// SaveMemory = 100 trims on every large allocation
function settingsText(showToc: boolean): string {
  const padLine = "# " + "x".repeat(98) + "\n";
  const pad = padLine.repeat(Math.ceil((2 * kLargeAllocationSize) / padLine.length));
  return `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nSaveMemory = 100\nShowToc = ${showToc}\n${pad}`;
}

function loadCount(logPath: string): number {
  try {
    return readFileSync(logPath, "utf8").split("LoadSettings(").length - 1;
  } catch {
    return 0;
  }
}

function waitForLoads(logPath: string, n: number): Promise<number> {
  return pollUntil(
    async () => loadCount(logPath),
    (count) => count >= n,
    {
      timeoutMs: 10000 * SLOW_BUILD_FACTOR,
      error: (count) => `${kTag}: ${count} settings loads, want ${n}`,
    },
  );
}

export async function testit(): Promise<void> {
  const appdata = tmpPath(kTag);
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const settingsPath = join(appdata, "SumatraPDF-settings.txt");
  const logPath = join(appdata, "log.txt");
  const cbz = join(appdata, "comic.cbz");
  copyFileSync(join(ROOT, "tests", "issue-1201.cbz"), cbz);
  writeFileSync(settingsPath, settingsText(false));

  // facing: single page view never asks the comic book settings
  const { proc, client } = await launchControlled([
    "-appdata",
    appdata,
    "-log-to-file",
    logPath,
    "-view",
    "facing",
    cbz,
  ]);
  try {
    await waitForLoads(logPath, 1);
    // the render cache needs a bitmap to consider freeing
    await client.waitForRenderIdle();

    const before = loadCount(logPath);
    writeFileSync(settingsPath, settingsText(true));
    await waitForLoads(logPath, before + 1);

    const ping = await client.request(ControlCommand.Ping);
    if (ping[0] !== "pong") {
      throw new Error(`${kTag}: ping after reload: ${JSON.stringify(ping)}`);
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
