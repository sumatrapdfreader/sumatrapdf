// Opening the Custom AI panel with no Model / Models configured selected
// item 0 of an empty model drop-down and fired ReportIf in
// CbSetCurrentSelection (crash 2026-11-20-02-12-cc5a).
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, ROOT, USE_NG, cmdId, pollUntil, runStandalone, writeAppdata } from "./util.ts";
import { FRAME_CLASS, sendCommandSync } from "./win-automation.ts";
import {
  ES_MULTILINE,
  GWL_STYLE,
  enumChildWindows,
  getClassName,
  getWindowLong,
  isWindowVisible,
  waitForTopWindow,
} from "./winapi.ts";

export async function testit(): Promise<void> {
  if (process.platform !== "win32" || USE_NG) return;
  const appdata = writeAppdata("custom-ai-no-model", `CustomAI [\n ApiKeyEnv = SUMATRA_CUSTOM_AI_NO_MODEL_KEY\n]\n`);
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      await client.waitForRenderIdle();
      sendCommandSync(frame, cmdId("CmdAIChatWithCustomAPI"));
      await pollUntil(
        async () => {
          let input = 0;
          enumChildWindows(frame, (hwnd) => {
            if (
              isWindowVisible(hwnd) &&
              /edit/i.test(getClassName(hwnd)) &&
              getWindowLong(hwnd, GWL_STYLE) & ES_MULTILINE
            )
              input = hwnd;
            return true;
          });
          return input;
        },
        (hwnd) => hwnd !== 0,
        { timeoutMs: 10000, error: "Custom AI input did not appear" },
      );
      await client.waitForRenderIdle();
    },
    ["-appdata", appdata, join(ROOT, "ext/a-zlib/zlib.3.pdf")],
  );
}

if (import.meta.main) await runStandalone(testit);
