// issue #6150: printing an image file from the command line produced a blank
// page. The job reached the printer, so the panel showed it as printed, but it
// carried no ink.
//
// PrintPageInBands() discarded any rendered band whose Pixmap had no DIB
// section (hbmp). Only the mupdf engine returns DIB-backed pixmaps; the image
// engine returns heap ones, which BlitPixmap() draws through StretchDIBits just
// fine. Every band was thrown away, the band height halved down to one row, and
// the page came out empty. Comic books (CBZ/CBR) render through the same engine
// and were blank too.

import { copyFileSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { runStandalone, tmpPath } from "./util.ts";
import { pdfPageCount, requirePrintToPdf, runPrintToPdf, tempPrintOutput } from "./print-util.ts";

const SRC_GIF = join(import.meta.dir, "issue-6150.gif");

// the Print to PDF driver embeds the page as an image XObject; a blank page has
// none at all (the pre-fix output was under 1 KB, with an empty /Resources)
function pdfHasImage(path: string): boolean {
  const text = readFileSync(path).toString("latin1");
  return /\/Subtype\s*\/Image/.test(text);
}

export async function testit(): Promise<void> {
  requirePrintToPdf();

  const dir = tmpPath("issue-6150");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  // a name with no spaces: this test is about the image engine, not the path
  const gif = join(dir, "issue-6150.gif");
  copyFileSync(SRC_GIF, gif);

  const out = tempPrintOutput("issue-6150");
  try {
    const res = runPrintToPdf(gif, out, "");
    if (!res.ok) {
      throw new Error(`issue-6150: printing the gif failed (exit ${res.exitCode}): ${res.stderr.trim()}`);
    }
    const pages = pdfPageCount(out);
    if (pages !== 1) {
      throw new Error(`issue-6150: expected 1 page, got ${pages}`);
    }
    if (!pdfHasImage(out)) {
      throw new Error("issue-6150: the printed page has no image on it (blank page)");
    }
    console.log("issue-6150: OK");
  } finally {
    rmSync(out, { force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
