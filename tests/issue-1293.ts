// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1293
//
// A fixed-layout EPUB chapter (<meta name="viewport">) is one page the size
// of the viewport. mupdf applied the default @page margin, so content filling
// the viewport spilled onto a blank extra page and sat off the page edges.
//
// Run: bun tests/issue-1293.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const W = 600;
const H = 800;

function xhtml(title: string): string {
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>${title}</title>` +
    `<meta name="viewport" content="width=${W},height=${H}"/></head>` +
    `<body style="width:${W}px;height:${H}px;margin:0">` +
    `<div style="width:${W}px;height:${H}px;background-color:#f00"><p style="margin:0">${title}</p></div>` +
    `</body></html>`
  );
}

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">issue-1293</dc:identifier>` +
    `<dc:title>Fixed layout</dc:title><dc:language>en</dc:language>` +
    `<meta property="rendition:layout">pre-paginated</meta></metadata>` +
    `<manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="c2" href="c2.xhtml" media-type="application/xhtml+xml"/>` +
    `</manifest><spine><itemref idref="c1"/><itemref idref="c2"/></spine></package>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(xhtml("PAGEONE")) },
    { name: "OEBPS/c2.xhtml", data: enc.encode(xhtml("PAGETWO")) },
  ];
  return makeZip(entries);
}

export async function testit(): Promise<void> {
  const epub = tmpPath("issue-1293.epub");
  writeFileSync(epub, makeEpub());

  // the red viewport-sized box fills the page: no @page margin around it, and
  // no spill onto a blank second page (pages counts laid-out chapters: 1 + 1)
  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestRenderPageColors, [epub, 1, 100]);
    const raw = String(res[1] ?? "").trim();
    console.log(`issue-1293: ${raw}`);
    if (res[0] !== 0) {
      throw new Error(`issue-1293: render failed: ${raw}`);
    }
    const box = (/box=([\d,]+)/.exec(raw)?.[1] ?? "").split(",").map(Number);
    const pages = Number(/pages=(\d+)/.exec(raw)?.[1] ?? 0);
    if (box[0] !== 0 || box[1] !== 0 || box[2] !== W || box[3] !== H) {
      throw new Error(`issue-1293: the viewport-sized box does not fill the page: ${raw}`);
    }
    if (pages !== 2) {
      throw new Error(`issue-1293: a fixed-layout chapter took more than one page: ${raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
