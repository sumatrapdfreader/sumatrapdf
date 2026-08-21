// #6018: ComicBookUI.PageSpacing must change the gap between CBZ pages in
// continuous view, PageSpacing must appear in Advanced Settings, and 0 0 must
// not leave a 1px canvas seam (layout size must match EngineImages/tile Round).
//
// CBZ files read ComicBookUI.PageSpacing, not FixedPageUI.PageSpacing.
//
// Run: bun tests/issue-6018.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { deflateSync } from "node:zlib";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { ControlCommand } from "./control.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { readWindowDCColumn, setProcessDpiAware, sleep } from "./winapi.ts";

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

function makePng(w: number, h: number, rgb: [number, number, number]): Buffer {
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y++) {
    const row = y * (w * 3 + 1);
    raw[row] = 0;
    for (let x = 0; x < w; x++) {
      const i = row + 1 + x * 3;
      raw[i] = rgb[0];
      raw[i + 1] = rgb[1];
      raw[i + 2] = rgb[2];
    }
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0);
  ihdr.writeUInt32BE(h, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  return Buffer.concat([
    Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a]),
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
    const name = Buffer.from(e.name, "utf8");
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

type PagePos = { n: number; y: number; dy: number; sy: number; sdy: number; sx: number; sdx: number };

function parsePages(raw: string): { spacingDy: number; pages: PagePos[] } {
  const sm = /pages count=\d+ spacing=\d+,(\d+)/.exec(raw);
  const pages: PagePos[] = [];
  for (const line of raw.split("\n")) {
    const m =
      /^page n=(\d+) shown=\d+ pos=(-?\d+),(-?\d+),(-?\d+),(-?\d+) screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)$/.exec(
        line.trim(),
      );
    if (m) {
      pages.push({
        n: +m[1]!,
        y: +m[3]!,
        dy: +m[5]!,
        sx: +m[6]!,
        sy: +m[7]!,
        sdx: +m[8]!,
        sdy: +m[9]!,
      });
    }
  }
  return { spacingDy: sm ? +sm[1]! : -1, pages };
}

function gapBetween(pages: PagePos[], a: number, b: number): number {
  const p = pages.find((x) => x.n === a);
  const q = pages.find((x) => x.n === b);
  if (!p || !q) {
    throw new Error(`issue-6018: missing page ${a} or ${b} in ${JSON.stringify(pages)}`);
  }
  return q.y - (p.y + p.dy);
}

async function layoutPages(client: { request: (cmd: number, args: unknown[]) => Promise<unknown[]> }): Promise<{
  spacingDy: number;
  pages: PagePos[];
  raw: string;
}> {
  const res = await client.request(ControlCommand.TestLayout, ["get"]);
  const raw = String(res[1] ?? "");
  return { ...parsePages(raw), raw };
}

function writeAppData(dir: string, spacing: string): string {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    [
      "RestoreSession = false",
      "CheckForUpdates = false",
      "ShowToc = false",
      "DefaultDisplayMode = continuous",
      "DefaultZoom = fit width",
      "ComicBookUI [",
      `    PageSpacing = ${spacing}`,
      "]",
      "",
    ].join("\n"),
  );
  return dir;
}

export async function testit(): Promise<void> {
  setProcessDpiAware();
  const cbz = tmpPath("issue-6018.cbz");
  writeFileSync(
    cbz,
    makeZip([
      // 400x51 so fit-width zoom * height is not an integer (the 1px
      // layout-vs-tile seam only shows then)
      { name: "001.png", data: makePng(400, 51, [200, 40, 40]) },
      { name: "002.png", data: makePng(400, 51, [40, 80, 180]) },
    ]),
  );

  const zeroDir = writeAppData(tmpPath("issue-6018-zero"), "0 0");
  const {
    proc: p0,
    client: c0,
    frame: f0,
  } = await launchControlled(["-appdata", zeroDir, cbz], {
    defaultWindowPos: true,
  });
  try {
    await c0.waitForRenderIdle();
    sendCommandSync(f0, cmdId("CmdZoomFitWidthAndContinuous"));
    await c0.waitForRenderIdle();
    const z = await layoutPages(c0);
    if (z.spacingDy !== 0) {
      throw new Error(`issue-6018: ComicBookUI.PageSpacing = 0 0 left model spacing.dy=${z.spacingDy}\n${z.raw}`);
    }
    const gap0 = gapBetween(z.pages, 1, 2);
    if (gap0 !== 0) {
      throw new Error(`issue-6018: expected no gap between CBZ pages, got ${gap0}\n${z.raw}`);
    }

    await c0.setNotificationsEnabled(false);
    const canvas = findCanvas(f0);
    if (!canvas) {
      throw new Error("issue-6018: canvas not found");
    }
    const p1 = z.pages.find((p) => p.n === 1)!;
    const x = p1.sx + Math.floor(p1.sdx / 2);
    const y = p1.sy + p1.sdy - 2;
    const col = readWindowDCColumn(canvas, x, y, 5);
    const isBlack = (c: number) => c === 0;
    if (col.slice(1, 4).some(isBlack)) {
      throw new Error(
        `issue-6018: 1px canvas seam between pages at y=${y}: ${col.map((c) => c.toString(16)).join(",")}\n${z.raw}`,
      );
    }

    sendCommandSync(f0, cmdId("CmdAdvancedSettings"));
    const deadline = Date.now() + 8000;
    let names = "";
    while (Date.now() < deadline) {
      const res = await c0.request(ControlCommand.TestAdvSettingsRows, ["names"]);
      names = String(res[1] ?? "");
      if (names.includes("ComicBookUI.PageSpacing") && names.includes("FixedPageUI.PageSpacing")) {
        break;
      }
      await sleep(50);
    }
    if (!names.includes("ComicBookUI.PageSpacing")) {
      throw new Error(`issue-6018: Advanced Settings is missing ComicBookUI.PageSpacing:\n${names}`);
    }
    if (!names.includes("FixedPageUI.PageSpacing")) {
      throw new Error(`issue-6018: Advanced Settings is missing FixedPageUI.PageSpacing:\n${names}`);
    }
  } finally {
    c0.close();
    await killAndWait(p0);
  }

  const gapDir = writeAppData(tmpPath("issue-6018-gap"), "0 40");
  const {
    proc: p1,
    client: c1,
    frame: f1,
  } = await launchControlled(["-appdata", gapDir, cbz], {
    defaultWindowPos: true,
  });
  try {
    await c1.waitForRenderIdle();
    sendCommandSync(f1, cmdId("CmdZoomFitWidthAndContinuous"));
    await c1.waitForRenderIdle();
    const g = await layoutPages(c1);
    if (g.spacingDy < 20) {
      throw new Error(`issue-6018: ComicBookUI.PageSpacing = 0 40 left model spacing.dy=${g.spacingDy}\n${g.raw}`);
    }
    const gap = gapBetween(g.pages, 1, 2);
    if (Math.abs(gap - g.spacingDy) > 2) {
      throw new Error(`issue-6018: page gap ${gap} != spacing.dy ${g.spacingDy}\n${g.raw}`);
    }
  } finally {
    c1.close();
    await killAndWait(p1);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
