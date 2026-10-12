// Percentage SVG dimensions must fill the EPUB's page viewport (#6324).

import { readFileSync, writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, makeZip, runStandalone, tmpPath } from "./util.ts";

const VIEW_W = 1419;
const VIEW_H = 1920;

function makeEpub(percent: number, css: string): Buffer {
  const html = `<html xmlns="http://www.w3.org/1999/xhtml"><head>
<meta name="viewport" content="width=${VIEW_W}, height=${VIEW_H}"/>
<style>html,body{margin:0;padding:0;font-size:0}svg{margin:0;padding:0}</style>
</head><body><div><svg xmlns="http://www.w3.org/2000/svg"
width="${percent}%" height="${percent}%" style="${css}" viewBox="0 0 ${VIEW_W} ${VIEW_H}">
<image width="${VIEW_W}" height="${VIEW_H}" href="red.jpg"/>
</svg></div></body></html>`;
  const container = `<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>`;
  const opf = `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:identifier id="id">issue-6324</dc:identifier>
<dc:title>Percentage SVG</dc:title><dc:language>en</dc:language>
<meta property="rendition:layout">pre-paginated</meta></metadata>
<manifest><item id="page" href="page.xhtml" media-type="application/xhtml+xml"/>
<item id="image" href="red.jpg" media-type="image/jpeg"/></manifest>
<spine><itemref idref="page"/></spine></package>`;
  return makeZip([
    { name: "mimetype", data: Buffer.from("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: Buffer.from(container) },
    { name: "content.opf", data: Buffer.from(opf) },
    { name: "page.xhtml", data: Buffer.from(html) },
    { name: "red.jpg", data: readFileSync(`${import.meta.dir}/issue-6236.jpg`) },
  ]);
}

export async function testit(): Promise<void> {
  await withControlledSumatra(EXE, async (client) => {
    const cases = [
      { percent: 100, css: "", expected: 1 },
      { percent: 50, css: "", expected: 0.25 },
      { percent: 100, css: "width:50%;height:50%", expected: 0.25 },
    ];
    for (const { percent, css, expected } of cases) {
      const path = tmpPath(`issue-6324-${percent}-${css ? "css" : "attributes"}.epub`);
      writeFileSync(path, makeEpub(percent, css));
      const res = await client.request(ControlCommand.TestRenderPageColors, [path]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-6324: render failed: ${raw.trim()}`);
      }
      const size = /size=(\d+)x(\d+)/.exec(raw);
      const area = Number(size?.[1]) * Number(size?.[2]);
      const red = Number(/red=(\d+)/.exec(raw)?.[1] ?? 0);
      const coverage = red / area;
      console.log(`issue-6324 ${percent}% ${css}: coverage=${coverage.toFixed(3)} ${raw.trim()}`);
      // Page margins and SVG aspect fitting leave a narrow white border.
      if (!(coverage > expected * 0.85 && coverage < expected * 1.05)) {
        throw new Error(`issue-6324: expected image coverage ${expected}, got ${coverage}`);
      }
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
