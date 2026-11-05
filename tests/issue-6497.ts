// Test for issue #6497: FitWidthPercent makes Fit Width fit the page to that
// percentage of the window, a reading column in the middle of a wide screen.
//
// Opens the same PDF at Fit Width with FitWidthPercent = 100 and 50 and compares
// the page's width on screen; Fit Page must not change.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, makeTopBottomPdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

// "OK pages=2 widths=... screen=612,792" => 612
async function pageDx(client: ControlClient): Promise<number> {
  let dx = 0;
  for (let i = 0; i < 50 && dx === 0; i++) {
    const [, raw] = await client.request(ControlCommand.TestPageInfo, []);
    const m = /screen=(\d+),(\d+)/.exec(String(raw ?? ""));
    dx = m ? Number(m[1]) : 0;
    if (dx === 0) {
      await sleep(100);
    }
  }
  return dx;
}

async function openAt(name: string, zoom: string, percent: number): Promise<number> {
  const dir = tmpPath(`issue-6497-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-6497.pdf");
  writeFileSync(pdf, makeTopBottomPdf(2));
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `RestoreSession = false\nFitWidthPercent = ${percent}\n`);
  let dx = 0;
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      dx = await pageDx(client);
    },
    ["-appdata", dir, "-zoom", zoom, pdf],
  );
  return dx;
}

export async function testit(): Promise<void> {
  const full = await openAt("full", "fit width", 100);
  const half = await openAt("half", "fit width", 50);
  const ratio = half / full;
  if (full === 0 || Math.abs(ratio - 0.5) > 0.03) {
    throw new Error(`issue-6497: Fit Width page is ${half}px at 50%, ${full}px at 100%: ratio ${ratio}`);
  }

  // Fit Page ignores it
  const pageFull = await openAt("page-full", "fit page", 100);
  const pageHalf = await openAt("page-half", "fit page", 50);
  if (Math.abs(pageFull - pageHalf) > 1) {
    throw new Error(`issue-6497: Fit Page changed with FitWidthPercent: ${pageFull}px vs ${pageHalf}px`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
