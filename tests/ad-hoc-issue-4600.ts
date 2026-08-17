// Ad-hoc probe for issue #4600 ("You used to be able to change font, but now
// you can't?").
//
// EBookUI.FontName (added for #3138) is applied as a user-CSS font-family with
// !important. This measures *when* that override actually reaches the text: it
// builds EPUBs that differ only in how the publisher declares font-family,
// renders each with FontName empty vs set, and counts differing pixels.
//
// Before the #4600 fix the override lost to a publisher !important rule, to an
// inline style attribute and to a class on a <span> (0.00% changed in those
// three rows). It now wins in all of them: the selector list covers inline
// elements, and user-stylesheet !important outranks inline styles in mupdf's
// cascade. Every row must be non-zero, with and without IgnoreDocumentCSS.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { deflateRawSync } from "node:zlib";
import { join } from "node:path";
import { tmpPath } from "./util.ts";
import { findCanvas, killAndWait, killProcessesNamed, launchSumatra, waitForFrame } from "./win-automation.ts";
import { captureWindowPixels, moveWindow, setForegroundWindow, showWindow, sleep, SW_RESTORE } from "./winapi.ts";

const EPUB_DIR = tmpPath("epub-font");

// --- minimal zip writer (an EPUB is a zip) --------------------------------

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    }
    t[i] = c >>> 0;
  }
  return t;
})();

function crc32(buf: Uint8Array): number {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) {
    c = CRC_TABLE[(c ^ buf[i]!) & 0xff]! ^ (c >>> 8);
  }
  return (c ^ 0xffffffff) >>> 0;
}

type Entry = { name: string; data: Uint8Array; store: boolean };

function zip(entries: Entry[]): Uint8Array {
  const chunks: Uint8Array[] = [];
  const central: Uint8Array[] = [];
  let offset = 0;
  for (const e of entries) {
    const name = new TextEncoder().encode(e.name);
    const body = e.store ? e.data : new Uint8Array(deflateRawSync(e.data));
    const crc = crc32(e.data);
    const local = new DataView(new ArrayBuffer(30));
    local.setUint32(0, 0x04034b50, true);
    local.setUint16(4, 20, true);
    local.setUint16(8, e.store ? 0 : 8, true);
    local.setUint32(14, crc, true);
    local.setUint32(18, body.length, true);
    local.setUint32(22, e.data.length, true);
    local.setUint16(26, name.length, true);
    chunks.push(new Uint8Array(local.buffer), name, body);

    const cd = new DataView(new ArrayBuffer(46));
    cd.setUint32(0, 0x02014b50, true);
    cd.setUint16(4, 20, true);
    cd.setUint16(6, 20, true);
    cd.setUint16(10, e.store ? 0 : 8, true);
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
  const out = new Uint8Array(total);
  let p = 0;
  for (const c of all) {
    out.set(c, p);
    p += c.length;
  }
  return out;
}

// --- fixtures --------------------------------------------------------------

const CONTAINER =
  `<?xml version="1.0"?>\n<container version="1.0" ` +
  `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
  `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>`;

const OPF =
  `<?xml version="1.0" encoding="utf-8"?>\n<package xmlns="http://www.idpf.org/2007/opf" version="3.0" ` +
  `unique-identifier="id"><metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
  `<dc:identifier id="id">urn:uuid:font-test</dc:identifier><dc:title>font test</dc:title>` +
  `<dc:language>en</dc:language></metadata><manifest>` +
  `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
  `<item id="css" href="s.css" media-type="text/css"/></manifest><spine><itemref idref="c1"/></spine></package>`;

const EN = "The quick brown fox jumps over the lazy dog. ".repeat(12);
const CN = "中文排版测试默认字体".repeat(12);
const SERIF = "'Times New Roman', serif";

// [css, body] -- each fixture declares font-family a different way
const VARIANTS: Record<string, [string, string]> = {
  none: ["", `<p>${EN}</p><p>${EN}</p><p>${EN}</p>`],
  cls: [`p.t { font-family: ${SERIF}; }`, `<p class="t">${EN}</p>`.repeat(3)],
  clsimp: [`p.t { font-family: ${SERIF} !important; }`, `<p class="t">${EN}</p>`.repeat(3)],
  inline: ["", `<p style="font-family: ${SERIF};">${EN}</p>`.repeat(3)],
  span: [`.t { font-family: ${SERIF}; }`, `<p><span class="t">${EN}</span></p>`.repeat(3)],
  cjk: ["", `<p>${CN}</p>`.repeat(3)],
};

export function makeFixtures(): void {
  mkdirSync(EPUB_DIR, { recursive: true });
  const enc = new TextEncoder();
  for (const [name, [css, body]] of Object.entries(VARIANTS)) {
    const html =
      `<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml"><head><title>t</title>` +
      `<link rel="stylesheet" type="text/css" href="s.css"/></head><body>${body}</body></html>`;
    const data = zip([
      { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
      { name: "META-INF/container.xml", data: enc.encode(CONTAINER), store: false },
      { name: "OEBPS/content.opf", data: enc.encode(OPF), store: false },
      { name: "OEBPS/s.css", data: enc.encode(css), store: false },
      { name: "OEBPS/c1.xhtml", data: enc.encode(html), store: false },
    ]);
    writeFileSync(join(EPUB_DIR, `${name}.epub`), data);
  }
}

