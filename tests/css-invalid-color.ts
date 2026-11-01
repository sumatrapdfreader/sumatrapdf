// An invalid CSS color must be ignored: mupdf parsed any unknown name as hex, so
// background-color:whi painted black behind black text.
//
// Run: bun tests/css-invalid-color.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const name = "css-invalid-color";
const chapters = [
  `<p style="background-color:whi">some text</p>`,
  `<p style="background-color:ff0000">a color without the #</p>`,
  `<p bgcolor="whi">some text</p>`,
];
// more than a line of text, less than a line-wide box behind it
const kMaxTextPixels = 1500;

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

type Render = { red: number; nonwhite: number; raw: string };

// Each render loads the file anew, with chapter layout deferred: page N is the first page of chapter N.
async function renderChapter(client: ControlClient, epub: string, chapterNo: number): Promise<Render> {
  const res = await client.request(ControlCommand.TestRenderPageColors, [epub, chapterNo, 100]);
  const raw = String(res[1] ?? "").trim();
  console.log(`${name}: chapter ${chapterNo}: ${raw}`);
  if (res[0] !== 0) {
    throw new Error(`${name}: chapter ${chapterNo}: render failed: ${raw}`);
  }
  return {
    red: Number(/red=(\d+)/.exec(raw)?.[1] ?? 0),
    nonwhite: Number(/nonwhite=(\d+)/.exec(raw)?.[1] ?? 0),
    raw,
  };
}

export async function testit(): Promise<void> {
  const epub = tmpPath(`${name}.epub`);
  writeFileSync(epub, makeEpub());

  await withControlledSumatra(EXE, async (client) => {
    for (const chapterNo of [1, 3]) {
      const c = await renderChapter(client, epub, chapterNo);
      if (c.nonwhite === 0 || c.nonwhite > kMaxTextPixels) {
        throw new Error(`${name}: chapter ${chapterNo}: invalid color painted a background: ${c.raw}`);
      }
    }
    const c2 = await renderChapter(client, epub, 2);
    if (c2.red < kMaxTextPixels) {
      throw new Error(`${name}: a hex color without the # stopped working: ${c2.raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
