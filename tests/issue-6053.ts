// Issue #6053: Stop Reading must be in the Read Aloud menu even when the
// playback bar is not on screen. It is disabled when nothing is being read.

import { join } from "node:path";
import { cmdId, ROOT, runStandalone } from "./util.ts";
import {
  findCanvas,
  findSubMenu,
  killAndWait,
  launchControlled,
  readContextMenuTree,
  sendCommand,
} from "./win-automation.ts";
import { setForegroundWindow } from "./winapi.ts";

const PDF = join(ROOT, "tests", "issue-1189.pdf");

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    setForegroundWindow(frame);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6053: no canvas");
    }

    const tree = await readContextMenuTree(canvas);
    if (tree.length === 0) {
      throw new Error("issue-6053: no context menu");
    }
    const readAloud = findSubMenu(tree, "Read Aloud");
    if (!readAloud || !readAloud.items) {
      throw new Error(`issue-6053: no Read Aloud submenu: ${JSON.stringify(tree.map((it) => it.text))}`);
    }
    const stop = readAloud.items.find((it) => it.text === "Stop Reading");
    if (!stop) {
      throw new Error(`issue-6053: Stop Reading missing from Read Aloud menu: ${JSON.stringify(readAloud.items)}`);
    }
    if (!stop.disabled) {
      throw new Error(
        `issue-6053: Stop Reading should be disabled when not reading: ${JSON.stringify(readAloud.items)}`,
      );
    }

    // Stop with no session must not crash (the playback bar / tab may already be gone)
    sendCommand(frame, cmdId("CmdStopReadAloud"));
    console.log(`issue-6053: Stop Reading is in the menu and disabled when idle`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
