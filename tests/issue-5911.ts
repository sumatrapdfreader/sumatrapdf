// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5911
//
// Dark-theme bitmap recolor used to snap blue hyperlink pixels to a solid
// link color, which turned anti-aliased URL text into a pixelated stencil.
// Recolor must keep intermediate coverage colors between link and page bg.
//
// Run: bun tests/issue-5911.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { assemblePdf, EXE, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { captureWindowPixels, captureWindowToPng } from "./winapi.ts";
import { findCanvas, waitForFrame } from "./win-automation.ts";

const STREAM = [
  "BT",
  "/F1 48 Tf",
  "0 g",
  "72 720 Td",
  "(plain) Tj",
  "0 -80 Td",
  "0 0 1 rg",
  "(https://example.com/) Tj",
  "ET",
].join("\n");

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R /Annots [6 0 R] >>",
    `<< /Length ${STREAM.length} >>\nstream\n${STREAM}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    "<< /Type /Annot /Subtype /Link /Rect [72 640 540 720] /Border [0 0 0] /A << /S /URI /URI (https://example.com/) >> >>",
  ]);
}

function urlBandStats(
  data: Uint8Array,
  w: number,
  h: number,
): { unique: number; pixels: number; modalShare: number; y0: number; y1: number } {
  let y0 = -1;
  let y1 = -1;
  for (let y = 0; y < h; y++) {
    let bright = 0;
    let mid = 0;
    const row = y * w * 4;
    for (let x = 0; x < w; x++) {
      const i = row + x * 4;
      const b = data[i]!;
      const g = data[i + 1]!;
      const r = data[i + 2]!;
      const lum = (r + g + b) / 3;
      if (lum < 18) {
        continue;
      }
      if (r > 200 && g > 200 && b > 200) {
        bright++;
      } else {
        mid++;
      }
    }
    // URL is mid-gray on dark paper; body text is near-white
    if (mid > 12 && bright < 8) {
      if (y0 < 0) {
        y0 = y;
      }
      y1 = y;
    }
  }
  if (y0 < 0 || y1 - y0 < 8) {
    throw new Error(`issue-5911: no URL band (y0=${y0} y1=${y1})`);
  }

  const counts = new Map<number, number>();
  let pixels = 0;
  for (let y = y0; y <= y1; y++) {
    const row = y * w * 4;
    for (let x = 0; x < w; x++) {
      const i = row + x * 4;
      const b = data[i]!;
      const g = data[i + 1]!;
      const r = data[i + 2]!;
      if ((r + g + b) / 3 < 18) {
        continue;
      }
      const key = (r << 16) | (g << 8) | b;
      counts.set(key, (counts.get(key) ?? 0) + 1);
      pixels++;
    }
  }
  let modal = 0;
  for (const n of counts.values()) {
    if (n > modal) {
      modal = n;
    }
  }
  return { unique: counts.size, pixels, modalShare: pixels ? modal / pixels : 0, y0, y1 };
}

export async function testit(): Promise<void> {
  const dir = writeAppdata(
    "issue-5911-appdata",
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "Theme = Dark",
      "DocumentColorsFollowTheme = smart",
      "",
    ].join("\n"),
  );
  const pdfPath = join(dir, "issue-5911.pdf");
  writeFileSync(pdfPath, makePdf(), "latin1");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-5911: no canvas");
      }
      const cap = captureWindowPixels(canvas);
      if (!cap) {
        throw new Error("issue-5911: capture failed");
      }
      captureWindowToPng(canvas, tmpPath("issue-5911-dark.png"));
      const stats = urlBandStats(cap.data, cap.w, cap.h);
      console.log(
        `  dark url: unique=${stats.unique} pixels=${stats.pixels} modal=${stats.modalShare.toFixed(2)} ` +
          `y=${stats.y0}-${stats.y1} ${cap.w}x${cap.h}`,
      );
      if (stats.pixels < 80) {
        throw new Error(`issue-5911: too few URL pixels (${stats.pixels})`);
      }
      // snapping collapses AA to one solid color; coverage remap keeps a ramp
      if (stats.unique < 8 || stats.modalShare > 0.85) {
        throw new Error(
          `issue-5911: hyperlink AA snapped (unique=${stats.unique} modal=${stats.modalShare.toFixed(2)})`,
        );
      }
    },
    ["-appdata", dir, "-view", "single page", "-zoom", "200", pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
