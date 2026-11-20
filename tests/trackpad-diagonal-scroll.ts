// ng: a trackpad slide carries both axes in one wheel event. The canvas used
// only deltaY, so a zoomed page couldn't be panned diagonally.
import { resolve } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { USE_NG, runStandalone } from "./util.ts";
import { killAndWait, launchControlled, ngInput } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const PDF = resolve("ext/a-zlib/zlib.3.pdf");
const NOTCH = 120;

type Layout = { canvas: { x: number; y: number; dx: number; dy: number }; v: number; h: number };

async function layout(client: ControlClient): Promise<Layout> {
  const raw = String((await client.request(ControlCommand.TestLayout, ["get"]))[1] ?? "");
  const c = /item name=canvas visible=1 rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  const v = /scrollV pos=(-?\d+)/.exec(raw);
  const h = /scrollH pos=(-?\d+)/.exec(raw);
  if (!c || !v || !h) {
    throw new Error(`trackpad-diagonal-scroll: bad layout: ${raw}`);
  }
  return { canvas: { x: +c[1]!, y: +c[2]!, dx: +c[3]!, dy: +c[4]! }, v: +v[1]!, h: +h[1]! };
}

export async function testit(): Promise<void> {
  if (!USE_NG) return;
  const { proc, client } = await launchControlled(["-zoom", "400", PDF]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const before = await layout(client);
    const x = before.canvas.x + (before.canvas.dx >> 1);
    const y = before.canvas.y + (before.canvas.dy >> 1);

    // down and to the right: negative deltas
    for (let i = 0; i < 3; i++) {
      await ngInput(client, ["wheelxy", x, y, -NOTCH, -NOTCH]);
    }

    let after = before;
    for (let i = 0; i < 40; i++) {
      await sleep(50);
      after = await layout(client);
      if (after.v > before.v && after.h > before.h) {
        break;
      }
    }
    if (after.v <= before.v || after.h <= before.h) {
      throw new Error(`trackpad-diagonal-scroll: scroll v ${before.v} -> ${after.v}, h ${before.h} -> ${after.h}`);
    }
    await client.quit();
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
