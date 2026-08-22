// Saving a CMYK bitmap from a PDF:
//   JPEG used to keep CMYK but invert the inks (PDF polarity dumped as a
//   standalone Adobe JPEG). TIFF used to convert to RGB.
// This drives the same GetImageDataForPageElement / TrySaveOriginalAsCmykTiff
// path as Image → Save.
//
// Run: bun tests/cmyk-image-save.ts [--no-build]   (or via tests/run-almost-all.ts)

import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function parseResult(raw: string): Record<string, number> {
  const out: Record<string, number> = {};
  for (const part of raw.trim().split(/\s+/)) {
    const eq = part.indexOf("=");
    if (eq <= 0) {
      continue;
    }
    out[part.slice(0, eq)] = Number(part.slice(eq + 1));
  }
  return out;
}

function near(got: number, want: number, tol: number, label: string): void {
  if (Math.abs(got - want) > tol) {
    throw new Error(`cmyk-image-save: ${label}=${got}, want ${want}±${tol}`);
  }
}

export async function testit(): Promise<void> {
  const jpegPath = tmpPath("cmyk-image-save.jpg");
  const tiffPath = tmpPath("cmyk-image-save.tif");

  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestCmykImageSave, [jpegPath, tiffPath]);
    const raw = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`cmyk-image-save: ${raw.trim()}`);
    }
    const v = parseResult(raw);
    if (v.jpeg_ok !== 1) {
      throw new Error(`cmyk-image-save: jpeg not written:\n${raw}`);
    }
    if (v.tiff_ok !== 1) {
      throw new Error(`cmyk-image-save: tiff not written:\n${raw}`);
    }
    if (v.jpeg_n !== 4) {
      throw new Error(`cmyk-image-save: jpeg is not CMYK (n=${v.jpeg_n}):\n${raw}`);
    }
    if (v.tiff_photo !== 5) {
      throw new Error(`cmyk-image-save: tiff is not CMYK (photometric=${v.tiff_photo}, 2=RGB):\n${raw}`);
    }
    // PDF polarity of the source patch is C=200 M=10 Y=20 K=30. Inverted
    // Adobe-JPEG-as-dumped-from-PDF would decode to ~55,245,235,225.
    near(v.jpeg_c ?? -1, 200, 25, "jpeg C");
    near(v.jpeg_m ?? -1, 10, 25, "jpeg M");
    near(v.jpeg_y ?? -1, 20, 25, "jpeg Y");
    near(v.jpeg_k ?? -1, 30, 25, "jpeg K");
    console.log(
      `  jpeg n=${v.jpeg_n} C=${v.jpeg_c} M=${v.jpeg_m} Y=${v.jpeg_y} K=${v.jpeg_k} tiff photo=${v.tiff_photo} ✓`,
    );
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
