// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6482
//
// Automatically Scroll couldn't go slower than 8 px/s: a lower
// ReadingAutoScrollSpeed was clamped to 8 and the "slower" arrow stopped
// there. It now goes down to 1 px/s.
//
// Run: bun tests/issue-6482.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { ROOT, runStandalone, tmpPath } from "./util.ts";
import { postMessage, sleep, VK_UP, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const kSlowest = 1;

async function state(client: ControlClient): Promise<{ speed: number; scrollY: number }> {
  const res = await client.request(ControlCommand.TestReadingAutoScroll, []);
  const m = /OK visible=1 .*speed=(\d+) scrollY=(-?\d+)/.exec(String(res[1] ?? ""));
  return m ? { speed: +m[1]!, scrollY: +m[2]! } : { speed: -1, scrollY: -1 };
}

async function speed(client: ControlClient): Promise<number> {
  return (await state(client)).speed;
}

async function waitSpeed(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + 4000;
  let got = await speed(client);
  while (got !== want && Date.now() < deadline) {
    await sleep(50);
    got = await speed(client);
  }
  if (got !== want) {
    throw new Error(`#6482: ${what}: speed is ${got} px/s, expected ${want}`);
  }
}

// scrolling down, Up is the "slower" arrow
function pressSlower(frame: number): void {
  postMessage(frame, WM_KEYDOWN, VK_UP, 1);
  postMessage(frame, WM_KEYUP, VK_UP, 1);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6482-appdata");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "ReadingAutoScrollSpeed = 2\nCheckForUpdates = false\nRestoreSession = false\n",
  );
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleAutomaticallyScroll"]);
    await waitSpeed(client, 2, "the speed from the settings should be kept");

    pressSlower(frame);
    await waitSpeed(client, kSlowest, "the slower arrow should step down to 1 px/s");
    pressSlower(frame);
    await sleep(200);
    await waitSpeed(client, kSlowest, "1 px/s should be the slowest");

    // 1 px/s still moves: a pixel about every second
    const startY = (await state(client)).scrollY;
    const deadline = Date.now() + 4000;
    while ((await state(client)).scrollY <= startY) {
      if (Date.now() > deadline) {
        throw new Error(`#6482: at 1 px/s the page didn't move from scrollY=${startY}`);
      }
      await sleep(100);
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("issue-6482: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
