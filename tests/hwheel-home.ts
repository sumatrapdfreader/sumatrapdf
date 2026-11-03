// A horizontal mouse wheel on the home page: the frame forwards it to the
// canvas, whose DefWindowProc passed it back to the frame, recursing until the
// stack overflowed.
//
// Run: bun tests/hwheel-home.ts [--no-build]

import { postMessage, sleep } from "./winapi.ts";
import { runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const WM_MOUSEHWHEEL = 0x020e;
const WHEEL_DELTA = 120;

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled([]);
  try {
    await client.homeSelection();
    postMessage(frame, WM_MOUSEHWHEEL, (WHEEL_DELTA << 16) >>> 0, 0);
    await sleep(500);
    if (proc.exitCode !== null) {
      throw new Error(`hwheel-home: app exited with ${proc.exitCode}`);
    }
    // still alive: it answers another request
    await client.homeSelection();
    console.log("hwheel-home: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
