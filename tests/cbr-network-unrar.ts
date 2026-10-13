// A cbx under 32 MB on a network drive is read into memory and opened by
// libarchive. libarchive only decompresses RAR 2.9+ (and RAR5), so an older
// comic (RAR 2.0, common for 2005-era .cbr) listed its pages but every page
// failed to load. RAR must go through unrar.dll, which needs a path: the
// network copy path.
//
// Builds a stored RAR with two PNG pages (no rar tool is needed; stored
// entries are just headers + bytes) and opens it through the drive's
// administrative share (\\localhost\c$\...), which counts as a network path.
// The fixture is one both libraries can read, so the test checks the route,
// not the decompression. Skips where the share is not reachable.
//
// Run: bun tests/cbr-network-unrar.ts [--no-build]
import { existsSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { crc32, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels } from "./winapi.ts";
import { findCanvas, launchControlled } from "./win-automation.ts";

function u32be(n: number): Uint8Array {
  return new Uint8Array([(n >>> 24) & 0xff, (n >>> 16) & 0xff, (n >>> 8) & 0xff, n & 0xff]);
}

function concat(parts: Uint8Array[]): Uint8Array {
  const out = new Uint8Array(parts.reduce((n, p) => n + p.length, 0));
  let off = 0;
  for (const p of parts) {
    out.set(p, off);
    off += p.length;
  }
  return out;
}

function pngChunk(type: string, data: Uint8Array): Uint8Array {
  const typeAndData = concat([new TextEncoder().encode(type), data]);
  return concat([u32be(data.length), typeAndData, u32be(crc32(typeAndData))]);
}

// 16x16 RGB PNG filled with one color
function makePng(r: number, g: number, b: number): Uint8Array {
  const size = 16;
  const raw = new Uint8Array(size * (1 + size * 3));
  for (let y = 0; y < size; y++) {
    const row = y * (1 + size * 3);
    for (let x = 0; x < size; x++) {
      raw.set([r, g, b], row + 1 + x * 3);
    }
  }
  const ihdr = concat([u32be(size), u32be(size), new Uint8Array([8, 2, 0, 0, 0])]);
  return concat([
    new Uint8Array([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
    pngChunk("IHDR", ihdr),
    pngChunk("IDAT", new Uint8Array(deflateSync(raw))),
    pngChunk("IEND", new Uint8Array(0)),
  ]);
}

const kRarMarker = new Uint8Array([0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00]);
const kBlockMain = 0x73;
const kBlockFile = 0x74;
const kBlockEnd = 0x7b;
const kFlagLongBlock = 0x8000;
const kHostWin32 = 2;
const kUnpVer29 = 29;
const kMethodStore = 0x30;

// a RAR 1.5-4.x block: HEAD_CRC is the low 16 bits of the crc32 of the rest
function rarBlock(type: number, flags: number, body: Uint8Array): Uint8Array {
  const size = 7 + body.length;
  const rest = concat([new Uint8Array([type, flags & 0xff, flags >> 8, size & 0xff, size >> 8]), body]);
  const crc = crc32(rest) & 0xffff;
  return concat([new Uint8Array([crc & 0xff, crc >> 8]), rest]);
}

function rarStoredFile(name: string, data: Uint8Array): Uint8Array {
  const nameBytes = new TextEncoder().encode(name);
  const body = new Uint8Array(25 + nameBytes.length);
  const dv = new DataView(body.buffer);
  dv.setUint32(0, data.length, true); // PACK_SIZE
  dv.setUint32(4, data.length, true); // UNP_SIZE
  body[8] = kHostWin32;
  dv.setUint32(9, crc32(data), true); // FILE_CRC
  dv.setUint32(13, 0, true); // FTIME
  body[17] = kUnpVer29;
  body[18] = kMethodStore;
  dv.setUint16(19, nameBytes.length, true);
  dv.setUint32(21, 0x20, true); // ATTR
  body.set(nameBytes, 25);
  return concat([rarBlock(kBlockFile, kFlagLongBlock, body), data]);
}

function makeStoredRar(files: [string, Uint8Array][]): Uint8Array {
  return concat([
    kRarMarker,
    rarBlock(kBlockMain, 0, new Uint8Array(6)),
    ...files.map(([name, data]) => rarStoredFile(name, data)),
    rarBlock(kBlockEnd, 0x4000, new Uint8Array(0)),
  ]);
}

// C:\dir\file.cbr => \\localhost\c$\dir\file.cbr
function toUncPath(path: string): string {
  return `\\\\localhost\\${path[0].toLowerCase()}$${path.slice(2)}`;
}

export async function testit(): Promise<void> {
  const cbr = tmpPath("cbr-network-unrar.cbr");
  const log = tmpPath("cbr-network-unrar.log");
  rmSync(log, { force: true });
  writeFileSync(
    cbr,
    makeStoredRar([
      ["001.png", makePng(200, 40, 40)],
      ["002.png", makePng(40, 40, 200)],
    ]),
  );
  const unc = toUncPath(cbr);
  if (!existsSync(unc)) {
    console.log(`\nSKIP cbr-network-unrar: '${unc}' is not reachable (needs the administrative share)`);
    return;
  }

  const { proc, client, frame } = await launchControlled(["-log-to-file", log, unc]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    // page 1 is solid red; a failed render leaves the page white
    const canvas = findCanvas(frame);
    await pollUntil(
      () => countRedPixels(canvas),
      (n) => n > 500,
      { timeoutMs: 10_000, error: "cbr-network-unrar: page 1 did not render" },
    );
  } finally {
    await client.quit();
    await proc.exited;
  }

  const text = readFileSync(log, "utf8");
  if (text.includes("MaybeCreateCbxFromMemory: loaded")) {
    throw new Error("cbr-network-unrar: a RAR on a network drive was opened in memory by libarchive");
  }
  if (!text.includes("EngineCbx::CreateFromFile")) {
    throw new Error("cbr-network-unrar: the RAR was not opened from a file (unrar.dll)");
  }
}

// GDI gives BGRA; red is the one channel over 150 with the other two under 90
function countRedPixels(hwnd: number): number {
  const px = captureWindowPixels(hwnd);
  if (!px) {
    return 0;
  }
  let n = 0;
  for (let i = 0; i < px.data.length; i += 4) {
    const [b, g, r] = [px.data[i], px.data[i + 1], px.data[i + 2]];
    if (r > 150 && g < 90 && b < 90) {
      n++;
    }
  }
  return n;
}

if (import.meta.main) {
  await runStandalone(testit);
}
