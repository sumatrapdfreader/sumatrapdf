// #6018: ComicBookUI.PageSpacing must change the gap between CBZ pages in
// continuous view, PageSpacing must appear in Advanced Settings, and 0 0 must
// not leave a canvas seam. Layout size must match EngineImages/tile Round, and
// the GDI+ bicubic path used for WebP must not darken page edges toward the
// black comic canvas.
//
// CBZ files read ComicBookUI.PageSpacing, not FixedPageUI.PageSpacing.
//
// Run: bun tests/issue-6018.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
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

// Lossless 400x51 solid-color WebP (VP8L). WebP uses EngineImages' GDI+ scale
// path (mupdf is not used), which is what left a 1-2px dark seam at
// PageSpacing 0 0. 400x51 so fit-width zoom * height is not an integer.
const kWebpRed400x51 = Buffer.from([
  82, 73, 70, 70, 36, 0, 0, 0, 87, 69, 66, 80, 86, 80, 56, 76, 23, 0, 0, 0, 47, 143, 129, 12, 0, 7, 80, 148, 34, 23,
  165, 255, 1, 0, 69, 250, 255, 95, 34, 250, 159, 210, 7, 0,
]);
const kWebpBlue400x51 = Buffer.from([
  82, 73, 70, 70, 36, 0, 0, 0, 87, 69, 66, 80, 86, 80, 56, 76, 23, 0, 0, 0, 47, 143, 129, 12, 0, 7, 80, 168, 162, 148,
  182, 255, 1, 0, 69, 250, 255, 95, 34, 250, 159, 210, 7, 0,
]);
const kPage1Color = 0x002828c8; // RGB(200,40,40) as COLORREF
const kPage2Color = 0x00b45028; // RGB(40,80,180) as COLORREF

function colorDist(c: number, want: number): number {
  return (
    Math.abs((c & 0xff) - (want & 0xff)) +
    Math.abs(((c >> 8) & 0xff) - ((want >> 8) & 0xff)) +
    Math.abs(((c >> 16) & 0xff) - ((want >> 16) & 0xff))
  );
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
      { name: "001.webp", data: kWebpRed400x51 },
      { name: "002.webp", data: kWebpBlue400x51 },
    ]),
  );

  const zeroDir = writeAppData(tmpPath("issue-6018-zero"), "0 0");
  const { proc: p0, client: c0, frame: f0 } = await launchControlled(["-appdata", zeroDir, cbz]);
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
    const hex = col.map((c) => c.toString(16)).join(",");
    const isBlack = (c: number) => c === 0;
    if (col.slice(1, 4).some(isBlack)) {
      throw new Error(`issue-6018: 1px canvas seam between pages at y=${y}: ${hex}\n${z.raw}`);
    }
    // last pixel of page 1 / first of page 2: GDI+ bicubic without wrap-mode
    // darkens these toward the black canvas (issue #6018 follow-up)
    if (colorDist(col[1]!, kPage1Color) > 80) {
      throw new Error(`issue-6018: page 1 edge darkened at y=${y + 1}: ${hex}\n${z.raw}`);
    }
    if (colorDist(col[2]!, kPage2Color) > 80) {
      throw new Error(`issue-6018: page 2 edge darkened at y=${y + 2}: ${hex}\n${z.raw}`);
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
  const { proc: p1, client: c1, frame: f1 } = await launchControlled(["-appdata", gapDir, cbz]);
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
