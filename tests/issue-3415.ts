// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/3415
//
// WebP already opens as a standalone image / CBZ page. Inside an EPUB, MuPDF
// used to fail fz_new_image_from_buffer and draw the IMAGE placeholder.
// This packs a solid-red 80x80 WebP into a one-page EPUB and checks that
// rendering page 1 paints a block of red.
//
// Run:  bun tests/issue-3415.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, makeZip } from "./util.ts";

// 80x80 lossy WebP, solid (220,0,0). Generated with Pillow.
const RED_WEBP = Buffer.from(
  "UklGRogAAABXRUJQVlA4IHwAAACQCgCdASpQAFAAPjEYi0QiIaEQpAAgAwS0gDsAfgAZHy/XQ+Sq51Oag0RmM2sd7lgTdgvOxAwq/15A9eSzVg7ARCiwThz6Hylhz6AdgIhRYJybIc8Fs59AOwEQm4AA/v/xz1f/+mkeNI8aR9Jv//90CfuPL9x5f+5tAAAA",
  "base64",
);

function makeEpub(webp: Buffer): Buffer {
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
    `<dc:identifier id="id">urn:uuid:issue-3415</dc:identifier>` +
    `<dc:title>webp in epub</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="img" href="red.webp" media-type="image/webp"/>` +
    `</manifest><spine><itemref idref="c1"/></spine></package>`;
  const html =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>t</title></head>` +
    `<body style="margin:0"><img src="red.webp" width="80" height="80" alt="red"/></body></html>`;
  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(html) },
    { name: "OEBPS/red.webp", data: webp, store: true },
  ]);
}

function parseColors(raw: string): { red: number; nonWhite: number; w: number; h: number } {
  const m = /red=(\d+) nonwhite=(\d+) size=(\d+)x(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`issue-3415: could not parse: ${raw}`);
  }
  return { red: +m[1]!, nonWhite: +m[2]!, w: +m[3]!, h: +m[4]! };
}

export async function testit(): Promise<void> {
  const webpPath = tmpPath("issue-3415.webp");
  const epubPath = tmpPath("issue-3415.epub");
  writeFileSync(webpPath, RED_WEBP);
  writeFileSync(epubPath, makeEpub(RED_WEBP));

  await withControlledSumatra(EXE, async (client) => {
    {
      const res = await client.request(ControlCommand.TestRenderPageColors, [webpPath]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-3415 standalone webp: ${raw.trim()}`);
      }
      const c = parseColors(raw);
      if (c.red < 200) {
        throw new Error(`issue-3415 standalone webp: red=${c.red}, want many:\n${raw}`);
      }
      console.log(`  standalone webp: ${c.w}x${c.h} red=${c.red} ✓`);
    }
    {
      const res = await client.request(ControlCommand.TestRenderPageColors, [epubPath]);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`issue-3415 epub: ${raw.trim()}`);
      }
      const c = parseColors(raw);
      if (c.red < 200) {
        throw new Error(`issue-3415 epub: red=${c.red} (WebP still a placeholder?):\n${raw}`);
      }
      console.log(`  epub webp: ${c.w}x${c.h} red=${c.red} ✓`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
