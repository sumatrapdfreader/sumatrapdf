// #6561: documents shown through a converted PDF (CAJ, DVI, PostScript) didn't
// get the custom page colors: the recolor pass only ran for the PDF engine
// itself. Uses DVI, which goes through the same converted-PDF engine as CAJ
// and only needs a TeX install. A white page must come out in the
// FixedPageUI.BackgroundColor, as a PDF's does.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { dviConverterAvailable, makeBlankDvi } from "./dvi-open.ts";
import { makeBlankPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const kSettings = `FixedPageUI [
    BackgroundColor = #ffff00
]
`;

function yellowShare(canvas: number): number {
  const px = captureWindowPixels(canvas)!;
  let yellow = 0;
  for (let i = 0; i < px.data.length; i += 4) {
    // BGRA
    if (px.data[i]! < 40 && px.data[i + 1]! > 215 && px.data[i + 2]! > 215) yellow++;
  }
  return yellow / (px.w * px.h);
}

async function pageIsYellow(client: ControlClient, canvas: number): Promise<number> {
  const deadline = Date.now() + 10_000 * SLOW_BUILD_FACTOR;
  let share = 0;
  do {
    await client.waitForRenderIdle(120_000);
    share = yellowShare(canvas);
    if (share > 0.2) return share;
    await sleep(100);
  } while (Date.now() < deadline);
  return share;
}

async function check(appdata: string, doc: string, what: string): Promise<void> {
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, "-zoom", "fit page", doc]);
  try {
    await client.setNotificationsEnabled(false);
    const share = await pageIsYellow(client, findCanvas(frame));
    if (share <= 0.2) {
      throw new Error(`issue-6561: ${what}: page isn't the custom background (${Math.round(share * 100)}% yellow)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (!dviConverterAvailable()) {
    console.log("SKIP issue-6561: needs dvipdfmx (MiKTeX, TeX Live) to open a DVI");
    return;
  }
  const dir = tmpPath("issue-6561");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), kSettings);
  const pdf = join(dir, "blank.pdf");
  writeFileSync(pdf, makeBlankPdf(), "latin1");
  const dvi = join(dir, "blank.dvi");
  writeFileSync(dvi, makeBlankDvi());

  await check(appdata, pdf, "PDF");
  await check(appdata, dvi, "DVI");
}

if (import.meta.main) {
  await runStandalone(testit);
}
