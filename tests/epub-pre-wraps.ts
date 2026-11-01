// Long lines of <pre> must wrap: pages can't scroll sideways, so text past the
// edge was lost. A book made from a text file is one big <pre>.
//
// Run: bun tests/epub-pre-wraps.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const name = "epub-pre-wraps";
const kLines = 4;
const chapters = [
  `<pre>${"word ".repeat(100)}</pre>`,
  `<pre>${Array(kLines).fill("word").join("\n")}</pre>`,
  `<pre style="white-space:nowrap">${"word ".repeat(100)}</pre>`,
];

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const items = chapters.map((_, i) => `<item id="c${i}" href="c${i}.xhtml" media-type="application/xhtml+xml"/>`);
  const refs = chapters.map((_, i) => `<itemref idref="c${i}"/>`);
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">${name}</dc:identifier>` +
    `<dc:title>${name}</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>${items.join("")}</manifest><spine>${refs.join("")}</spine></package>`;
  const xhtml = (body: string) =>
    `<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml">` +
    `<head><title>t</title></head><body>${body}</body></html>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    ...chapters.map((body, i) => ({ name: `OEBPS/c${i}.xhtml`, data: enc.encode(xhtml(body)) })),
  ];
  return makeZip(entries);
}

type Render = { dx: number; right: number; textDy: number; raw: string };

// Each render loads the file anew, with chapter layout deferred: page N is the first page of chapter N.
async function renderChapter(client: ControlClient, epub: string, chapterNo: number): Promise<Render> {
  const res = await client.request(ControlCommand.TestRenderPageColors, [epub, chapterNo, 100]);
  const raw = String(res[1] ?? "").trim();
  console.log(`${name}: chapter ${chapterNo}: ${raw}`);
  if (res[0] !== 0) {
    throw new Error(`${name}: chapter ${chapterNo}: render failed: ${raw}`);
  }
  const box = (/box=([\d,]+)/.exec(raw)?.[1] ?? "").split(",").map(Number);
  return {
    dx: Number(/size=(\d+)x/.exec(raw)?.[1] ?? 0),
    right: box[2] ?? 0,
    textDy: (box[3] ?? 0) - (box[1] ?? 0),
    raw,
  };
}

export async function testit(): Promise<void> {
  const epub = tmpPath(`${name}.epub`);
  writeFileSync(epub, makeEpub());

  await withControlledSumatra(EXE, async (client) => {
    const long = await renderChapter(client, epub, 1);
    if (long.right >= long.dx) {
      throw new Error(`${name}: a long line runs off the page: ${long.raw}`);
    }

    // newlines still break lines: taller than kLines - 1 lines of the wrapped text's line height
    const lines = await renderChapter(client, epub, 2);
    if (lines.textDy < (kLines - 1) * 10) {
      throw new Error(`${name}: newlines in pre don't break lines: ${lines.raw}`);
    }

    // an explicit nowrap is kept
    const nowrap = await renderChapter(client, epub, 3);
    if (nowrap.right < nowrap.dx) {
      throw new Error(`${name}: white-space:nowrap wrapped: ${nowrap.raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
