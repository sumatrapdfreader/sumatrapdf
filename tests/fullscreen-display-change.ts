// A fullscreen or presentation window keeps covering its monitor when the
// display changes (resolution, rotation, a taskbar moved): orig handles
// WM_DISPLAYCHANGE and WM_SETTINGCHANGE by re-fitting the window. The test
// shrinks the window, as a resolution change would leave it, then sends the
// message Windows sends.
//
// Run: bun tests/fullscreen-display-change.ts [--no-build] [-ng]

import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { getWindowRect, monitorRectOfWindow, type Rect, sendMessage, setWindowPos, sleep } from "./winapi.ts";

const name = "fullscreen-display-change";
const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const WM_DISPLAYCHANGE = 0x007e;
const WM_SETTINGCHANGE = 0x001a;
const SPI_SETWORKAREA = 0x002f;

function show(r: Rect): string {
  return `${r.left},${r.top},${r.right},${r.bottom}`;
}

async function waitCovers(frame: number, what: string): Promise<void> {
  const deadline = Date.now() + 3000;
  for (;;) {
    const r = getWindowRect(frame);
    const m = monitorRectOfWindow(frame);
    if (show(r) === show(m)) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`${name}: ${what}: window ${show(r)}, monitor ${show(m)}`);
    }
    await sleep(50);
  }
}

async function check(client: ControlClient, frame: number, cmd: string, msg: number, wp: number): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
  await waitCovers(frame, `${cmd} didn't cover the monitor`);

  const m = monitorRectOfWindow(frame);
  setWindowPos(frame, m.left + 100, m.top + 100, (m.right - m.left) / 2, (m.bottom - m.top) / 2);
  await sleep(100);
  sendMessage(frame, msg, wp, 0);
  await waitCovers(frame, `${cmd}: message 0x${msg.toString(16)} didn't re-fit the window`);

  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
  await sleep(300);
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log(`SKIP ${name}: Windows display messages`);
    return;
  }
  const { proc, client, frame } = await launchControlled([PDF]);
  try {
    await client.waitForRenderIdle();
    await check(client, frame, "CmdToggleFullscreen", WM_DISPLAYCHANGE, 32);
    await check(client, frame, "CmdTogglePresentationMode", WM_DISPLAYCHANGE, 32);
    await check(client, frame, "CmdToggleFullscreen", WM_SETTINGCHANGE, SPI_SETWORKAREA);
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
