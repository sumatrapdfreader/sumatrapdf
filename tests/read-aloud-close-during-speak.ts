// A speech backend call pumps messages (COM's modal loop), so the user can
// close the window while TtsSpeakUtf8 runs; read aloud then went on with the
// freed tab (crashes 2026-09-21-11-44-0b14 and -9cbf). -for-testing makes the
// next speak pump the queue, where a WM_CLOSE waits, and then fail.
//
// Run: bun tests/read-aloud-close-during-speak.ts [--no-build]

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, DEBUG_REPORT_EXIT_CODE } from "./control.ts";
import { cmdId, ROOT, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { postMessage, WM_CLOSE } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation.ts";

const MARKER = "tts: SpeakChunk: tab closed during speak";

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const log = tmpPath("read-aloud-close-during-speak.log");
  const { proc, client, frame } = await launchControlled(["-log-to-file", log, pdf]);
  let exited = false;
  try {
    await client.waitForRenderIdle();
    await client.request(ControlCommand.TestTtsPumpOnSpeak, []);
    // queued in this order, so the close is dispatched from inside the speak call
    sendCommand(frame, cmdId("CmdReadAloudFromTopPage"));
    postMessage(frame, WM_CLOSE, 0, 0);
    exited = await waitForExit(proc, 20000 * SLOW_BUILD_FACTOR);
  } finally {
    client.close();
    if (!exited) {
      await killAndWait(proc);
    }
  }
  if (!exited) {
    throw new Error("read-aloud-close-during-speak: app did not exit after WM_CLOSE");
  }
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    const what = exitCode === DEBUG_REPORT_EXIT_CODE ? "debug report (ReportIf) fired" : `exit code ${exitCode}`;
    throw new Error(`read-aloud-close-during-speak: ${what}`);
  }
  // a whole line: the log also records this process's command line
  const logLines = readFileSync(log, "utf8").split(/\r?\n/);
  if (!logLines.includes(MARKER)) {
    throw new Error(`read-aloud-close-during-speak: log lacks "${MARKER}"`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
