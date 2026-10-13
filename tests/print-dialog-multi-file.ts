// -print-dialog with several files asks for one dialog per file. The Windows 11
// dialog is asynchronous, so all of them were requested at once on one window:
// the losers fell back to classic dialogs nested in each other and crashed.
// Several files must get the modal classic dialog, one after another.
//
// Run: bun tests/print-dialog-multi-file.ts [--no-build]

import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_WIN } from "./host.ts";
import { makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchSumatra } from "./win-automation.ts";

const LOG_PRINT_START = "PrintCurrentFile: start";
const LOG_CLASSIC_DIALOG = "PrintCurrentFile: PrintDlgEx start";
const LOG_WIN11_DIALOG = "Win11 print: start";

function countLines(log: string, needle: string): number {
  return log.split("\n").filter((line) => line.includes(needle)).length;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("  skipped: original Windows print dialog only");
    return;
  }

  const dir = tmpPath("print-dialog-multi-file");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const files = ["a.pdf", "b.pdf", "c.pdf"].map((name) => join(dir, name));
  for (const file of files) {
    writeFileSync(file, makeBlankPdf(), "latin1");
  }
  const logPath = join(dir, "log.txt");
  const readLog = () => (existsSync(logPath) ? readFileSync(logPath, "utf8") : "");

  const proc = launchSumatra(["-print-dialog", ...files, "-log-to-file", logPath]);
  try {
    // the first dialog, of either kind, is up
    const log = await pollUntil(readLog, (s) => s.includes(LOG_CLASSIC_DIALOG) || s.includes(LOG_WIN11_DIALOG), {
      error: "print-dialog-multi-file: no print dialog was started",
    });
    if (log.includes(LOG_WIN11_DIALOG)) {
      throw new Error("print-dialog-multi-file: used the asynchronous Windows 11 dialog");
    }

    // modal: the other files wait for this dialog to close
    const nStarted = countLines(readLog(), LOG_PRINT_START);
    if (nStarted !== 1) {
      throw new Error(`print-dialog-multi-file: ${nStarted} print dialogs started at once`);
    }
    console.log("print-dialog-multi-file: OK");
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
