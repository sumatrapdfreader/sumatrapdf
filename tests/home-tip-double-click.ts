// Double-clicking the tip band on the home page shows another random tip. A
// single click doesn't, or a double-click would change it twice.
//
// Run: bun tests/home-tip-double-click.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient, HomeSelection } from "./control.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { captureWindowDCRegionPixels, postMessage, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const WM_LBUTTONDOWN = 0x201;
const WM_LBUTTONUP = 0x202;
const WM_LBUTTONDBLCLK = 0x203;
const MK_LBUTTON = 1;
const lp = (x: number, y: number) => ((y & 0xffff) << 16) | (x & 0xffff);

async function waitHome(client: ControlClient): Promise<HomeSelection> {
  const deadline = Date.now() + 8000;
  for (;;) {
    const h = await client.homeSelection();
    if (h.ready && h.tipRect[2]! > 0) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`home-tip-double-click: no tip band on the home page: ${h.raw}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("home-tip-double-click");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowTips = true\n",
  );
  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    const before = await waitHome(client);
    const canvas = findCanvas(frame);
    // the band's left edge, away from the tip text and its links
    const [x, y, , dy] = before.tipRect as [number, number, number, number];
    const at = lp(x + 4, y + Math.floor(dy / 2));
    const tipPixels = () => Buffer.from(captureWindowDCRegionPixels(canvas, x, y, before.tipRect[2]!, dy)!);
    const beforePixels = tipPixels();

    postMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, at);
    postMessage(canvas, WM_LBUTTONUP, 0, at);
    await sleep(300);
    const afterClick = await client.homeSelection();
    if (afterClick.tip !== before.tip) {
      throw new Error(`home-tip-double-click: a single click changed the tip ${before.tip} -> ${afterClick.tip}`);
    }

    postMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, at);
    postMessage(canvas, WM_LBUTTONUP, 0, at);
    postMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, at);
    postMessage(canvas, WM_LBUTTONUP, 0, at);
    const deadline = Date.now() + 3000;
    let after = await client.homeSelection();
    while (after.tip === before.tip && Date.now() < deadline) {
      await sleep(50);
      after = await client.homeSelection();
    }
    if (after.tip === before.tip) {
      throw new Error(`home-tip-double-click: a double-click didn't change the tip ${before.tip}`);
    }
    // and the band shows it
    const drawnDeadline = Date.now() + 3000;
    while (Buffer.compare(tipPixels(), beforePixels) === 0) {
      if (Date.now() > drawnDeadline) {
        throw new Error(`home-tip-double-click: the tip changed ${before.tip} -> ${after.tip} but wasn't redrawn`);
      }
      await sleep(50);
    }
    console.log("home-tip-double-click: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
