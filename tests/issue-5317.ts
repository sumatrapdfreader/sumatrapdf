// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5317
//
// Comic archives whose images live in chapter folders should show those
// folders as nested Bookmarks entries. A single shared directory is stripped
// so a flat comic stays a flat list. ComicInfo.xml bookmarks still win.
//
// Run:  bun tests/issue-5317.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// 1x1 transparent PNG
const PNG = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==",
  "base64",
);

function crc32(buf: Uint8Array): number {
  let c = 0xffffffff;
  for (const b of buf) {
    c ^= b;
    for (let i = 0; i < 8; i++) {
      c = (c >>> 1) ^ (c & 1 ? 0xedb88320 : 0);
    }
  }
  return (c ^ 0xffffffff) >>> 0;
}

function writeU16(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff);
}

function writeU32(out: number[], v: number): void {
  out.push(v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff);
}

// Store-only ZIP (no compression). Names may include '/' for folders.
function writeZip(path: string, files: { name: string; data: Buffer }[]): void {
  const local: number[] = [];
  const central: number[] = [];
  for (const f of files) {
    const name = Buffer.from(f.name, "utf8");
    const crc = crc32(f.data);
    const localOff = local.length;
    writeU32(local, 0x04034b50);
    writeU16(local, 20);
    writeU16(local, 0);
    writeU16(local, 0);
    writeU16(local, 0);
    writeU16(local, 0);
    writeU32(local, crc);
    writeU32(local, f.data.length);
    writeU32(local, f.data.length);
    writeU16(local, name.length);
    writeU16(local, 0);
    for (const b of name) {
      local.push(b);
    }
    for (const b of f.data) {
      local.push(b);
    }

    writeU32(central, 0x02014b50);
    writeU16(central, 20);
    writeU16(central, 20);
    writeU16(central, 0);
    writeU16(central, 0);
    writeU16(central, 0);
    writeU16(central, 0);
    writeU32(central, crc);
    writeU32(central, f.data.length);
    writeU32(central, f.data.length);
    writeU16(central, name.length);
    writeU16(central, 0);
    writeU16(central, 0);
    writeU16(central, 0);
    writeU16(central, 0);
    writeU32(central, 0);
    writeU32(central, localOff);
    for (const b of name) {
      central.push(b);
    }
  }
  const eocd: number[] = [];
  writeU32(eocd, 0x06054b50);
  writeU16(eocd, 0);
  writeU16(eocd, 0);
  writeU16(eocd, files.length);
  writeU16(eocd, files.length);
  writeU32(eocd, central.length);
  writeU32(eocd, local.length);
  writeU16(eocd, 0);
  writeFileSync(path, Buffer.from([...local, ...central, ...eocd]));
}

async function tocOf(client: ControlClient, path: string): Promise<string> {
  const [exitCode, raw] = await client.request(ControlCommand.TestGetToc, [path]);
  if (exitCode !== 0) {
    throw new Error(`issue-5317: TestGetToc failed for ${path}: ${(raw ?? "").trim()}`);
  }
  return String(raw ?? "");
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5317");
  mkdirSync(dir, { recursive: true });

  const nested = join(dir, "chapters.cbz");
  writeZip(nested, [
    { name: "Chapter 01/001.png", data: PNG },
    { name: "Chapter 01/002.png", data: PNG },
    { name: "Chapter 02/001.png", data: PNG },
  ]);
  const nestedWant =
    ["Chapter 01|page=1", "  001.png|page=1", "  002.png|page=2", "Chapter 02|page=3", "  001.png|page=3"].join("\n") +
    "\n";

  const shared = join(dir, "images-prefix.cbz");
  writeZip(shared, [
    { name: "images/001.png", data: PNG },
    { name: "images/002.png", data: PNG },
  ]);
  const sharedWant = ["001.png|page=1", "002.png|page=2"].join("\n") + "\n";

  const flat = join(dir, "flat.cbz");
  writeZip(flat, [
    { name: "001.png", data: PNG },
    { name: "002.png", data: PNG },
  ]);
  const flatWant = ["001.png|page=1", "002.png|page=2"].join("\n") + "\n";

  await withControlledSumatra(EXE, async (client) => {
    const nestedGot = await tocOf(client, nested);
    if (nestedGot !== nestedWant) {
      throw new Error(`issue-5317: nested TOC mismatch.\nexpected:\n${nestedWant}got:\n${nestedGot}`);
    }

    const sharedGot = await tocOf(client, shared);
    if (sharedGot !== sharedWant) {
      throw new Error(`issue-5317: shared-folder TOC should stay flat.\nexpected:\n${sharedWant}got:\n${sharedGot}`);
    }

    const flatGot = await tocOf(client, flat);
    if (flatGot !== flatWant) {
      throw new Error(`issue-5317: flat TOC mismatch.\nexpected:\n${flatWant}got:\n${flatGot}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
