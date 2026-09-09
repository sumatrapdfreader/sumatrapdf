// Crash 2026-09-09-13-17-f889: the file was auto-reloaded while the canvas
// context menu was open. TrackPopupMenu runs a nested message loop, so the
// reload deleted the DisplayModel/engine that OnWindowContextMenu had cached
// before the menu opened; picking "Selected Image / Copy To Clipboard" then
// used the freed DisplayModel. The auto-reload now waits for the menu to close.
import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { runStandalone, tmpPath } from "./util";
import { clientToScreen, getClientRect, postMessage, sleep, VK_ESCAPE, WM_CHAR, WM_KEYDOWN } from "./winapi";
import { findCanvas, killAndWait, launchControlled, openContextMenu, waitForContextMenu } from "./win-automation";

// auto-reload waits for two 500ms ticks that see an unchanged file
const kReloadWaitMs = 8000;

function reloaded(logPath: string): boolean {
  try {
    return readFileSync(logPath, "utf8").includes("ReloadDocument:");
  } catch {
    return false;
  }
}

async function waitForReload(logPath: string, timeoutMs: number): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (reloaded(logPath)) {
      return true;
    }
    await sleep(200);
  }
  return false;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ctx-menu-reload");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nReloadModifiedDocuments = true\n",
  );

  const img = join(dir, "img.png");
  const logPath = join(dir, "log.txt");
  copyFileSync("tests/issue-1201-data/001.png", img);

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    dir,
    "-log-to-file",
    logPath,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    img,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    const cr = getClientRect(canvas);
    const pt = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
    openContextMenu(canvas, pt.x, pt.y);
    const popup = await waitForContextMenu(3000);
    if (!popup) {
      throw new Error("ctx-menu-reload: context menu did not open");
    }

    // replace the document under the open menu. The file watcher must not
    // reload it while the menu's nested message loop is running.
    copyFileSync("tests/issue-1201-data/002.png", img);
    await sleep(4000);
    if (reloaded(logPath)) {
      postMessage(popup, WM_KEYDOWN, VK_ESCAPE, 0);
      throw new Error("ctx-menu-reload: document reloaded while the context menu was open");
    }

    // "Selected &Image" then "C&opy To Clipboard"
    postMessage(popup, WM_CHAR, "i".charCodeAt(0), 0);
    await sleep(600);
    postMessage(popup, WM_CHAR, "o".charCodeAt(0), 0);

    // the deferred reload still has to happen once the menu is gone
    if (!(await waitForReload(logPath, kReloadWaitMs))) {
      throw new Error("ctx-menu-reload: document was never reloaded after the menu closed");
    }

    try {
      await client.waitForRenderIdle(5000);
    } catch (e) {
      throw new Error(`ctx-menu-reload: SumatraPDF crashed after copying an image from a reloaded document: ${e}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
