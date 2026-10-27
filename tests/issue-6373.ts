// Run PDF Script: LF-only text (GitHub's copy button) must show as separate lines.
import { resolve } from "node:path";
import { cmdId, pollUntil, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  getClassName,
  getWindowLong,
  getWindowPid,
  getWindowText,
  isWindowVisible,
  sendMessage,
  sendText,
} from "./winapi.ts";

const PDF = resolve(import.meta.dir, "issue-1189.pdf");
const EM_GETLINECOUNT = 0x00ba;
const GWL_STYLE = -16;
const ES_MULTILINE = 0x4;
const ES_READONLY = 0x800;

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("issue-6373: orig only (ng's script box is a GPUI input)");
    return;
  }
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdRunPDFScript"));

    const pid = proc.pid!;
    const scriptEdit = await pollUntil(
      () => {
        let edit = 0;
        enumWindows((h) => {
          if (getWindowPid(h) !== pid || getWindowText(h) !== "Run PDF Script" || !isWindowVisible(h)) return true;
          enumChildWindows(h, (c) => {
            const style = getWindowLong(c, GWL_STYLE);
            if (getClassName(c) === "Edit" && style & ES_MULTILINE && !(style & ES_READONLY)) edit = c;
            return true;
          });
          return true;
        });
        return edit;
      },
      Boolean,
      { error: "script edit" },
    );

    sendText(scriptEdit, "let a = 1;\nlet b = 2;\nprint(a + b);");
    const lines = Number(sendMessage(scriptEdit, EM_GETLINECOUNT, 0, 0));
    if (lines !== 3) throw new Error(`LF-only script shows as ${lines} line(s), expected 3`);
    console.log("issue-6373: LF-only script shows 3 lines");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
