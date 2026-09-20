// Lazy-layout comics (continuous mode) render a not-yet-measured page with the
// estimated media box, so a page taller than the estimate got a bitmap of the
// estimate's shape. Measuring it later laid it out at the same fit-width zoom,
// the cache still matched and painted that bitmap stretched (#6225).
//
// Run: bun tests/issue-6225.ts [--no-build]
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";
import { captureWindowPixels } from "./winapi.ts";

const PAGE_W = 400;
const SHORT_H = 1000;
const TALL_H = 2000;
const BAND_H = 25; // the tall page alternates red/blue bands this high
const N_SHORT = 11;
const TALL_PAGE = N_SHORT + 1;
// PAGE_W x TALL_H, bands BAND_H high; regenerate with tests/issue-6225-make-fixture.py
const TALL_JPG = join(dirname(fileURLToPath(import.meta.url)), "issue-6225.jpg");

function crc32(buf: Buffer): number {
  let crc = 0xffffffff;
  for (let n = 0; n < buf.length; n++) {
    let c = (crc ^ buf[n]!) & 0xff;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    crc = (crc >>> 8) ^ c;
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function pngChunk(type: string, data: Buffer): Buffer {
  const len = Buffer.alloc(4);
  len.writeUInt32BE(data.length);
  const body = Buffer.concat([Buffer.from(type, "latin1"), data]);
  const crc = Buffer.alloc(4);
  crc.writeUInt32BE(crc32(body));
  return Buffer.concat([len, body, crc]);
}

// 8-bit RGB PNG; rowColor picks the color of each row
function makePng(w: number, h: number, rowColor: (y: number) => [number, number, number]): Buffer {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) {
    const [r, g, b] = rowColor(y);
    const row = y * (w * 3 + 1) + 1;
    for (let x = 0; x < w; x++) {
      raw[row + x * 3] = r;
      raw[row + x * 3 + 1] = g;
      raw[row + x * 3 + 2] = b;
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  return Buffer.concat([
    Buffer.from("89504e470d0a1a0a", "hex"),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", deflateSync(raw)),
    pngChunk("IEND", Buffer.alloc(0)),
  ]);
}

function makeZip(entries: { name: string; data: Buffer }[]): Buffer {
  const locals: Buffer[] = [];
  const centrals: Buffer[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = Buffer.from(e.name, "latin1");
    const crc = crc32(e.data);
    const lh = Buffer.alloc(30);
    lh.writeUInt32LE(0x04034b50, 0);
    lh.writeUInt16LE(20, 4);
    lh.writeUInt32LE(crc, 14);
    lh.writeUInt32LE(e.data.length, 18);
    lh.writeUInt32LE(e.data.length, 22);
    lh.writeUInt16LE(name.length, 26);
    locals.push(lh, name, e.data);
    const ch = Buffer.alloc(46);
    ch.writeUInt32LE(0x02014b50, 0);
    ch.writeUInt16LE(20, 4);
    ch.writeUInt16LE(20, 6);
    ch.writeUInt32LE(crc, 16);
    ch.writeUInt32LE(e.data.length, 20);
    ch.writeUInt32LE(e.data.length, 24);
    ch.writeUInt16LE(name.length, 28);
    ch.writeUInt32LE(offset, 42);
    centrals.push(ch, name);
    offset += 30 + name.length + e.data.length;
  }
  const localBuf = Buffer.concat(locals);
  const centralBuf = Buffer.concat(centrals);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralBuf.length, 12);
  end.writeUInt32LE(localBuf.length, 16);
  return Buffer.concat([localBuf, centralBuf, end]);
}

// height in pixels of the first complete red/blue band down the middle column,
// and how wide the page is on screen (its non-background pixels)
function measureBand(canvas: number): { bandPx: number; pageWidthPx: number; h: number } {
  const px = captureWindowPixels(canvas);
  if (!px) {
    throw new Error("issue-6225: could not capture the canvas");
  }
  const { w, h, data } = px;
  const kind = (x: number, y: number): "r" | "b" | "-" => {
    const i = (y * w + x) * 4;
    const b = data[i]!;
    const g = data[i + 1]!;
    const r = data[i + 2]!;
    if (r > 150 && g < 80 && b < 80) {
      return "r";
    }
    if (b > 150 && g < 80 && r < 80) {
      return "b";
    }
    return "-";
  };
  const x = Math.floor(w / 2);
  // rows where the color flips; interpolated pixels between bands are skipped
  const edges: number[] = [];
  let prev: "r" | "b" | "-" = "-";
  for (let y = 0; y < h; y++) {
    const k = kind(x, y);
    if (k === "-") {
      continue;
    }
    if (prev !== "-" && k !== prev) {
      edges.push(y);
    }
    prev = k;
  }
  const y = Math.floor(h / 2);
  let pageWidthPx = 0;
  for (let i = 0; i < w; i++) {
    if (kind(i, y) !== "-") {
      pageWidthPx++;
    }
  }
  const bandPx = edges.length >= 2 ? edges[1]! - edges[0]! : 0;
  return { bandPx, pageWidthPx, h };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6225");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const cbz = join(dir, "pages.cbz");
  const entries: { name: string; data: Buffer }[] = [];
  for (let i = 1; i <= N_SHORT; i++) {
    entries.push({ name: `${String(i).padStart(3, "0")}.png`, data: makePng(PAGE_W, SHORT_H, () => [0, 200, 0]) });
  }
  // JPEG on purpose: mupdf decodes only the requested sub-rect of a JPEG, so a
  // render asked for the estimated (shorter) box gets just the top of the page.
  // A PNG is decoded whole and squashed into the box, which hides the bug.
  entries.push({ name: `${String(TALL_PAGE).padStart(3, "0")}.jpg`, data: readFileSync(TALL_JPG) });
  writeFileSync(cbz, makeZip(entries));
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    ["ReuseInstance = false", "RestoreSession = false", "ShowStartPage = false", "CheckForUpdates = false"].join("\n"),
  );

  // on the command line: ComicBookUI.DefaultZoom (fit page) overrides DefaultZoom
  const args = ["-appdata", dir, "-view", "continuous", "-zoom", "fit width", cbz];
  const { proc, client, frame } = await launchControlled(args);
  try {
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();
    // the page before the tall one: the tall page is prerendered as its
    // neighbour while its size is still the estimate (a short page)
    const at = await client.goToLocation(0, TALL_PAGE - 1);
    if (at.page !== TALL_PAGE - 1) {
      throw new Error(`issue-6225: landed on page ${at.page}, expected ${TALL_PAGE - 1}`);
    }
    await client.waitForRenderIdle();
    await client.goToLocation(0, TALL_PAGE);
    await client.waitForRenderIdle();

    const canvas = findCanvas(frame);
    const { bandPx, pageWidthPx, h } = measureBand(canvas);
    if (pageWidthPx === 0 || bandPx === 0) {
      throw new Error(`issue-6225: no red/blue bands on screen (pageWidthPx=${pageWidthPx} bandPx=${bandPx} h=${h})`);
    }
    // fit width: one page unit is pageWidthPx / PAGE_W pixels
    const want = (BAND_H * pageWidthPx) / PAGE_W;
    if (Math.abs(bandPx - want) > want * 0.3) {
      throw new Error(
        `issue-6225: band is ${bandPx}px on screen, expected ~${want.toFixed(1)}px: page drawn at the wrong scale`,
      );
    }
    console.log(`issue-6225: band ${bandPx}px, expected ~${want.toFixed(1)}px`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
