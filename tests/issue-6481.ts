// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6481
//
// A document opened from Outlook is our copy under <data dir>/open-cache. Moving
// its tab to another window, or closing a duplicate of it, deleted the copy
// while a tab still showed it, so moving the tab again failed with "file does
// not exist".
//
// Run: bun tests/issue-6481.ts [--no-build] [-ng]

import { copyFileSync, existsSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, USE_NG, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

// where each app keeps open-cache (GetSumatraDataDirTemp): orig ignores -appdata
function openCacheDir(appData: string): string {
  if (USE_NG) {
    return join(appData, "data", "open-cache");
  }
  return join(process.env.LOCALAPPDATA ?? "", "SumatraPDF-data", "open-cache");
}

type TabRow = { win: number; tab: number; ctrl: boolean; path: string };

async function migrate(client: ControlClient, srcWin: number, tabIdx: number, dstWin: number): Promise<TabRow[]> {
  const res = await client.request(ControlCommand.TestMigrateTab, [srcWin, tabIdx, dstWin]);
  const out = String(res[1] ?? "");
  if (res[0] !== 0) {
    throw new Error(`#6481: TestMigrateTab failed: ${out}`);
  }
  return [...out.matchAll(/^win=(\d+) tab=(\d+) ctrl=(\d) path=(.*)$/gm)].map((m) => ({
    win: +m[1]!,
    tab: +m[2]!,
    ctrl: m[3] === "1",
    path: m[4]!.trim(),
  }));
}

// polls until the cached document is loaded in `count` tabs, in window `win` if given
async function waitLoaded(client: ControlClient, cached: string, count: number, win?: number): Promise<TabRow[]> {
  const deadline = Date.now() + 10000;
  for (;;) {
    const rows = (await migrate(client, 0, -1, 0)).filter((r) => r.path === cached);
    const ok = rows.filter((r) => r.ctrl && (win === undefined || r.win === win));
    if (ok.length === count) {
      return ok;
    }
    if (Date.now() > deadline) {
      throw new Error(`#6481: expected ${count} loaded tab(s) of the cached copy, got ${JSON.stringify(rows)}`);
    }
    await sleep(50);
  }
}

function checkExists(cached: string, after: string): void {
  if (!existsSync(cached)) {
    throw new Error(`#6481: the cached copy was deleted after ${after} while a tab still shows it`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6481-appdata");
  rmSync(dir, { recursive: true, force: true });
  // a folder of our own: orig's open-cache is the user's real one
  const cacheDir = join(openCacheDir(dir), `issue-6481-${process.pid}`);
  try {
    mkdirSync(dir, { recursive: true });
    mkdirSync(cacheDir, { recursive: true });
    const cached = join(cacheDir, "attachment.pdf");
    copyFileSync(join(ROOT, "tests", "issue-1189.pdf"), cached);
    const other = join(dir, "other.pdf");
    copyFileSync(join(ROOT, "tests", "issue-1189.pdf"), other);

    const { proc, client } = await launchControlled(["-appdata", dir, other, cached]);
    try {
      await client.waitForRenderIdle();
      const [start] = await waitLoaded(client, cached, 1);

      // drag out into a new window, then back
      await migrate(client, start!.win, start!.tab, -1);
      const [moved] = await waitLoaded(client, cached, 1, 1);
      checkExists(cached, "moving its tab to a new window");
      await migrate(client, moved!.win, moved!.tab, 0);
      await waitLoaded(client, cached, 1, 0);
      checkExists(cached, "moving its tab back");

      // the moved-back tab is current: duplicate it, close the duplicate
      await client.request(ControlCommand.TestInvokeCommand, ["CmdDuplicateInNewTab"]);
      await waitLoaded(client, cached, 2);
      await client.request(ControlCommand.TestInvokeCommand, ["CmdClose"]);
      await waitLoaded(client, cached, 1);
      checkExists(cached, "closing a duplicate tab");
    } finally {
      client.close();
      await killAndWait(proc);
    }
  } finally {
    rmSync(dir, { recursive: true, force: true });
    rmSync(cacheDir, { recursive: true, force: true });
  }
  console.log("issue-6481: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
