// A tab's tooltip shows the file's size. It was looked up on every hover, on
// the UI thread, which blocks on an offline network share. The size is now
// looked up once per load (and when the unsaved state changes).
//
// ng only: it counts the lookups through -dbg-control.
// Run: bun tests/tab-tooltip-size.ts [--no-build] -ng

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand } from "./control.ts";
import { makePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const kHovers = 6;

async function lookups(client: ControlClient): Promise<number> {
  const s = String((await client.request(ControlCommand.TestUiState, ["tipsizes", 0]))[1] ?? "");
  const m = /\blookups=(\d+)/.exec(s);
  if (!m) {
    throw new Error(`tab-tooltip-size: bad tipsizes reply: ${s}`);
  }
  return +m[1]!;
}

// the centers of the tabs, Home first
async function tabCenters(client: ControlClient): Promise<{ x: number; y: number }[]> {
  const raw = String((await client.request(ControlCommand.TestTabWidths, []))[1] ?? "");
  return [...raw.matchAll(/^dx=(\d+) .*\n {2}at x=(-?\d+) y=(-?\d+) dy=(\d+)$/gm)].map((m) => ({
    x: +m[2]! + Math.floor(+m[1]! / 2),
    y: +m[3]! + Math.floor(+m[4]! / 2),
  }));
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("SKIP tab-tooltip-size: ng only (run with -ng)");
    return;
  }
  const dir = tmpPath("tab-tooltip-size");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfs = [join(dir, "a.pdf"), join(dir, "b.pdf")];
  for (const p of pdfs) {
    writeFileSync(p, makePdf(2, 601, 0, 0), "latin1");
  }

  const { proc, client } = await launchControlled(pdfs);
  try {
    await client.waitForRenderIdle();
    const tabs = await tabCenters(client);
    if (tabs.length < 3) {
      throw new Error(`tab-tooltip-size: want Home and 2 document tabs, got ${tabs.length}`);
    }
    const before = await lookups(client);
    for (let i = 0; i < kHovers; i++) {
      const t = tabs[1 + (i % 2)]!;
      await client.request(ControlCommand.TestInput, ["move", t.x, t.y, 0, 0]);
      await sleep(50);
    }
    const n = (await lookups(client)) - before;
    console.log(`tab-tooltip-size: ${n} size lookups for ${kHovers} tab hovers`);
    // each tab may look its size up once
    if (n > 2) {
      throw new Error(`tab-tooltip-size: ${n} size lookups for ${kHovers} tab hovers, want <= 2`);
    }
    if (n === 0 && before === 0) {
      throw new Error("tab-tooltip-size: hovering the tabs never built a tooltip");
    }
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
  console.log("tab-tooltip-size: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
