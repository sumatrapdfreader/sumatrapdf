// Opening a document from a slow (sleeping) drive must not freeze the UI: the
// engine is created on a thread while a "Loading" tab is shown, then the
// document goes on screen. The open comes the way a reuse-instance launch
// sends it (kCopyDataOpen); -dbg-control TestLoadDelay simulates the drive.
//
// Run: bun tests/open-slow-drive.ts [--no-build] [-ng]

import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sendOpenCopyData, sleep } from "./winapi.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const LOAD_DELAY_MS = 3000;

async function currentTab(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestCurrentTab);
  return String(res[1] ?? "");
}

// a Ping round-trip: how long the UI thread takes to answer
async function pingMs(client: ControlClient): Promise<number> {
  const t0 = Date.now();
  await client.request(ControlCommand.Ping);
  return Date.now() - t0;
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP open-slow-drive: sends a Windows WM_COPYDATA open");
    return;
  }
  const { proc, client, frame } = await launchControlled([]);
  try {
    await client.request(ControlCommand.Ping);
    await client.request(ControlCommand.TestLoadDelay, [LOAD_DELAY_MS]);
    const t0 = Date.now();
    if (!sendOpenCopyData(frame, PDF)) {
      throw new Error("open-slow-drive: the open request wasn't handled");
    }

    // while the drive spins up the UI answers and shows the loading tab
    let sawLoading = false;
    let worstPing = 0;
    while (Date.now() - t0 < LOAD_DELAY_MS / 2) {
      worstPing = Math.max(worstPing, await pingMs(client));
      const tab = await currentTab(client);
      if (tab.includes("zlib.3.pdf") && tab.includes("page=0")) {
        sawLoading = true;
      }
      await sleep(50);
    }
    if (worstPing >= LOAD_DELAY_MS / 2) {
      throw new Error(`open-slow-drive: the UI didn't answer for ${worstPing} ms while loading`);
    }
    if (!sawLoading) {
      throw new Error(`open-slow-drive: no loading tab (current tab: ${await currentTab(client)})`);
    }

    await client.request(ControlCommand.TestLoadDelay, [0]);
    const deadline = Date.now() + LOAD_DELAY_MS + 10000;
    let tab = "";
    while (Date.now() < deadline) {
      tab = await currentTab(client);
      if (tab.includes("zlib.3.pdf") && tab.includes("page=1")) {
        console.log(`open-slow-drive: OK, loaded after ${Date.now() - t0} ms, worst ping ${worstPing} ms`);
        return;
      }
      await sleep(100);
    }
    throw new Error(`open-slow-drive: the document didn't finish loading (current tab: ${tab})`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
