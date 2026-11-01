// A chapter starting with a page-tall image must not leave a blank first page.
// The image is scaled to the page height, the body margin above it left too
// little room, and mupdf moved it to the next page.
//
// Run: bun tests/epub-first-page-image.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, makePng, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const name = "epub-first-page-image";
const img = `<img src="tall.png"/>`;
const chapters = [
  `${img}<p style="color:#00f">after</p>`,
  `<p><a id="a"></a> ${img}</p>`,
  `<p style="color:#00f">before</p>${img}`,
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
    `<manifest>${items.join("")}<item id="i" href="tall.png" media-type="image/png"/></manifest>` +
    `<spine>${refs.join("")}</spine></package>`;
  const xhtml = (body: string) =>
    `<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml">` +
    `<head><title>t</title></head><body>${body}</body></html>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/tall.png", data: makePng(600, 900, [255, 0, 0]) },
    ...chapters.map((body, i) => ({ name: `OEBPS/c${i}.xhtml`, data: enc.encode(xhtml(body)) })),
  ];
  return makeZip(entries);
}

type Render = { red: number; blue: number; pageDy: number; boxBottom: number; raw: string };

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
    red: Number(/red=(\d+)/.exec(raw)?.[1] ?? 0),
    blue: Number(/blue=(\d+)/.exec(raw)?.[1] ?? 0),
    pageDy: Number(/size=\d+x(\d+)/.exec(raw)?.[1] ?? 0),
    boxBottom: box[3] ?? 0,
    raw,
  };
}

export async function testit(): Promise<void> {
  const epub = tmpPath(`${name}.epub`);
  writeFileSync(epub, makeEpub());

  await withControlledSumatra(EXE, async (client) => {
    const c1 = await renderChapter(client, epub, 1);
    if (c1.red === 0) {
      throw new Error(`${name}: first page is blank: ${c1.raw}`);
    }
    if (c1.boxBottom >= c1.pageDy) {
      throw new Error(`${name}: the image runs off the first page: ${c1.raw}`);
    }

    // only an anchor and a space before the image, in a paragraph with its own margin
    const c2 = await renderChapter(client, epub, 2);
    if (c2.red === 0) {
      throw new Error(`${name}: blank page before an image in a paragraph: ${c2.raw}`);
    }

    // text above it: the page isn't empty, so the image still moves on
    const c3 = await renderChapter(client, epub, 3);
    if (c3.blue === 0 || c3.red !== 0) {
      throw new Error(`${name}: expected only the text on the page: ${c3.raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
