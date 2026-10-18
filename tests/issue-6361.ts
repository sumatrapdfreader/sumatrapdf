// EngineeringDrawingEnhance = auto took a scanned book page (one big image of
// cream paper) for a raster CAD drawing and the gray enhancement ruined its
// colors (issue #6361). Raster CAD is line work on white paper; a page image
// that is not mostly white must not be enhanced. Black lines on white must
// still be.
//
// Run: bun tests/issue-6361.ts [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

const IMG = 64;

// one page that is a single full-page RGB image; pixel(x, y) gives "rrggbb"
function makeImagePdf(pixel: (x: number, y: number) => string): string {
  let hex = "";
  for (let y = 0; y < IMG; y++) {
    for (let x = 0; x < IMG; x++) {
      hex += pixel(x, y);
    }
  }
  // square, since image coverage is measured against the longer side squared
  const content = "q 800 0 0 800 0 0 cm /Im0 Do Q";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 800 800] /Resources << /XObject << /Im0 5 0 R >> >> /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    `<< /Type /XObject /Subtype /Image /Width ${IMG} /Height ${IMG} /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /ASCIIHexDecode /Length ${hex.length + 1} >>\nstream\n${hex}>\nendstream`,
  ]);
}

// cream paper with blotchy mid-tone stains and brown ink, like the scan in the issue
function scanPixel(x: number, y: number): string {
  if (y % 16 === 8) {
    return "5a4632";
  }
  return (x + y) % 5 === 0 ? "a89c88" : "e8dcc4";
}

// a drawing: black lines on white paper
function cadPixel(x: number, y: number): string {
  return x % 16 === 0 || y % 16 === 0 ? "000000" : "ffffff";
}

// the line the engine logs about auto-detection when it opens the document;
// empty when it logged none (a low score isn't logged)
async function detectLine(name: string, pdf: string, appdata: string): Promise<string> {
  const logPath = join(appdata, `${name}.log`);
  const { proc, client } = await launchControlled(["-appdata", appdata, "-log-to-file", logPath, pdf]);
  try {
    await client.waitForRenderIdle();
  } finally {
    client.close();
    await killAndWait(proc);
  }
  const line = readFileSync(logPath, "utf8")
    .split("\n")
    .find((l) => l.includes("CAD enhance"));
  console.log(`issue-6361: ${name}: ${line?.trim() ?? "(not logged)"}`);
  return line ?? "";
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata("issue-6361-appdata", "UiLanguage = en\nEngineeringDrawingEnhance = auto");
  const scanPdf = tmpPath("issue-6361-scan.pdf");
  const cadPdf = tmpPath("issue-6361-cad.pdf");
  writeFileSync(scanPdf, makeImagePdf(scanPixel), "latin1");
  writeFileSync(cadPdf, makeImagePdf(cadPixel), "latin1");

  const scan = await detectLine("scan", scanPdf, appdata);
  if (scan.includes("CAD enhance detect")) {
    throw new Error(`issue-6361: a scanned page on cream paper was taken for CAD: ${scan.trim()}`);
  }
  const cad = await detectLine("cad", cadPdf, appdata);
  if (!cad.includes("reason=raster-image")) {
    throw new Error(`issue-6361: a raster drawing on white paper is no longer detected: ${cad.trim()}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
