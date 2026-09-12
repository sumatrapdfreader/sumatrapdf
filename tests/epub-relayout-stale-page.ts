// Tries to reproduce crash 2026-09-12-09-59-1328: a debug report
// `!pageInfo` in DisplayModel::CvtToScreen (DisplayModel.cpp:1961) after an
// EPUB restyle.
//
// Shape of the crash, from its log.txt: the reader had scrolled deep into a
// chaptered EPUB, so chapters 28..34 had laid out progressively and the flat
// page count was past 95. A theme toggle then ran ApplyReflowThemeCss, which
// resets the chapter table ("chapters reset, chapter 1 -> 1 pages"); chapters
// 33..36 re-laid out one at a time while something still held flat page 95
// and asked for its screen coordinates. GetPageInfo(95) returned null because
// DisplayModel had already rebuilt to the collapsed page count.
//
// SyncWithEngineLayout() does call cb->PagesRenumbered(), so some holder of a
// page number (selection, link, annotation, find result, ...) is not
// remapping. The dump is from build 22229, which predates c8bfbb630, so it
// carries no callstack and does not say which one. This test drives the
// sequence and lets the harness catch the ReportIf (exit code 105).
//
// The whole scenario runs once per EBookUI layout in LAYOUTS: a reflow
// document's pagination is fixed by font size and margins at load time, so
// those are the knob that decides how many flat pages a chapter is worth and
// how far the count collapses when the chapter table resets. Small font / no
// margin packs the most text per page (fewest pages), big font / big margin
// the least (most pages).
//
// The EPUB is generated here rather than taken from tests/: the repo's only
// multi-chapter EPUB (issue-6095.epub) has 3 spine items and never gets near
// a flat page 95.
//
// Run: bun tests/epub-relayout-stale-page.ts [--no-build]

import { deflateRawSync } from "node:zlib";
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, withControlledSumatra } from "./control.ts";
import { cmdId, EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, writeAppdata } from "./util.ts";
import { sleep } from "./winapi.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";

// the crash doc had 36+ chapters and reached flat page 95
const CHAPTER_COUNT = 40;
const PARAS_PER_CHAPTER = 40;
// stop short of the last chapter, like the reader who was at chapter 34 of 36+
const DEEP_CHAPTER = 34;
const THEME_TOGGLES = 3;
// the flat page number the crash was holding
const CRASH_PAGE_NO = 95;

// EBookUI.FontSize is in points (0 = the 8.0 default), EBookUI.Margin in
// points too (empty = the default 3em/2em, "0" = no margin at all)
type Layout = { name: string; fontSize: number; margin: string };

const LAYOUTS: Layout[] = [
  { name: "small-font-no-margin", fontSize: 5, margin: "0" },
  { name: "default", fontSize: 0, margin: "" },
  { name: "big-font-big-margin", fontSize: 18, margin: "60" },
];

type ZipEntry = { name: string; data: Uint8Array; store?: boolean };

let crcTable: Uint32Array | undefined;