// --- rendering -------------------------------------------------------------

type Cfg = { fontName: string; ignoreCss: boolean };

async function render(epub: string, cfg: Cfg): Promise<Uint8Array> {
  const appdata = tmpPath("issue-4600-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      `EBookUI [`,
      `\tFontName = ${cfg.fontName}`,
      `\tIgnoreDocumentCSS = ${cfg.ignoreCss}`,
      `]`,
      `RestoreSession = false`,
      `ShowStartPage = false`,
      `ShowToc = false`,
      ``,
    ].join("\n"),
  );
  const proc = launchSumatra(["-appdata", appdata, "-view", "single page", "-zoom", "fit page", epub]);
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!frame) {
      throw new Error(`no frame for ${epub}`);
    }
    showWindow(frame, SW_RESTORE);
    moveWindow(frame, 40, 40, 1000, 800);
    setForegroundWindow(frame);
    await sleep(2500);
    const cap = captureWindowPixels(findCanvas(frame));
    if (!cap) {
      throw new Error(`no capture for ${epub}`);
    }
    return cap.data;
  } finally {
    await killAndWait(proc);
  }
}

function diffPct(a: Uint8Array, b: Uint8Array): number {
  const n = Math.min(a.length, b.length) / 4;
  let d = 0;
  for (let i = 0; i < n; i++) {
    const o = i * 4;
    if (Math.abs(a[o]! + a[o + 1]! + a[o + 2]! - (b[o]! + b[o + 1]! + b[o + 2]!)) > 40) {
      d++;
    }
  }
  return (d * 100) / n;
}

export async function testit(): Promise<void> {
  makeFixtures();
  await killProcessesNamed("SumatraPDF.exe");
  for (const v of Object.keys(VARIANTS)) {
    const epub = join(EPUB_DIR, `${v}.epub`);
    const font = v === "cjk" ? "SimSun" : "Arial";
    const base = await render(epub, { fontName: "", ignoreCss: false });
    const set = await render(epub, { fontName: font, ignoreCss: false });
    const ign = await render(epub, { fontName: font, ignoreCss: true });
    console.log(
      `  ${v.padEnd(7)} FontName='${font}': ${diffPct(base, set).toFixed(2)}% changed` +
        ` | +IgnoreDocumentCSS: ${diffPct(base, ign).toFixed(2)}%`,
    );
  }
}

if (import.meta.main) {
  const { runStandalone } = await import("./util.ts");
  await runStandalone(testit);
}
