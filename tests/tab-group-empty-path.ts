// Restoring a tab group whose settings have a TabFile without a Path
// ("[ ]") opens the other documents instead of passing "" to LoadDocument,
// which reported len(filePath) == 0 from FileHistoryDemote.
//
// Run: bun tests/tab-group-empty-path.ts [--no-build]

import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumWindows,
  getWindowPid,
  getWindowText,
  postMessage,
  sleep,
  VK_DOWN,
  VK_RETURN,
  WM_KEYDOWN,
  WM_KEYUP,
} from "./winapi.ts";

const kSettings = "SumatraPDF-settings.txt";

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
  throw new Error(`tab-group-empty-path: ${what}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("tab-group-empty-path");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const doc = join(dir, "doc-a.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), doc);
  const settings =
    `RestoreSession = false\nCheckForUpdates = false\nTabGroups [\n` +
    `\t[\n\t\tName = g\n\t\tTabFiles [\n\t\t\t[\n\t\t\t\tPath = ${doc}\n\t\t\t]\n\t\t\t[\n\t\t\t]\n\t\t]\n\t]\n]\n`;
  writeFileSync(join(dir, kSettings), settings);
  const logPath = join(dir, "log.txt");

  const { proc, client, frame } = await launchControlled(["-appdata", dir, "-log-to-file", logPath], {
    saveSettings: true,
  });
  try {
    await sleep(800);
    sendCommand(frame, cmdId("CmdTabGroupRestore"));
    const dlg = await waitFor("no Restore Tab Group window", () => findTopWindow(proc.pid!, "Restore Tab Group"));
    postMessage(dlg, WM_KEYDOWN, VK_DOWN, 1);
    postMessage(dlg, WM_KEYUP, VK_DOWN, 1);
    await sleep(200);
    postMessage(dlg, WM_KEYDOWN, VK_RETURN, 1);
    postMessage(dlg, WM_KEYUP, VK_RETURN, 1);
    await waitFor(`doc-a did not open: "${getWindowText(frame)}"`, () => getWindowText(frame).includes("doc-a"));
    await sleep(500);
    await client.quit();
    await proc.exited;
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const log = existsSync(logPath) ? readFileSync(logPath, "utf8") : "";
  const report = log.split("\n").find((l) => l.includes("_uploadDebugReport"));
  if (report) {
    throw new Error(`tab-group-empty-path: debug report: ${report.trim()}`);
  }
  rmSync(dir, { recursive: true, force: true });
}

if (import.meta.main) {
  await runStandalone(testit);
}
