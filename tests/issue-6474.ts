// #6474: with files of the history on a network share that can't be reached,
// startup froze for ~20 s. Regenerating missing Home thumbnails checked every
// history file on the UI thread before the message loop ran, and a check of a
// path on a server that doesn't answer waits out a network timeout.
//
// The history points at TEST-NET-1 (192.0.2.0/24, never routed). Windows remembers
// a server that didn't answer for a while, so each run picks another address.
// The check is skipped for -for-testing, so this launch doesn't pass it.
//
// Run: bun tests/issue-6474.ts [--no-build]

import { IS_MAC } from "./host.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, writeAppdata } from "./util.ts";
import { FRAME_CLASS, killAndWait, windowPosArgs } from "./win-automation.ts";
import { findTopWindow, sleep, windowResponds } from "./winapi.ts";

const N_FILES = 5;
const FRAME_TIMEOUT_MS = 15_000;
// normal startup answers in about a second; the network timeout is ~21 s
const RESPONSIVE_MS = 5_000 * SLOW_BUILD_FACTOR;
const PROBE_MS = 100;

function unreachableHost(): string {
  return `192.0.2.${1 + Math.floor(Math.random() * 254)}`;
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP issue-6474: probes the window with SendMessageTimeout");
    return;
  }
  const host = unreachableHost();
  let files = "FileStates [\n";
  for (let i = 0; i < N_FILES; i++) {
    files += `\t[\n\t\tFilePath = \\\\${host}\\share\\doc${i}.pdf\n\t\tOpenCount = ${N_FILES - i}\n\t]\n`;
  }
  files += "]\n";
  const appdata = writeAppdata(
    "issue-6474-appdata",
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nReuseInstance = false\n" + files,
  );

  const start = Date.now();
  const proc = Bun.spawn([EXE, ...windowPosArgs(), "-appdata", appdata], { stdout: "ignore", stderr: "ignore" });
  try {
    let frame = 0;
    while (!frame && Date.now() - start < FRAME_TIMEOUT_MS) {
      frame = findTopWindow(proc.pid!, FRAME_CLASS);
      await sleep(20);
    }
    if (!frame) {
      throw new Error("issue-6474: no window");
    }
    while (!windowResponds(frame, PROBE_MS)) {
      if (Date.now() - start > RESPONSIVE_MS) {
        throw new Error(`issue-6474: not responding ${RESPONSIVE_MS} ms after launch, history on \\\\${host}`);
      }
    }
    console.log(`issue-6474: responding after ${Date.now() - start} ms`);
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
