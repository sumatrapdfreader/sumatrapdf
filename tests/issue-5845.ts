// Test for https://github.com/sumatrapdfreader/sumatrapdf/discussions/5845
//
// CmdDeleteFileAndOpenNext should load the next file before moving the current
// file to the Recycle Bin. When no other file remains, it must keep the current
// file open and leave it on disk.

import { copyFileSync, existsSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { cmdId, tmpPath } from "./util.ts";
import { launchSumatra, sendCommand, waitForFrame, killAndWait } from "./win-automation.ts";
import { getWindowText, sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");

async function waitForState(frame: number, titlePart: string, deletedPath: string, timeoutMs = 5000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let title = "";
  while (Date.now() < deadline) {
    title = getWindowText(frame);
    if (title.includes(titlePart) && !existsSync(deletedPath)) {
      return;
    }
    await sleep(100);
  }
  throw new Error(`title is "${title}" or "${deletedPath}" was not deleted; expected ${titlePart}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5845");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const aaa = join(dir, "aaa.pdf");
  const bbb = join(dir, "bbb.pdf");
  const ccc = join(dir, "ccc.pdf");
  copyFileSync(SRC_PDF, aaa);
  copyFileSync(SRC_PDF, bbb);
  copyFileSync(SRC_PDF, ccc);

  const proc = launchSumatra([aaa]);
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error("SumatraPDF frame window not found");
    }
    const command = cmdId("CmdDeleteFileAndOpenNext");

    sendCommand(frame, command);
    await waitForState(frame, "bbb.pdf", aaa);

    sendCommand(frame, command);
    await waitForState(frame, "ccc.pdf", bbb);

    sendCommand(frame, command);
    // the last file must stay put; watch a short window for a late delete
    const until = Date.now() + 200;
    while (Date.now() < until) {
      if (!getWindowText(frame).includes("ccc.pdf") || !existsSync(ccc)) {
        throw new Error("the last file was deleted even though no next file was available");
      }
      await sleep(30);
    }
  } finally {
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit, "issue-5845");
}
