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

import { deflateRawSync, deflateSync } from "node:zlib";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, sendCommand, waitForFrame } from "./win-automation.ts";

const IMG = 80;
const RED_R = 255;
const RED_G = 0;
const RED_B = 0;

function crc32(buf: Uint8Array): number {
  const table = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    table[i] = c >>> 0;
  }
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) {
    c = table[(c ^ buf[i]!) & 0xff]! ^ (c >>> 8);
  }
  return (c ^ 0xffffffff) >>> 0;
}

function pngChunk(type: string, data: Buffer): Buffer {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length, 0);
  const body = Buffer.concat([Buffer.from(type, "ascii"), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body), 0);
  return Buffer.concat([len, body, crc]);
}

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

type ZipEntry = { name: string; data: Uint8Array; store?: boolean };

function zip(entries: ZipEntry[]): Buffer {
  const chunks: Uint8Array[] = [];
  const central: Uint8Array[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = new TextEncoder().encode(e.name);
    const store = e.store === true;
    const body = store ? e.data : new Uint8Array(deflateRawSync(e.data));
    const crc = crc32(e.data);
    const local = new DataView(new ArrayBuffer(30));
    local.setUint32(0, 0x04034b50, true);
    local.setUint16(4, 20, true);
    local.setUint16(8, store ? 0 : 8, true);
    local.setUint32(14, crc, true);
    local.setUint32(18, body.length, true);
    local.setUint32(22, e.data.length, true);
    local.setUint16(26, name.length, true);
    chunks.push(new Uint8Array(local.buffer), name, body);

    const cd = new DataView(new ArrayBuffer(46));
    cd.setUint32(0, 0x02014b50, true);
    cd.setUint16(4, 20, true);
    cd.setUint16(6, 20, true);
    cd.setUint16(10, store ? 0 : 8, true);
    cd.setUint32(16, crc, true);
    cd.setUint32(20, body.length, true);
    cd.setUint32(24, e.data.length, true);
    cd.setUint16(28, name.length, true);
    cd.setUint32(42, offset, true);
    central.push(new Uint8Array(cd.buffer), name);
    offset += 30 + name.length + body.length;
  }
  const cdSize = central.reduce((n, c) => n + c.length, 0);
  const end = new DataView(new ArrayBuffer(22));
  end.setUint32(0, 0x06054b50, true);
  end.setUint16(8, entries.length, true);
  end.setUint16(10, entries.length, true);
  end.setUint32(12, cdSize, true);
  end.setUint32(16, offset, true);
  const all = [...chunks, ...central, new Uint8Array(end.buffer)];
  const total = all.reduce((n, c) => n + c.length, 0);
  const out = Buffer.alloc(total);
  let p = 0;
  for (const c of all) {
    out.set(c, p);
    p += c.length;
  }
  return out;
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
  return zip([
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
