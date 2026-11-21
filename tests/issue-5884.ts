// LazyLoading must not touch a restored tab's file before the tab is selected
// (follow-up in #5884): every check of a file on a sleeping drive spins it up,
// so a session on several drives waited for each of them at startup.
//
// The session has three tabs in three folders: a.pdf (selected), b.pdf and a
// missing c.pdf. After the restore only a.pdf's folder may be watched and the
// missing file must not have been noticed yet; selecting b.pdf loads it in the
// background, without freezing the UI while the (simulated) drive spins up.
//
// Run: bun tests/issue-5884.ts [--no-build]

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const LOAD_DELAY_MS = 3000;

function setup(dir: string): { a: string; b: string; c: string } {
  rmSync(dir, { recursive: true, force: true });
  for (const sub of ["dirA", "dirB", "dirC"]) {
    mkdirSync(join(dir, sub), { recursive: true });
  }
  const a = join(dir, "dirA", "a.pdf");
  const b = join(dir, "dirB", "b.pdf");
  const c = join(dir, "dirC", "c.pdf");
  copyFileSync(SRC_PDF, a);
  copyFileSync(SRC_PDF, b);
  const tab = (p: string) => `\t\t\t[\n\t\t\t\tFilePath = ${p}\n\t\t\t]`;
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = true\nRememberOpenedFiles = true\n` +
      `LazyLoading = true\nReloadModifiedDocuments = true\nKeepMissingSessionTabs = true\n` +
      `SessionData [\n\t[\n\t\tTabStates [\n${[tab(a), tab(b), tab(c)].join("\n")}\n\t\t]\n\t\tTabIndex = 1\n\t]\n]\n`,
  );
  return { a, b, c };
}

function readLog(path: string): string {
  return existsSync(path) ? readFileSync(path, "utf8") : "";
}

// a folder the file watcher started watching
// Windows watches a file's folder; macOS polls the file itself
function watched(log: string, dir: string): boolean {
  const lines = log.split("\n");
  if (IS_MAC) {
    return lines.some((l) => l.startsWith("FileWatcherSubscribe: watching") && l.includes(`'${dir}/`));
  }
  const want = dir.replaceAll("/", "\\").toLowerCase();
  return lines.some((l) => l.startsWith("StartMonitoringDirForChangesAPC()") && l.toLowerCase().includes(want));
}

async function waitLog(path: string, pred: (log: string) => boolean): Promise<boolean> {
  const deadline = Date.now() + 8000;
  while (Date.now() < deadline) {
    if (pred(readLog(path))) {
      return true;
    }
    await sleep(100);
  }
  return pred(readLog(path));
}

async function selectTab(client: ControlClient, idx: number): Promise<void> {
  const res = await client.request(ControlCommand.TestStripGroups, ["select", idx]);
  if (res[0] !== 0) {
    throw new Error(`issue-5884: select tab ${idx}: ${String(res[1] ?? "")}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5884");
  setup(dir);
  const logPath = join(dir, "log.txt");
  const { proc, client } = await launchControlled(["-appdata", dir, "-log-to-file", logPath], { saveSettings: true });
  try {
    await client.waitForSessionRestored();
    await client.waitForRenderIdle();
    if (!(await waitLog(logPath, (l) => watched(l, join(dir, "dirA"))))) {
      throw new Error("issue-5884: the selected tab's folder isn't watched");
    }
    // give anything that would touch the other files time to do it
    await sleep(500);
    const log = readLog(logPath);
    if (watched(log, join(dir, "dirB"))) {
      throw new Error("issue-5884: a lazy tab's folder is watched before the tab is selected");
    }
    if (log.includes("NewMissingSessionTab")) {
      throw new Error("issue-5884: a lazy tab's file was checked before the tab was selected");
    }

    // a lazy tab loads in the background: selecting it while its drive spins
    // up must not freeze the UI (TestStripGroups runs on the UI thread)
    await client.request(ControlCommand.TestLoadDelay, [LOAD_DELAY_MS]);
    const t0 = Date.now();
    // tabs: Home, a.pdf, b.pdf, c.pdf
    await selectTab(client, 2);
    const selectMs = Date.now() - t0;
    if (selectMs >= LOAD_DELAY_MS / 2) {
      throw new Error(`issue-5884: selecting a lazy tab blocked the UI for ${selectMs} ms`);
    }
    if (!(await waitLog(logPath, (l) => watched(l, join(dir, "dirB"))))) {
      throw new Error("issue-5884: selecting the lazy tab didn't load it");
    }
    await client.request(ControlCommand.TestLoadDelay, [0]);
    // the missing file is noticed when its tab is selected
    await selectTab(client, 3);
    const missing = /(not found|does not exist).*c\.pdf|c\.pdf.*(not found|does not exist)/i;
    if (!(await waitLog(logPath, (l) => missing.test(l)))) {
      throw new Error("issue-5884: selecting the missing file's tab didn't report it");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
