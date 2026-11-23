// The Translate dialog disables the main window like a modal dialog, so it
// must be owned by it: an unowned window can fall behind the disabled frame
// and the app looks frozen.
//
// orig only: ng's dialog is a gpui tool window.
// Run: bun tests/translate-dialog-owner.ts [--no-build]

import { join } from "node:path";
import { cmdId, ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { enumWindows, getWindowOwner, getWindowPid, getWindowText, isWindowVisible, sleep } from "./winapi.ts";

function findDialog(pid: number, frame: number): number {
  let found = 0;
  enumWindows((h) => {
    if (h !== frame && getWindowPid(h) === pid && isWindowVisible(h) && getWindowText(h) === "Translate") {
      found = h;
      return false;
    }
    return true;
  });
  return found;
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("SKIP translate-dialog-owner: orig only");
    return;
  }
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle(30000);
    await client.seedTextSelection(1);
    sendCommand(frame, cmdId("CmdTranslateSelection"));
    let dlg = 0;
    const deadline = Date.now() + 8000;
    while (!dlg && Date.now() < deadline) {
      await sleep(100);
      dlg = findDialog(proc.pid!, frame);
    }
    if (!dlg) {
      throw new Error("translate-dialog-owner: the Translate dialog didn't open");
    }
    const owner = getWindowOwner(dlg);
    if (owner !== frame) {
      throw new Error(`translate-dialog-owner: dialog owner is ${owner}, want the main window ${frame}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("translate-dialog-owner: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
