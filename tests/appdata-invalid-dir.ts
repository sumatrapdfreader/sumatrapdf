// -appdata with a path that can't be created (a stray quote pasted before a
// drive letter: `-appdata “D:\dir`) hit ReportIf in SetAppDataDir and sent a
// debug report (crash 2026-10-28-02-43-1a4b). It must fall back to the
// default dir and keep running.
//
// Run: bun tests/appdata-invalid-dir.ts [--no-build]

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { IS_WIN } from "./host.ts";
import { makeBlankPdf, pollUntil, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchSumatra } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

// -for-testing turns a ReportIf into this exit code
const kReportIfExitCode = 105;

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("  skipped: original Windows app only");
    return;
  }

  const pdf = tmpPath("appdata-invalid-dir.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  const logPath = tmpPath("appdata-invalid-dir-log.txt");
  writeFileSync(logPath, "");
  const readLog = () => (existsSync(logPath) ? readFileSync(logPath, "utf8") : "");

  // ':' after the first path component makes CreateDirectory fail with error 123
  const badDir = tmpPath("appdata-invalid-dir") + "\\\u201cD:\\SumatraPDF";
  const proc = launchSumatra(["-appdata", badDir, "-log-to-file", logPath, pdf]);
  try {
    await pollUntil(readLog, (s) => s.includes("SetAppDataDir") || s.includes("LoadDocument"), {
      error: "appdata-invalid-dir: app logged nothing about the app data dir",
    });
    await Promise.race([proc.exited, sleep(2000)]);
    if (proc.exitCode === kReportIfExitCode) {
      throw new Error("appdata-invalid-dir: an invalid -appdata sent a debug report");
    }
    if (proc.exitCode !== null) {
      throw new Error(`appdata-invalid-dir: exited with code ${proc.exitCode}`);
    }
    console.log("appdata-invalid-dir: OK");
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
