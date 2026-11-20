// ng: a trackpad pinch over the document zooms it continuously; one that
// starts off the canvas (the toolbar) is left alone. macOS delivers the pinch;
// TestInput "pinch" drives the same canvas code on every platform.
import { resolve } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { USE_NG, runStandalone } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const PDF = resolve("ext/a-zlib/zlib.3.pdf");
const BEGIN = 0;
const CHANGE = 1;
const END = 2;

async function pinch(client: ControlClient, x: number, y: number, phase: number, milli: number) {
  const raw = String((await client.request(ControlCommand.TestInput, ["pinch", x, y, phase, milli]))[1] ?? "");
  const m = /^OK handled=(\d) zoom=([\d.]+)/.exec(raw);
  if (!m) {
    throw new Error(`trackpad-pinch-zoom: TestInput pinch failed: ${raw}`);
  }
  return { handled: m[1] === "1", zoom: +m[2]! };
}

export async function testit(): Promise<void> {
  if (!USE_NG) return;
  const { proc, client } = await launchControlled(["-zoom", "100", PDF]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const raw = String((await client.request(ControlCommand.TestLayout, ["get"]))[1] ?? "");
    const c = /item name=canvas visible=1 rect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
    if (!c) {
      throw new Error(`trackpad-pinch-zoom: no canvas in layout: ${raw}`);
    }
    const x = +c[1]! + (+c[3]! >> 1);
    const y = +c[2]! + (+c[4]! >> 1);

    const start = await pinch(client, x, y, BEGIN, 0);
    await pinch(client, x, y, CHANGE, 250);
    const grown = await pinch(client, x, y, CHANGE, 250);
    const end = await pinch(client, x, y, END, 0);
    // 100% * (1 + 0.25 + 0.25)
    if (!grown.handled || !end.handled || Math.abs(grown.zoom - start.zoom * 1.5) > 1) {
      throw new Error(`trackpad-pinch-zoom: zoom ${start.zoom} -> ${grown.zoom} (handled ${grown.handled})`);
    }

    // above the canvas: not the canvas' gesture
    const off = await pinch(client, x, Math.max(0, +c[2]! - 20), BEGIN, 0);
    const offChange = await pinch(client, x, Math.max(0, +c[2]! - 20), CHANGE, 250);
    await pinch(client, x, Math.max(0, +c[2]! - 20), END, 0);
    if (off.handled || offChange.handled || offChange.zoom !== grown.zoom) {
      throw new Error(`trackpad-pinch-zoom: pinch off the canvas zoomed ${grown.zoom} -> ${offChange.zoom}`);
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
