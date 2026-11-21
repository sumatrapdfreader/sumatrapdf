// At startup the app deletes cached copies nobody opened in a while: network
// drive comics in <data>/cbx-cache after 7 days, OneNote / Outlook extracts in
// <data>/open-cache after a day. ng used to keep them forever.
//
// Run: bun tests/stale-cache-cleanup.ts [-ng] [--no-build]

import { existsSync, mkdirSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kDaySec = 24 * 60 * 60;

// where each app keeps its caches: ng under -appdata, orig per user
function dataDir(appdata: string): string {
  if (USE_NG) {
    return join(appdata, "data");
  }
  return join(process.env.LOCALAPPDATA ?? "", "SumatraPDF-data");
}

function makeOld(path: string, ageDays: number): void {
  const t = Date.now() / 1000 - ageDays * kDaySec;
  utimesSync(path, t, t);
}

export async function testit(): Promise<void> {
  if (!IS_WIN) {
    console.log("SKIP stale-cache-cleanup: the caches are Windows only");
    return;
  }
  const appdata = tmpPath("stale-cache-cleanup");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const data = dataDir(appdata);
  const cbxDir = join(data, "cbx-cache");
  const openDir = join(data, "open-cache");
  mkdirSync(cbxDir, { recursive: true });
  mkdirSync(openDir, { recursive: true });
  const id = `stale-cache-cleanup-${process.pid}`;
  const staleCbx = join(cbxDir, `${id}.cbz`);
  const freshCbx = join(cbxDir, `${id}-fresh.cbz`);
  const staleOpen = join(openDir, `${id}.one`);
  writeFileSync(staleCbx, "x");
  writeFileSync(freshCbx, "x");
  writeFileSync(staleOpen, "x");
  makeOld(staleCbx, 30);
  makeOld(staleOpen, 3);

  const { proc, client } = await launchControlled(["-appdata", appdata]);
  try {
    await pollUntil(
      () => existsSync(staleCbx) || existsSync(staleOpen),
      (left) => !left,
      { timeoutMs: 10000, error: "stale-cache-cleanup: stale cache files were not deleted" },
    );
    if (!existsSync(freshCbx)) {
      throw new Error("stale-cache-cleanup: a recently used cache file was deleted");
    }
  } finally {
    client.close();
    await killAndWait(proc);
    for (const f of [staleCbx, freshCbx, staleOpen]) {
      rmSync(f, { force: true });
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
