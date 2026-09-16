// Profile session restore with the -profile build: seed 3 files, restart,
// wait until restore finishes, quit, summarize sumperf.txt.
//
// bun tests/ad-hoc-session-restore-perf.ts

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, uniquePipeName } from "./control.ts";
import { ROOT, tmpPath } from "./util.ts";
import { killAndWait, waitForExit, waitForFrame, windowPosArgs } from "./win-automation.ts";

const EXE = join(ROOT, "out", "prf64", "SumatraPDF.exe");
const PDFS = [
  join(ROOT, "ext", "a-zlib", "zlib.3.pdf"),
  join(ROOT, "tests", "issue-1189.pdf"),
  join(ROOT, "tests", "issue-5871.pdf"),
];

type FnStat = { name: string; calls: number; us: number };

function parseHexUs(s: string): number | null {
  if (!/^[0-9a-f]+$/i.test(s)) {
    return null;
  }
  return parseInt(s, 16);
}

function summarizePerfLog(text: string): { topInclusive: FnStat[]; topCalls: FnStat[]; interesting: FnStat[] } {
  const byName = new Map<string, FnStat>();
  const add = (name: string, us: number) => {
    const cur = byName.get(name);
    if (cur) {
      cur.calls++;
      cur.us += us;
      return;
    }
    byName.set(name, { name, calls: 1, us });
  };
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.replace(/\s+$/, "");
    if (!line || line === "perf log start") {
      continue;
    }
    const m = /^( *)(.+?)(?: {2}([0-9a-f]+))?$/.exec(line);
    if (!m) {
      continue;
    }
    const us = m[3] ? parseHexUs(m[3]) : null;
    if (us === null) {
      continue;
    }
    add(m[2].trim(), us);
  }
  const all = [...byName.values()];
  all.sort((a, b) => b.us - a.us);
  const topInclusive = all.slice(0, 40);
  const byCalls = [...all].sort((a, b) => b.calls - a.calls || b.us - a.us);
  const interestingNames = [
    "RelayoutFrame",
    "ScheduleUiUpdate",
    "FrameUpdateUi",
    "LayoutTabs",
    "RebuildTabCtrls",
    "AddTabToWindow",
    "InsertTab",
    "UpdateTabWidth",
    "ShowTabBar",
    "ShowMainWindow",
    "CreateAndShowMainWindow",
    "CreateMainWindow",
    "RestoreTabOnStartup",
    "LoadDocument",
    "LoadDocumentFinish",
    "ReplaceDocumentInCurrentTab",
    "UpdateUiForCurrentTab",
    "SetSidebarVisibility",
    "UpdateCanvasSize",
    "HomePageRelayout",
    "RelayoutCaption",
    "TabsSelect",
    "LoadModelIntoTab",
    "ReloadDocument",
    "UpdateWindow",
    "RedrawWindow",
    "RedrawAll",
    "HwndInvalidate",
    "CreateThumbnailForFile",
    "FinishPendingDocumentRelayout",
    "LayoutAndFocusOnStartup",
    "NotifySessionRestoreFinished",
  ];
  const interesting: FnStat[] = [];
  for (const name of interestingNames) {
    for (const s of all) {
      if (s.name === name || s.name.endsWith("::" + name) || s.name.includes(name)) {
        interesting.push(s);
      }
    }
  }
  interesting.sort((a, b) => b.us - a.us);
  return { topInclusive, topCalls: byCalls.slice(0, 30), interesting };
}

function printStats(title: string, rows: FnStat[]) {
  console.log(`\n${title}`);
  console.log("us".padStart(12), "calls".padStart(8), "name");
  for (const s of rows) {
    console.log(String(s.us).padStart(12), String(s.calls).padStart(8), s.name);
  }
}

async function launchProfile(args: string[]): Promise<{ proc: Bun.Subprocess; client: ControlClient }> {
  const pipe = uniquePipeName("sumatra-prf-restore");
  const proc = Bun.spawn([EXE, ...windowPosArgs(), "-dbg-control", pipe, ...args], {
    stdout: "ignore",
    stderr: "pipe",
  });
  if (proc.stderr) {
    void new Response(proc.stderr).text();
  }
  const client = await ControlClient.connect(pipe, 30000);
  const frame = await waitForFrame(proc.pid!);
  if (!frame) {
    client.close();
    throw new Error("profile SumatraPDF main window did not appear");
  }
  return { proc, client };
}

async function quit(proc: Bun.Subprocess, client: ControlClient) {
  try {
    await client.quit();
  } catch {
    // process may already be exiting
  }
  client.close();
  if (!(await waitForExit(proc, 8000))) {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`missing ${EXE}; build with bun cmd/build.ts -profile`);
  }
  for (const pdf of PDFS) {
    if (!existsSync(pdf)) {
      throw new Error(`missing ${pdf}`);
    }
  }

  const appData = tmpPath("session-restore-perf-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = true",
      "RememberOpenedFiles = true",
      "RememberStatePerDocument = true",
      "LazyLoading = true",
      "UseTabs = true",
      "ReuseInstance = false",
      "ShowStartPage = true",
      "",
    ].join("\n"),
  );

  console.log("seed: open 3 files and quit");
  const seed = await launchProfile(["-appdata", appData, ...PDFS]);
  try {
    await seed.client.waitForSessionRestored(30000);
    await seed.client.waitForRenderIdle(30000);
  } finally {
    await quit(seed.proc, seed.client);
  }

  const outDir = tmpPath("session-restore-perf");
  mkdirSync(outDir, { recursive: true });
  const perfPath = join(outDir, "sumperf-restore.txt");
  rmSync(perfPath, { force: true });

  console.log("profile: restore session");
  const restored = await launchProfile(["-appdata", appData, "-start-perf-log", "-log-perf-file", perfPath]);
  try {
    const info = await restored.client.waitForSessionRestored(30000);
    console.log("WaitSessionRestored:", info);
    await restored.client.request(ControlCommand.StopPerfLog);
  } finally {
    await quit(restored.proc, restored.client);
  }

  if (!existsSync(perfPath)) {
    throw new Error(`perf log not written: ${perfPath}`);
  }
  const text = readFileSync(perfPath, "utf8");
  console.log(`perf log ${perfPath} (${text.length} bytes, ${text.split(/\n/).length} lines)`);
  const { topInclusive, topCalls, interesting } = summarizePerfLog(text);
  printStats("interesting restore/layout/redraw", interesting);
  printStats("top inclusive (hex us decoded)", topInclusive);
  printStats("top call counts", topCalls);
}

if (import.meta.main) {
  try {
    await testit();
  } catch (e) {
    console.error(e);
    process.exit(1);
  }
}
