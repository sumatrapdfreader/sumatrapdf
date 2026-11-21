// macOS: the Delete key removes the selected home page entry from the file
// history, as Del does on Windows. A Mac's Delete key is Backspace
// (VK_BACK), which the home page didn't handle.
//
// Run: bun tests/home-page-mac-delete.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient, HomeSelection } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { sendMessage, setCursorPos, sleep } from "./winapi.ts";

const name = "home-page-mac-delete";
const WM_KEYDOWN = 0x0100;
const VK_BACK = 0x08;
const nFiles = 2;

function makeAppDir(): string {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "sub", `doc-${i}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\nFileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

async function waitForHome(client: ControlClient, pred: (h: HomeSelection) => boolean, what: string) {
  const deadline = Date.now() + 8000;
  for (;;) {
    const h = await client.homeSelection();
    if (h.ready && pred(h)) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`${name}: ${what} (last: ${h.raw})`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG || !IS_MAC) {
    console.log(`SKIP ${name}: ng on macOS only`);
    return;
  }
  // a cursor over the thumbnails would move the selection through hover
  setCursorPos(0, 0);
  const { proc, client, frame } = await launchControlled(["-appdata", makeAppDir()]);
  try {
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error(`${name}: home-page canvas not found`);
    }
    await waitForHome(client, (h) => h.entries === nFiles && h.sel === 0, "home page never showed the files");
    sendMessage(canvas, WM_KEYDOWN, VK_BACK, 0);
    await waitForHome(client, (h) => h.entries === nFiles - 1, "Delete (Backspace) didn't remove the entry");
    console.log(`${name}: OK`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
