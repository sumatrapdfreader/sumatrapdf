// CmdCloseWindow closes the window with all its tabs, like its close button;
// the other windows stay. On macOS it is ⇧⌘W.
//
// Run: bun tests/close-window.ts [--no-build]

import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { macControlRequest } from "./mac-control.ts";
import { cmdId, ROOT, runStandalone, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const name = "close-window";
// TestInput's modifier bits
const kInputShift = 2;
const kInputCmd = 16;
const VK_W = 0x57;

async function windowCount(client: ControlClient): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
  return Number(/ windows=(\d+)/.exec(raw)?.[1] ?? -1);
}

async function waitWindows(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 8000;
  let n = -1;
  while (Date.now() < deadline) {
    n = await windowCount(client);
    if (n === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`${name}: ${what}: ${n} windows, want ${want}`);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdNewWindow"));
    await waitWindows(client, 2, "CmdNewWindow");
    sendCommand(frame, cmdId("CmdCloseWindow"));
    await waitWindows(client, 1, "CmdCloseWindow");
    if (IS_MAC) {
      sendCommand(frame, cmdId("CmdNewWindow"));
      await waitWindows(client, 2, "CmdNewWindow again");
      macControlRequest(ControlCommand.TestInput, ["key", VK_W, kInputShift | kInputCmd, 0, 0]);
      await waitWindows(client, 1, "⇧⌘W");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
