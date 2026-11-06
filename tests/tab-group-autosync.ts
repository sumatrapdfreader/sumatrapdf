// A tab group saved with "Keep the group in sync with this window's tabs"
// (TabGroup.AutoSync) follows the window it is restored into: opening a
// document there adds it to the group. A group without AutoSync stays the
// snapshot it was saved as.
//
// Run: bun tests/tab-group-autosync.ts [--no-build]

import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumWindows,
  getWindowPid,
  getWindowText,
  postMessage,
  sendCopyDataW,
  sleep,
  VK_DOWN,
  VK_RETURN,
  WM_KEYDOWN,
  WM_KEYUP,
} from "./winapi.ts";

const kSettings = "SumatraPDF-settings.txt";
// WM_COPYDATA id for DDE-style commands
const kCopyDataDde = 0x44646557;

function groupsSettings(paths: string[]): string {
  const files = (ps: string[]) => ps.map((p) => `\t\t\t[\n\t\t\t\tPath = ${p}\n\t\t\t]`).join("\n");
  return (
    `RestoreSession = false\nCheckForUpdates = false\nTabGroups [\n` +
    `\t[\n\t\tName = live\n\t\tTabFiles [\n${files(paths.slice(0, 2))}\n\t\t]\n\t\tAutoSync = true\n\t]\n` +
    `\t[\n\t\tName = snapshot\n\t\tTabFiles [\n${files(paths.slice(0, 1))}\n\t\t]\n\t]\n` +
    `]\n`
  );
}

// the documents of the group named name, from the saved settings
function groupFiles(settings: string, name: string): string[] {
  const start = settings.indexOf(`Name = ${name}`);
  if (start < 0) {
    return [];
  }
  const rest = settings.slice(start);
  const end = rest.indexOf("\n\t]");
  const block = end < 0 ? rest : rest.slice(0, end);
  return [...block.matchAll(/Path = (.*)/g)].map((m) => m[1]!.trim());
}

function findTopWindow(pid: number, title: string): number {
  let found = 0;
  enumWindows((h) => {
    if (getWindowPid(h) === pid && getWindowText(h) === title) {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

async function waitFor<T>(what: string, get: () => T | null | 0, ms = 8000): Promise<T> {
  const deadline = Date.now() + ms;
  while (Date.now() < deadline) {
    const v = get();
    if (v) {
      return v;
    }
    await sleep(100);
  }
  throw new Error(`tab-group-autosync: ${what}`);
}

function postKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 1);
  postMessage(hwnd, WM_KEYUP, vk, 1);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("tab-group-autosync");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const paths = ["doc-a.pdf", "doc-b.pdf", "doc-c.pdf"].map((n) => join(dir, n));
  for (const p of paths) {
    copyFileSync(src, p);
  }
  writeFileSync(join(dir, kSettings), groupsSettings(paths));

  const { proc, client, frame } = await launchControlled(["-appdata", dir], { saveSettings: true });
  try {
    // the Home page: nothing to wait for rendering
    await sleep(800);
    const pid = proc.pid!;

    // restore "live", the first group
    sendCommand(frame, cmdId("CmdTabGroupRestore"));
    const dlg = await waitFor("no Restore Tab Group window", () => findTopWindow(pid, "Restore Tab Group"));
    postKey(dlg, VK_DOWN);
    await sleep(200);
    postKey(dlg, VK_RETURN);
    await waitFor(`doc-b did not open: "${getWindowText(frame)}"`, () => getWindowText(frame).includes("doc-b"));

    // a document opened in that window joins the group
    sendCopyDataW(frame, kCopyDataDde, `[Open("${paths[2]}", 0, 1, 0)]`);
    await waitFor(`doc-c did not open: "${getWindowText(frame)}"`, () => getWindowText(frame).includes("doc-c"));
    await sleep(300);

    await client.quit();
    await proc.exited;
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const saved = readFileSync(join(dir, kSettings), "utf8");
  const live = groupFiles(saved, "live");
  const snapshot = groupFiles(saved, "snapshot");
  console.log(`  live: ${live.length} documents, snapshot: ${snapshot.length}`);
  if (live.join("|") !== paths.join("|")) {
    throw new Error(`tab-group-autosync: the AutoSync group should have doc-a, doc-b, doc-c: ${JSON.stringify(live)}`);
  }
  if (snapshot.join("|") !== paths[0]) {
    throw new Error(`tab-group-autosync: the snapshot group should be unchanged: ${JSON.stringify(snapshot)}`);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