function crc32(data: Uint8Array): number {
  if (!crcTable) {
    crcTable = new Uint32Array(256);
    for (let i = 0; i < 256; i++) {
      let c = i;
      for (let k = 0; k < 8; k++) {
        c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
      }
      crcTable[i] = c >>> 0;
    }
  }
  let crc = 0xffffffff;
  for (const b of data) {
    crc = crcTable[(crc ^ b) & 0xff]! ^ (crc >>> 8);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

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

// each chapter must be several pages so the flat page count grows well past
// the per-chapter placeholder count a restyle collapses it to
function chapterHtml(n: number): string {
  const paras: string[] = [];
  for (let i = 1; i <= PARAS_PER_CHAPTER; i++) {
    const words: string[] = [];
    for (let w = 0; w < 60; w++) {
      words.push(`ch${n}p${i}w${w}`);
    }
    paras.push(`<p>${words.join(" ")}</p>`);
  }
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Chapter ${n}</title></head>` +
    `<body><h1>Chapter ${n}</h1>${paras.join("")}</body></html>`
  );
}

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;

  const items: string[] = [];
  const refs: string[] = [];
  const navLis: string[] = [];
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
  ];
  for (let n = 1; n <= CHAPTER_COUNT; n++) {
    items.push(`<item id="c${n}" href="c${n}.xhtml" media-type="application/xhtml+xml"/>`);
    refs.push(`<itemref idref="c${n}"/>`);
    navLis.push(`<li><a href="c${n}.xhtml">Chapter ${n}</a></li>`);
    entries.push({ name: `OEBPS/c${n}.xhtml`, data: enc.encode(chapterHtml(n)) });
  }

  const nav =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">` +
    `<head><title>toc</title></head><body><nav epub:type="toc"><ol>${navLis.join("")}</ol></nav></body></html>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">epub-relayout-stale-page</dc:identifier>` +
    `<dc:title>Relayout stale page</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest><item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>` +
    `${items.join("")}</manifest><spine>${refs.join("")}</spine></package>`;

  entries.push({ name: "OEBPS/nav.xhtml", data: enc.encode(nav) });
  entries.push({ name: "OEBPS/content.opf", data: enc.encode(opf) });
  return zip(entries);
}

// DocumentColorsFollowTheme must be on: with the default `off` the page colors
// are black-on-white whatever the theme, so UpdateDocumentColors()
// early-returns and only the very first toggle of a process restyles
function settingsFor(l: Layout): string {
  const ebook = ["EBookUI [", `\tFontSize = ${l.fontSize}`, `\tMargin = ${l.margin}`, "]"];
  return [
    "UiLanguage = en",
    "RestoreSession = false",
    "ShowStartPage = false",
    "CheckForUpdates = false",
    "DocumentColorsFollowTheme = smart",
    "Theme = Light",
    ...ebook,
  ].join("\n");
}

// walk chapter by chapter so chapters lay out progressively, the way scrolling
// forward does -- a single jump to the last chapter lays out only that one
async function walkToChapter(client: ControlClient, last: number): Promise<void> {
  for (let ch = 1; ch <= last; ch++) {
    await client.goToLocation(ch, 1);
  }
  await client.waitForRenderIdle(30000);
}

// returns the flat page count once DEEP_CHAPTER has laid out
async function runLayout(l: Layout, epub: string, epub2: string, log: string): Promise<number> {
  const appdata = writeAppdata(`epub-relayout-stale-page-${l.name}`, settingsFor(l));
  let deepPageCount = 0;
  let selParts = 0;

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      const start = await client.chapterInfo();
      if (!start.hasChapters || start.chapterCount < DEEP_CHAPTER) {
        throw new Error(
          `${l.name}: expected a chaptered doc with >= ${DEEP_CHAPTER} chapters, ` +
            `got hasChapters=${start.hasChapters} chapterCount=${start.chapterCount}`,
        );
      }

      await walkToChapter(client, DEEP_CHAPTER);

      const deep = await client.chapterInfo();
      deepPageCount = deep.pageCount;
      if (deep.pageCount < CRASH_PAGE_NO) {
        throw new Error(
          `${l.name}: flat page count only reached ${deep.pageCount}, ` +
            `the crash needs it past ${CRASH_PAGE_NO} -- make the chapters longer`,
        );
      }

      // A page-number holder for the restyle to leave behind. Note it cannot
      // be the holder the crash had: TextSelection only fills TextSel.quads
      // for rotated text, and this EPUB is axis-aligned, so every entry takes
      // SelectionOnPage::GetRect()'s null-checked path rather than the
      // unguarded quad branch. Seeded anyway -- it is what a reader has open
      // across a theme toggle, and nothing else about it is exercised.
      const sel = await client.seedTextSelection(deep.page);
      selParts = sel.parts;
      await sleep(200 * SLOW_BUILD_FACTOR);

      for (let i = 0; i < THEME_TOGGLES; i++) {
        // resets the chapter table: pageCount collapses to one placeholder
        // page per chapter while holders still have the old flat numbers
        sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
        await sleep(500 * SLOW_BUILD_FACTOR);

        // force the progressive re-layout the crash log shows after the reset
        await walkToChapter(client, DEEP_CHAPTER);
        sendCommandSync(frame, cmdId("CmdScrollDownPage"));
        await sleep(200 * SLOW_BUILD_FACTOR);
        await client.waitForRenderIdle(30000);
      }

      // Same thing with the selection in a *background* tab. The restyle
      // reaches every tab's DisplayModel (UpdateDocumentColors loops over all
      // of them), but ControllerCallbackHandler::PagesRenumbered() returns
      // early unless the dm is the current tab's, so RemapSelOnRenumber() /
      // RemapTextSelection() never run for the other one. Switching back then
      // paints a selection whose pageNo is from the pre-restyle numbering.
      sendCommandSync(frame, cmdId("CmdPrevTab"));
      await sleep(300 * SLOW_BUILD_FACTOR);
      await client.waitForRenderIdle(30000);
      sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
      await sleep(500 * SLOW_BUILD_FACTOR);
      await client.waitForRenderIdle(30000);

      // back to the tab holding the stale selection, and paint it before
      // anything re-lays-out the chapters it was numbered against
      sendCommandSync(frame, cmdId("CmdNextTab"));
      await sleep(300 * SLOW_BUILD_FACTOR);
      await client.waitForRenderIdle(30000);

      const after = await client.chapterInfo();
      if (after.pageCount < 1) {
        throw new Error(`${l.name}: page count collapsed to ${after.pageCount} after the restyles`);
      }
    },
    ["-appdata", appdata, "-log-to-file", log, "-window-pos", "1000x900@40x40", "-view", "continuous", epub, epub2],
  );

  // without the restyle the run proves nothing: fail loudly instead of
  // reporting green on a process that never reset the chapter table
  const logText = readFileSync(log, "utf8");
  const resets = logText.split("\n").filter((line) => line.includes("ApplyReflowThemeCss: chapters reset")).length;
  if (selParts < 1) {
    throw new Error(`${l.name}: seeded selection is empty, the run has no page-number holder at all`);
  }
  if (resets < THEME_TOGGLES + 1) {
    throw new Error(
      `${l.name}: expected at least ${THEME_TOGGLES + 1} chapter-table resets, saw ${resets} -- ` +
        `the theme toggle did not restyle the document`,
    );
  }
  return deepPageCount;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("epub-relayout-stale-page-data");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "chapters.epub");
  const bytes = makeEpub();
  writeFileSync(epub, bytes);
  // a second tab, like the two-tab window in the crash log
  const epub2 = join(dir, "chapters-2.epub");
  writeFileSync(epub2, bytes);

  const counts: number[] = [];
  for (const l of LAYOUTS) {
    const log = join(dir, `log-${l.name}.txt`);
    const n = await runLayout(l, epub, epub2, log);
    counts.push(n);
    console.log(`epub-relayout-stale-page: ${l.name}: ${n} pages at chapter ${DEEP_CHAPTER}`);
  }

  // a bigger font and bigger margins must fit less text per page. If the knob
  // stopped working every run would relayout to the same pagination and the
  // test would silently cover only one case
  for (let i = 1; i < counts.length; i++) {
    if (counts[i]! <= counts[i - 1]!) {
      throw new Error(
        `epub-relayout-stale-page: page counts ${counts.join(", ")} are not increasing across ` +
          `${LAYOUTS.map((l) => l.name).join(", ")} -- EBookUI FontSize/Margin no longer changes pagination`,
      );
    }
  }

  console.log(`epub-relayout-stale-page: OK (pages: ${counts.join(", ")})`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
