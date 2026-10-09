// Crash 2026-09-09-13-17-f889: the file was auto-reloaded while the canvas
// context menu was open. TrackPopupMenu runs a nested message loop, so the
// reload deleted the DisplayModel/engine that OnWindowContextMenu had cached
// before the menu opened; picking "Selected Image / Copy To Clipboard" then
// used the freed DisplayModel. The auto-reload now waits for the menu to close.
import { copyFileSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { cmdId, runStandalone, tmpPath, USE_NG } from "./util";
import { clientToScreen, getClientRect, postMessage, sleep, VK_ESCAPE, WM_CHAR, WM_KEYDOWN } from "./winapi";
import { findCanvas, killAndWait, launchControlled, openContextMenu, waitForContextMenu } from "./win-automation";

// auto-reload waits for two 500ms ticks that see an unchanged file
const kReloadWaitMs = 8000;
const kReloadDeferralProbeMs = 1500;

function reloaded(logPath: string): boolean {
  try {
    return readFileSync(logPath, "utf8").includes("ReloadDocument:");
  } catch {
    return false;
  }
}

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

async function openImageMenu(client: ControlClient, frame: number): Promise<number> {
  if (!USE_NG) {
    const canvas = findCanvas(frame);
    const cr = getClientRect(canvas);
    const pt = clientToScreen(canvas, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
    openContextMenu(canvas, pt.x, pt.y);
    const popup = await waitForContextMenu(3000);
    if (!popup) {
      throw new Error("ctx-menu-reload: context menu did not open");
    }
    return popup;
  }
  const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
  if (!m) {
    throw new Error("ctx-menu-reload: no canvas rect");
  }
  const x = +m[1]! + Math.floor(+m[3]! / 2);
  const y = +m[2]! + Math.floor(+m[4]! / 2);
  await client.request(ControlCommand.TestInput, ["click", x, y, 1, 0]);
  const deadline = Date.now() + 3000;
  while (Date.now() < deadline) {
    if (/ popup=1/.test(await uiState(client))) {
      return 0;
    }
    await sleep(40);
  }
  throw new Error("ctx-menu-reload: context menu did not open");
}

async function dismissMenu(client: ControlClient, popup: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestInput, ["key", VK_ESCAPE, 0, 0, 0]);
    return;
  }
  postMessage(popup, WM_KEYDOWN, VK_ESCAPE, 0);
}

async function menuChar(client: ControlClient, popup: number, ch: string): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestInput, ["key", ch.toUpperCase().charCodeAt(0), 0, 0, 0]);
    return;
  }
  postMessage(popup, WM_CHAR, ch.charCodeAt(0), 0);
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
    const popup = await openImageMenu(client, frame);

    // replace the document under the open menu. The file watcher must not
    // reload it while the menu is up.
    copyFileSync("tests/issue-1201-data/002.png", img);
    await sleep(kReloadDeferralProbeMs);
    if (reloaded(logPath)) {
      await dismissMenu(client, popup);
      throw new Error("ctx-menu-reload: document reloaded while the context menu was open");
    }

    // "Selected &Image" then "C&opy To Clipboard". ng has no menu window to
    // post the mnemonic to; the command is the one that menu item runs.
    if (USE_NG) {
      await client.request(ControlCommand.TestUiState, ["ctxcmd", cmdId("CmdCopyImage")]);
      await dismissMenu(client, popup);
    } else {
      await menuChar(client, popup, "i");
      await sleep(100);
      await menuChar(client, popup, "o");
    }

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
