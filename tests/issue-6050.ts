// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6050
//
// DocumentColorsFollowTheme used to recolor the rendered page bitmap for EPUB
// (and other reflowable MuPDF formats), which inverted some images. Those
// formats now inject CSS for text/background; images stay as in the file even
// in `legacy` (the PDF bitmap-invert mode).
//
// `legacy` is the mode that would invert this red PNG without the CSS path.
//
// Run: bun tests/issue-6050.ts [--no-build]   (or via tests/run-almost-all.ts)

import { deflateSync } from "node:zlib";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, EXE, runStandalone, tmpPath, pngChunk, makeZip } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, sendCommand, waitForFrame } from "./win-automation.ts";

const IMG = 80;
const RED_R = 255;
const RED_G = 0;
const RED_B = 0;

function makeRedPng(size: number): Buffer {
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(size, 0);
  ihdr.writeUInt32BE(size, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  const stride = 1 + size * 3;
  const raw = Buffer.alloc(stride * size);
  for (let y = 0; y < size; y++) {
    const row = y * stride;
    raw[row] = 0;
    for (let x = 0; x < size; x++) {
      const i = row + 1 + x * 3;
      raw[i] = RED_R;
      raw[i + 1] = RED_G;
      raw[i + 2] = RED_B;
    }
  }
  const sig = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]);
  return Buffer.concat([
    sig,
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

function makeEpub(png: Buffer): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">urn:uuid:issue-6050</dc:identifier>` +
    `<dc:title>theme css image</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="img" href="red.png" media-type="image/png"/>` +
    `</manifest><spine><itemref idref="c1"/></spine></package>`;
  const html =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>t</title></head>` +
    `<body style="margin:0"><img src="red.png" width="${IMG}" height="${IMG}" alt="red"/></body></html>`;
  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(html) },
    { name: "OEBPS/red.png", data: png, store: true },
  ]);
}

function countNear(data: Uint8Array, r: number, g: number, b: number, slop: number): number {
  let n = 0;
  for (let i = 0; i < data.length; i += 4) {
    const pb = data[i]!;
    const pg = data[i + 1]!;
    const pr = data[i + 2]!;
    if (Math.abs(pr - r) <= slop && Math.abs(pg - g) <= slop && Math.abs(pb - b) <= slop) {
      n++;
    }
  }
  return n;
}

function captureCanvas(frame: number, label: string): { data: Uint8Array; w: number; h: number } {
  const canvas = findCanvas(frame);
  if (!canvas) {
    throw new Error(`issue-6050: ${label}: no canvas`);
  }
  const cap = captureWindowPixels(canvas);
  if (!cap) {
    throw new Error(`issue-6050: ${label}: capture failed`);
  }
  return cap;
}

function assertRedImage(
  cap: { data: Uint8Array; w: number; h: number },
  label: string,
  checkPageNotWhite: boolean,
): void {
  const red = countNear(cap.data, RED_R, RED_G, RED_B, 20);
  const white = countNear(cap.data, 255, 255, 255, 8);
  if (red < 200) {
    throw new Error(`issue-6050: ${label}: red image was recolored (red=${red} white=${white} ${cap.w}x${cap.h})`);
  }
  if (checkPageNotWhite && white > cap.w * cap.h * 0.2) {
    throw new Error(
      `issue-6050: ${label}: page still mostly white; theme CSS did not apply (red=${red} white=${white} ${cap.w}x${cap.h})`,
    );
  }
  console.log(`  ${label}: red=${red} white=${white} ${cap.w}x${cap.h} ✓`);
}

export async function testit(): Promise<void> {
  // legacy mode inverts the painted bitmap. An engine render does not.
  if (IS_MAC) {
    console.log("SKIP issue-6050: the check reads canvas pixels with GetWindowDC");
    return;
  }
  const dir = tmpPath("issue-6050");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epubPath = join(dir, "issue-6050.epub");
  writeFileSync(epubPath, makeEpub(makeRedPng(IMG)));
  const appData = join(dir, "appdata");
  mkdirSync(appData);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "Theme = Dark",
      "DocumentColorsFollowTheme = legacy",
      "",
    ].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);
      assertRedImage(captureCanvas(frame, "legacy dark"), "legacy dark", true);

      sendCommand(frame, cmdId("CmdInvertColors"));
      await client.waitForRenderIdle(30000);
      // invert swaps to a light page; the image must still be red (not cyan)
      assertRedImage(captureCanvas(frame, "after invert"), "after invert", false);
    },
    ["-appdata", appData, "-view", "single page", "-zoom", "fit page", epubPath],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
