// ng: closing the AI chat panel left its web view (a native child window)
// painted over the document, because a frame that doesn't build the panel
// never hid it.
import { existsSync } from "node:fs";
import { homedir } from "node:os";
import { join, resolve } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, USE_NG, cmdId, pollUntil, runStandalone, writeAppdata } from "./util.ts";
import { FRAME_CLASS, sendCommandSync } from "./win-automation.ts";
import { enumChildWindows, getClassName, isWindowVisible, sleep, waitForTopWindow } from "./winapi.ts";

const PDF = resolve("ext/a-zlib/zlib.3.pdf");

// the panel needs an installed provider; Custom AI's never shows a web view here
function claudeInstalled(): boolean {
  const home = homedir();
  const paths = [
    ".local/bin/claude.exe",
    "AppData/Local/Programs/claude-code/claude.exe",
    "AppData/Roaming/npm/claude.cmd",
  ];
  return paths.some((p) => existsSync(join(home, p))) || Bun.which("claude") !== null;
}

function visibleWebViews(frame: number): number {
  let n = 0;
  enumChildWindows(frame, (hwnd) => {
    if (isWindowVisible(hwnd) && /^(WRY_WEBVIEW|Chrome_WidgetWin)/.test(getClassName(hwnd))) n++;
    return true;
  });
  return n;
}

export async function testit(): Promise<void> {
  if (process.platform !== "win32" || !USE_NG) return;
  if (!claudeInstalled()) {
    console.log("Skipping aichat-close-hides-webview: Claude Code not installed (https://claude.com/claude-code)");
    return;
  }
  const appdata = writeAppdata("aichat-close-hides-webview", "");
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      await client.waitForRenderIdle();
      const cmd = cmdId("CmdAIChatWithClaudeCode");
      sendCommandSync(frame, cmd);
      await pollUntil(
        async () => visibleWebViews(frame),
        (n) => n > 0,
        { timeoutMs: 15000, error: "AI chat web view did not appear" },
      );
      await sleep(1500);
      sendCommandSync(frame, cmd);
      // polls for the web view to go; on failure it stays up indefinitely
      for (let i = 0; i < 20 && visibleWebViews(frame) > 0; i++) await sleep(100);
      if (visibleWebViews(frame) > 0) {
        throw new Error("AI chat web view still visible after closing the panel");
      }
      await client.quit();
    },
    ["-appdata", appdata, PDF],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
