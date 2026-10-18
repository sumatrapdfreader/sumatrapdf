// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1293
//
// A fixed-layout EPUB chapter (<meta name="viewport">) is one page the size
// of the viewport. mupdf applied the default @page margin, so content filling
// the viewport spilled onto a blank extra page and sat off the page edges.
//
// InDesign's credits pages draw text at 20x size inside a box shrunk with
// transform: scale(0.05), each word a <span> with position:absolute. mupdf
// had no CSS transform and ignored the inline positioning.
//
// Run: bun tests/issue-1293.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const W = 600;
const H = 800;

// a box 10x the page, shrunk onto it from its top left corner
function scaled(content: string): string {
  return (
    `<div style="position:absolute;top:0;left:0;width:${W * 10}px;height:${H * 10}px;` +
    `transform-origin:0% 0%;transform:rotate(0deg) scale(0.1)">${content}</div>`
  );
}

const pages = [
  // fills the viewport
  `<div style="width:${W}px;height:${H}px;background-color:#f00"><p style="margin:0">PAGEONE</p></div>`,
  `<p>PAGETWO</p>`,
  // a 1000px block at 3000,4000 lands at 300,400 as 100x100
  scaled(
    `<div style="position:absolute;top:4000px;left:3000px;width:1000px;height:1000px;background-color:#f00"></div>`,
  ),
  // a positioned word at 1000,2000 lands at 100,200
  scaled(
    `<p style="margin:0;white-space:nowrap"><span style="color:#fff">pad </span>` +
      `<span style="position:absolute;top:2000px;left:1000px;font-size:400px;line-height:1;color:#00f">HH</span></p>`,
  ),
];

function xhtml(n: number, body: string): string {
  return (
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>p${n}</title>` +
    `<meta name="viewport" content="width=${W},height=${H}"/></head>` +
    `<body style="width:${W}px;height:${H}px;margin:0">${body}</body></html>`
  );
}

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const items = pages.map((_, i) => `<item id="c${i}" href="c${i}.xhtml" media-type="application/xhtml+xml"/>`);
  const refs = pages.map((_, i) => `<itemref idref="c${i}"/>`);
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">issue-1293</dc:identifier>` +
    `<dc:title>Fixed layout</dc:title><dc:language>en</dc:language>` +
    `<meta property="rendition:layout">pre-paginated</meta></metadata>` +
    `<manifest>${items.join("")}</manifest><spine>${refs.join("")}</spine></package>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    ...pages.map((body, i) => ({ name: `OEBPS/c${i}.xhtml`, data: enc.encode(xhtml(i, body)) })),
  ];
  return makeZip(entries);
}

type Render = { box: number[]; pages: number; red: number; blue: number; raw: string };

async function render(client: ControlClient, epub: string, pageNo: number): Promise<Render> {
  const res = await client.request(ControlCommand.TestRenderPageColors, [epub, pageNo, 100]);
  const raw = String(res[1] ?? "").trim();
  console.log(`issue-1293: page ${pageNo}: ${raw}`);
  if (res[0] !== 0) {
    throw new Error(`issue-1293: page ${pageNo}: render failed: ${raw}`);
  }
  return {
    box: (/box=([\d,]+)/.exec(raw)?.[1] ?? "").split(",").map(Number),
    pages: Number(/pages=(\d+)/.exec(raw)?.[1] ?? 0),
    red: Number(/red=(\d+)/.exec(raw)?.[1] ?? 0),
    blue: Number(/blue=(\d+)/.exec(raw)?.[1] ?? 0),
    raw,
  };
}

function near(box: number[], want: number[], slack: number): boolean {
  return want.every((v, i) => Math.abs(box[i]! - v) <= slack);
}

export async function testit(): Promise<void> {
  const epub = tmpPath("issue-1293.epub");
  writeFileSync(epub, makeEpub());

  await withControlledSumatra(EXE, async (client) => {
    // the viewport-sized box fills the page: no @page margin around it, and
    // no spill onto a blank page (pages counts laid-out chapters: 1 + 3)
    const p1 = await render(client, epub, 1);
    if (!near(p1.box, [0, 0, W, H], 0)) {
      throw new Error(`issue-1293: the viewport-sized box does not fill the page: ${p1.raw}`);
    }
    if (p1.pages !== pages.length) {
      throw new Error(`issue-1293: a fixed-layout chapter took more than one page: ${p1.raw}`);
    }

    const p3 = await render(client, epub, 3);
    if (!near(p3.box, [300, 400, 400, 500], 2) || p3.red < 9000) {
      throw new Error(`issue-1293: transform: scale() block misplaced: ${p3.raw}`);
    }

    // "HH" at 40px: ~58px wide, cap height ~27px from about 200 + 9
    const p4 = await render(client, epub, 4);
    const [x0, y0, x1, y1] = p4.box;
    const ok = p4.blue > 100 && Math.abs(x0! - 100) <= 4 && y0! >= 200 && y0! <= 215 && x1! < 170 && y1! < 245;
    if (!ok) {
      throw new Error(`issue-1293: absolutely positioned span misplaced: ${p4.raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
