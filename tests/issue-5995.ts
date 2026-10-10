// #5995: the automatic Windows light/dark mode existed as Theme = System in
// advanced settings, but Change Theme only listed concrete color themes.
// Select Follow Windows in the real dialog, verify that it saves System, then
// reopen the dialog and verify that System is selected instead of its resolved
// Light/Dark theme.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control";
import { IS_MAC } from "./host";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util";
import {
  enumWindows,
  getWindowPid,
  getWindowText,
  postMessage,
  sleep,
  VK_DOWN,
  VK_RETURN,
  VK_UP,
  WM_CLOSE,
} from "./winapi";
import { killAndWait, launchControlled, pressKey, sendCommand, waitForExit } from "./win-automation";

function findChangeThemeDialog(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getWindowText(hwnd) === "Change Theme") {
      found = hwnd;
    }
    return true;
  });
  return found;
}

async function macThemeOpen(client: ControlClient): Promise<boolean> {
  const raw = String((await client.request(ControlCommand.TestToolWindow, ["state", "changetheme"]))[1] ?? "");
  return raw.startsWith("OK");
}

async function waitMacTheme(client: ControlClient, open: boolean): Promise<void> {
  const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
  let openNow = false;
  while (Date.now() < deadline) {
    openNow = await macThemeOpen(client);
    if (openNow === open) {
      return;
    }
    await sleep(30);
  }
  throw new Error(`issue-5995: Change Theme dialog did not ${open ? "appear" : "close"}`);
}

async function macThemeKey(client: ControlClient, vk: number): Promise<void> {
  const res = await client.request(ControlCommand.TestToolWindow, ["input", "changetheme", "key", vk, 0, 0, 0]);
  const raw = String(res[1] ?? "");
  if (raw.startsWith("ERR") || raw === "NOTREADY") {
    throw new Error(`issue-5995: theme key ${vk}: ${raw}`);
  }
}

async function waitForChangeThemeDialog(pid: number, open: boolean, timeoutMs = 5000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const found = findChangeThemeDialog(pid);
    if (open === (found !== 0)) {
      return found;
    }
    await sleep(30);
  }
  throw new Error(`issue-5995: Change Theme dialog did not ${open ? "appear" : "close"}`);
}

function savedTheme(settingsPath: string): string {
  const settings = readFileSync(settingsPath, "utf8");
  const match = /^Theme\s*=\s*(.*)$/m.exec(settings);
  return match?.[1]?.trim() ?? "";
}

async function chooseAdjacentTheme(appDataDir: string, key: number): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", appDataDir], { saveSettings: true });
  try {
    sendCommand(frame, cmdId("CmdChangeTheme"));
    if (IS_MAC) {
      await waitMacTheme(client, true);
      await macThemeKey(client, key);
      await macThemeKey(client, VK_RETURN);
      await waitMacTheme(client, false);
    } else {
      const dialog = await waitForChangeThemeDialog(proc.pid!, true);
      await pressKey(dialog, key, 0);
      await pressKey(dialog, VK_RETURN, 0);
      await waitForChangeThemeDialog(proc.pid!, false);
    }
    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5995: SumatraPDF did not exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const appDataDir = tmpPath("issue-5995-appdata");
  rmSync(appDataDir, { recursive: true, force: true });
  mkdirSync(appDataDir, { recursive: true });
  const settingsPath = join(appDataDir, "SumatraPDF-settings.txt");
  writeFileSync(settingsPath, "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nTheme = Light\n");

  // Follow Windows is immediately above Light in the list.
  await chooseAdjacentTheme(appDataDir, VK_UP);
  const automatic = savedTheme(settingsPath);
  if (automatic !== "System") {
    throw new Error(`issue-5995: Follow Windows saved Theme = ${automatic}, expected System`);
  }

  // System must reopen on Follow Windows. Moving down once should select Light;
  // if the dialog highlighted the resolved concrete theme, it would select Dark.
  await chooseAdjacentTheme(appDataDir, VK_DOWN);
  const concrete = savedTheme(settingsPath);
  if (concrete !== "Light") {
    throw new Error(`issue-5995: reopened System mode moved down to ${concrete}, expected Light`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
