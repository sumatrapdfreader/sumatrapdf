// A recolored EPUB page had a near-white line along its right (and, at other
// zooms, bottom) edge. Reflowable documents get their page colors through CSS:
// MuPDF fills the page's background as a rectangle, which at most zooms ends
// inside the last pixel column / row of a bitmap that was cleared to white.
// The bitmap must start out in the page's background color.
//
// Run: bun tests/issue-6610.ts [--no-build]

import { mkdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";
import { IS_WIN } from "./host.ts";
import { ROOT, USE_NG, makeOnePagePdf, runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { captureWindowPixels } from "./winapi.ts";

const kPageBg = 0x202060;
// a page fits the window whole at each of these; its size in pixels is
// fractional at most of them
const kZooms = ["fit page", "47", "53", "61"];

// number of pixels just outside the page's right and bottom edges that are
// not the color of the window around the page
function countEdgePixels(canvas: number, zoom: string): number {
  const img = captureWindowPixels(canvas, "client");
  if (!img) {
    throw new Error("issue-6610: could not capture the canvas");
  }
  const px = (x: number, y: number) => {
    const i = (y * img.w + x) * 4;
    return (img.data[i + 2] << 16) | (img.data[i + 1] << 8) | img.data[i];
  };
  let [left, top, right, bottom] = [img.w, img.h, -1, -1];
  for (let y = 0; y < img.h; y++) {
    for (let x = 0; x < img.w; x++) {
      if (px(x, y) !== kPageBg) {
        continue;
      }
      left = Math.min(left, x);
      right = Math.max(right, x);
      top = Math.min(top, y);
      bottom = Math.max(bottom, y);
    }
  }
  if (right < 0) {
    throw new Error(`issue-6610: no page in the background color at zoom ${zoom}`);
  }
  if (left < 1 || top < 1 || right + 1 >= img.w || bottom + 1 >= img.h) {
    throw new Error(`issue-6610: the page doesn't fit the window at zoom ${zoom}`);
  }
  let n = 0;
  for (let y = top; y <= bottom; y++) {
    if (px(right + 1, y) !== px(left - 1, y)) {
      n++;
    }
  }
  for (let x = left; x <= right; x++) {
    if (px(x, bottom + 1) !== px(x, top - 1)) {
      n++;
    }
  }
  return n;
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) {
    console.log("SKIP issue-6610: samples the original app's canvas");
    return;
  }
  const dir = tmpPath("issue-6610-data");
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `RestoreSession = false
Theme = Light
FixedPageUI [
    TextColor = #00a000
    BackgroundColor = #${kPageBg.toString(16)}
]
`,
  );
  const epub = join(ROOT, "tests", "issue-6095.epub");
  // a PDF page is recolored as a bitmap, from white: it must not start out in
  // the background color, which that recoloring would turn into something else
  const pdf = join(dir, "page.pdf");
  writeFileSync(pdf, makeOnePagePdf(), "latin1");
  const docs = [...kZooms.map((zoom) => [epub, zoom]), [pdf, "fit page"]];
  for (const [doc, zoom] of docs) {
    const args = ["-appdata", dir, "-zoom", zoom, "-view", "single page", doc];
    const { proc, client, frame } = await launchControlled(args);
    try {
      await client.setNotificationsEnabled(false);
      await client.waitForRenderIdle();
      const n = countEdgePixels(findCanvas(frame), `${zoom} of ${basename(doc)}`);
      if (n > 0) {
        throw new Error(`issue-6610: ${n} pixels along the page's right and bottom edges at zoom ${zoom}`);
      }
    } finally {
      client.close();
      await killAndWait(proc);
    }
  }
  console.log("issue-6610: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
