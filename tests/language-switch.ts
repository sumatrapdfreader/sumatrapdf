import { dlopen, FFIType } from "bun:ffi";
import { cmdId, pollUntil, runStandalone, USE_NG, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled, pressKey, sendCommand } from "./win-automation.ts";
import {
  enumWindows,
  findVisibleChildWindow,
  getMenuItemText,
  getWindowOwner,
  isWindowVisible,
  sendText,
  VK_RETURN,
} from "./winapi.ts";

export async function testit(): Promise<void> {
  if (process.platform !== "win32" || USE_NG) return;
  const user32 = dlopen("user32.dll", { GetMenu: { args: [FFIType.u64], returns: FFIType.u64 } });
  const appdata = writeAppdata(
    "language-switch",
    "UiLanguage = cn\nUseTabs = false\nShowMenubar = true\nCheckForUpdates = false\nRestoreSession = false\n",
  );
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-log-to-file", `${appdata}/log.txt`]);
  const firstMenu = () => getMenuItemText(BigInt(user32.symbols.GetMenu(BigInt(frame))), 0);
  try {
    await pollUntil(
      async () => firstMenu(),
      (text) => text.includes("文件"),
      {
        error: (last) => `Chinese menu missing: ${JSON.stringify(last)}, menu=${user32.symbols.GetMenu(BigInt(frame))}`,
      },
    );
    for (const [search, expected] of [
      ["German", "Datei"],
      ["English", "File"],
      ["Chinese Simplified", "文件"],
    ]) {
      sendCommand(frame, cmdId("CmdChangeLanguage"));
      const dialog = await pollUntil(
        async () => {
          let result = 0;
          enumWindows((hwnd) => {
            if (getWindowOwner(hwnd) === frame && isWindowVisible(hwnd) && findVisibleChildWindow(hwnd, "Edit"))
              result = hwnd;
            return true;
          });
          return result;
        },
        (hwnd) => hwnd !== 0,
      );
      const edit = findVisibleChildWindow(dialog, "Edit");
      sendText(edit, search!);
      await pressKey(edit, VK_RETURN);
      await pollUntil(
        async () => firstMenu(),
        (text) => text.includes(expected!),
        { error: `Language menu did not refresh to ${expected}` },
      );
    }
  } finally {
    await killAndWait(proc, client);
    user32.close();
  }
}

if (import.meta.main) await runStandalone(testit);
