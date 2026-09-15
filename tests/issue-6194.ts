// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6194
//
// A thin annotation is hovered anywhere inside its drawn (inflated) marker, at
// any zoom. Hit-testing used the bare bounds, which shrink below a pixel when
// zoomed out.
//
// Run:  bun tests/issue-6194.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { packCoords, sendMessage, sleep, WM_MOUSEMOVE } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

type Screen = { x: number; y: number; dx: number; dy: number };

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Square /Rect [100 400 500 402] /C [1 0 0] /BS << /W 1 >> >>",
  ]);
}

async function dump(client: ControlClient): Promise<{ hover: boolean; screen: Screen | null; raw: string }> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const st = /state selected=\d hover=(\d)/.exec(raw);
  const sc = /type=Square .* screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (res[0] !== 0 || !st) {
    throw new Error(`issue-6194: could not read state\n${raw}`);
  }
  const screen = sc ? { x: +sc[1]!, y: +sc[2]!, dx: +sc[3]!, dy: +sc[4]! } : null;
  return { hover: st[1] === "1", screen, raw };
}

async function hoverAt(client: ControlClient, canvas: number, x: number, y: number): Promise<boolean> {
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x, y));
  await sleep(100);
  return (await dump(client)).hover;
}

// Windows re-sends the position of the physical cursor, which sits wherever the
// last test left it: such a move lands between the posted one and the read and
// clears the hover, so post again until it sticks.
async function hoverUntil(client: ControlClient, canvas: number, x: number, y: number): Promise<boolean> {
  const deadline = Date.now() + 2000 * SLOW_BUILD_FACTOR;
  do {
    if (await hoverAt(client, canvas, x, y)) {
      return true;
    }
  } while (Date.now() < deadline);
  return false;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6194");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "thin.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
    "-view",
    "single page",
    "-zoom",
    "25",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    const canvas = findCanvas(frame);
    const deadline = Date.now() + 5000 * SLOW_BUILD_FACTOR;
    let d = await dump(client);
    while (!d.screen && Date.now() < deadline) {
      await sleep(50);
      d = await dump(client);
    }
    const r = d.screen;
    if (!r) {
      throw new Error(`issue-6194: square annotation not loaded\n${d.raw}`);
    }

    const cx = r.x + Math.floor(r.dx / 2);
    if (!(await hoverUntil(client, canvas, cx, r.y + Math.floor(r.dy / 2)))) {
      throw new Error(`issue-6194: not hovered on the annotation\n${(await dump(client)).raw}`);
    }
    for (const y of [r.y - 3, r.y + r.dy + 3]) {
      await hoverAt(client, canvas, cx, r.y - 40);
      if (!(await hoverUntil(client, canvas, cx, y))) {
        throw new Error(
          `issue-6194: not hovered 3px off a thin annotation at 25% (y=${y})\n${(await dump(client)).raw}`,
        );
      }
    }
    if (await hoverAt(client, canvas, cx, r.y - 40)) {
      throw new Error(`issue-6194: hovered 40px away from the annotation\n${(await dump(client)).raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("issue-6194: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
