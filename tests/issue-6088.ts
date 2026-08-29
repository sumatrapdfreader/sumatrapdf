// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6088
//
// Dark theme + DocumentColorsFollowTheme = smart preserved figures that are
// artwork drawn on a uniform light backdrop, so the backdrop stayed a bright
// block on an otherwise dark page. Such an image is a figure on paper, not a
// photo: it has to recolor with the page.
//
// The fixture is one 256x256 image: a saturated green disc on flat 0xcc gray.
// The gray must not survive on screen in dark theme.
//
// Run: bun tests/issue-6088.ts [--no-build]   (or via tests/run-almost-all.ts)

import { deflateSync } from "node:zlib";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, waitForFrame } from "./win-automation.ts";

const IMG_SIZE = 256;
const BACKDROP = 0xcc;
// disc radius covering ~40% of the image, enough saturated pixels that the
// legacy stats call it a picture (which is what made it "preserve")
const DISC_R = 91;

const PAGE_W = 612;
const PAGE_H = 792;
const IMG_PT = 220;
const IMG_X = 100;
const IMG_Y = PAGE_H - 100 - IMG_PT;

function makeImageRgb(): Buffer {
  const px = Buffer.alloc(IMG_SIZE * IMG_SIZE * 3, BACKDROP);
  const c = IMG_SIZE / 2;
  for (let y = 0; y < IMG_SIZE; y++) {
    for (let x = 0; x < IMG_SIZE; x++) {
      const dx = x - c;
      const dy = y - c;
      if (dx * dx + dy * dy > DISC_R * DISC_R) {
        continue;
      }
      // vertical shading so the disc isn't one flat color
      const shade = y / IMG_SIZE;
      const i = (y * IMG_SIZE + x) * 3;
      px[i] = Math.round(60 * shade);
      px[i + 1] = Math.round(60 + 195 * shade);
      px[i + 2] = Math.round(40 + 120 * shade);
    }
  }
  return px;
}

function pdfWithImage(): Buffer {
  const imgData = deflateSync(makeImageRgb());
  const content = Buffer.from(`q ${IMG_PT} 0 0 ${IMG_PT} ${IMG_X} ${IMG_Y} cm /Im0 Do Q\n`, "latin1");
  const objs: Buffer[] = [
    Buffer.from("<</Type/Catalog/Pages 2 0 R>>", "latin1"),
    Buffer.from("<</Type/Pages/Kids[3 0 R]/Count 1>>", "latin1"),
    Buffer.from(
      `<</Type/Page/Parent 2 0 R/MediaBox[0 0 ${PAGE_W} ${PAGE_H}]` +
        `/Resources<</XObject<</Im0 5 0 R>>>>/Contents 4 0 R>>`,
      "latin1",
    ),
    Buffer.concat([
      Buffer.from(`<</Length ${content.length}>>\nstream\n`, "latin1"),
      content,
      Buffer.from("endstream", "latin1"),
    ]),
    Buffer.concat([
      Buffer.from(
        `<</Type/XObject/Subtype/Image/Width ${IMG_SIZE}/Height ${IMG_SIZE}` +
          `/ColorSpace/DeviceRGB/BitsPerComponent 8/Filter/FlateDecode/Length ${imgData.length}>>\nstream\n`,
        "latin1",
      ),
      imgData,
      Buffer.from("\nendstream", "latin1"),
    ]),
  ];

  const parts: Buffer[] = [Buffer.from("%PDF-1.5\n%\xe2\xe3\xcf\xd3\n", "latin1")];
  const offsets: number[] = [];
  let at = parts[0]!.length;
  for (let i = 0; i < objs.length; i++) {
    const b = Buffer.concat([Buffer.from(`${i + 1} 0 obj\n`, "latin1"), objs[i]!, Buffer.from("\nendobj\n", "latin1")]);
    offsets.push(at);
    parts.push(b);
    at += b.length;
  }
  let xref = `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    xref += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  xref += `trailer\n<</Size ${objs.length + 1}/Root 1 0 R>>\nstartxref\n${at}\n%%EOF\n`;
  parts.push(Buffer.from(xref, "latin1"));
  return Buffer.concat(parts);
}

function countBackdrop(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (Math.abs(r - BACKDROP) <= 12 && Math.abs(g - BACKDROP) <= 12 && Math.abs(b - BACKDROP) <= 12) {
      n++;
    }
  }
  return n;
}

// pixels that are neither the dark page nor the light backdrop, i.e. the disc
function countMidtone(data: Uint8Array): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const lum = (0.2126 * data[i + 2]! + 0.7152 * data[i + 1]! + 0.0722 * data[i]!) / 255;
    if (lum > 0.1 && lum < 0.65) {
      n++;
    }
  }
  return n;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6088");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfPath = join(dir, "issue-6088.pdf");
  writeFileSync(pdfPath, pdfWithImage());
  const appData = join(dir, "appdata");
  mkdirSync(appData);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "Theme = Dark",
      "DocumentColorsFollowTheme = smart",
      "",
    ].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-6088: no canvas");
      }
      const cap = captureWindowPixels(canvas);
      if (!cap) {
        throw new Error("issue-6088: capture failed");
      }
      const midtone = countMidtone(cap.data);
      if (midtone < 5000) {
        throw new Error(`issue-6088: figure not rendered (midtone=${midtone} ${cap.w}x${cap.h})`);
      }
      const backdrop = countBackdrop(cap.data);
      if (backdrop > 500) {
        throw new Error(
          `issue-6088: light backdrop kept in dark theme (backdrop=${backdrop} midtone=${midtone} ${cap.w}x${cap.h})`,
        );
      }
      console.log(`  backdrop=${backdrop} midtone=${midtone} ${cap.w}x${cap.h} ✓`);
    },
    ["-appdata", appData, "-view", "single page", "-zoom", "fit page", pdfPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
