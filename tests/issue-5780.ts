// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5780
//
// Ctrl+Shift+Right / Ctrl+Shift+Left (CmdOpenNextFileInFolder /
// CmdOpenPrevFileInFolder) stopped working: DirIter built its FindFirstFileW
// pattern with a plain string join ("C:\dir*" instead of "C:\dir\*"), so it
// enumerated zero files and folder navigation silently did nothing.
//
// This puts 3 copies of a PDF in a temp folder, opens the first one and drives
// the next/prev commands via WM_COMMAND, asserting the window title follows
// aaa -> bbb -> ccc -> bbb.
//
// Run:  bun tests/issue-5780.ts [--no-build]   (or via tests/all.ts)

import { mkdirSync, copyFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { cmdId } from "./util.ts";
import { launchSumatra, waitForFrame, sendCommand } from "./win-automation.ts";
import { getWindowText, sleep } from "./winapi.ts";

// look these up by name: command ids are auto-numbered and shift whenever
// commands are added/removed, so hardcoding them silently sends the wrong command
const CmdOpenNextFileInFolder = cmdId("CmdOpenNextFileInFolder");
const CmdOpenPrevFileInFolder = cmdId("CmdOpenPrevFileInFolder");

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

async function waitForTitle(frame: number, substr: string, timeoutMs = 5000): Promise<string> {
  const deadline = Date.now() + timeoutMs;
  let title = "";
  while (Date.now() < deadline) {
    title = getWindowText(frame);
    if (title.includes(substr)) {
      return title;
    }
    await sleep(200);
  }
  throw new Error(`window title is "${title}", expected it to contain "${substr}"`);
}

export async function testit(): Promise<void> {
  const dir = join(process.env.TEMP!, "sumatra-issue-5780");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  copyFileSync(SRC_PDF, join(dir, "aaa.pdf"));
  copyFileSync(SRC_PDF, join(dir, "bbb.pdf"));
  copyFileSync(SRC_PDF, join(dir, "ccc.pdf"));

  const proc = launchSumatra([join(dir, "aaa.pdf")]);
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error("SumatraPDF frame window not found");
    }
    await waitForTitle(frame, "aaa.pdf");

    sendCommand(frame, CmdOpenNextFileInFolder);
    await waitForTitle(frame, "bbb.pdf");

    sendCommand(frame, CmdOpenNextFileInFolder);
    await waitForTitle(frame, "ccc.pdf");

    sendCommand(frame, CmdOpenPrevFileInFolder);
    await waitForTitle(frame, "bbb.pdf");
  } finally {
    proc.kill();
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit, "issue-5780");
}
