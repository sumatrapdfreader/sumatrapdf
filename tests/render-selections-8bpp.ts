// A selection spanning two pages is rendered as one image (Google Lens). The
// mupdf engine hands back an 8-bit palette DIB for a page with few colors, and
// the combiner copied each row as 32bpp: four times past the row, then off the
// end of the section (crash 2026-09-21-18-57-670c, read fault in memcpy from
// RenderSelectionsAsRenderedBitmap).
//
// Run: bun tests/render-selections-8bpp.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { makeBookmarkedPdf } from "./toc-tree-sent-click.ts";
import { runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("render-selections-8bpp.pdf");
  writeFileSync(pdfPath, makeBookmarkedPdf());

  const { proc, client } = await launchControlled([pdfPath]);
  try {
    await client.waitForRenderIdle();
    const res = await client.request(ControlCommand.TestRenderSelections);
    const code = typeof res[0] === "number" ? res[0] : -1;
    const raw = String(res[1] ?? "").trim();
    if (code !== 0) {
      throw new Error(`render-selections-8bpp: ${raw || code}`);
    }
    const m = /^OK w=(\d+) h=(\d+) format=(\d+) white=(\d+) total=(\d+)$/.exec(raw);
    if (!m) {
      throw new Error(`render-selections-8bpp: could not parse '${raw}'`);
    }
    const white = parseInt(m[4], 10);
    const total = parseInt(m[5], 10);
    // both strips are blank page: every pixel of the combined image is white
    if (total === 0 || white !== total) {
      throw new Error(`render-selections-8bpp: ${white} of ${total} pixels white (${raw})`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
