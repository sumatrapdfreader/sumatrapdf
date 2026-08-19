// #5995: the automatic Windows light/dark mode existed as Theme = System in
// advanced settings, but Change Theme only listed concrete color themes.
// Select Follow Windows in the real dialog, verify that it saves System, then
// reopen the dialog and verify that System is selected instead of its resolved
// Light/Dark theme.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util";
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

async function waitForChangeThemeDialog(pid: number, timeoutMs = 5000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    let found = 0;
    enumWindows((hwnd) => {
      if (getWindowPid(hwnd) === pid && getWindowText(hwnd) === "Change Theme") {
        found = hwnd;
      }
      return true;
    });
    if (found) {
      return found;
    }
    await sleep(30);
  }
  throw new Error("issue-5995: Change Theme dialog did not appear");
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
    const dialog = await waitForChangeThemeDialog(proc.pid!);
    await pressKey(dialog, key);
    await pressKey(dialog, VK_RETURN);
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
