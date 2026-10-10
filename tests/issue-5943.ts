// #5943: EPUB TOC entries whose ids sit on nested <span>s wrapping a block
// all resolved to the chapter's first page. MuPDF kept appending to the same
// interrupted inline context; upstream 37a14e17 (bug 709648) starts a new one.
//
// Run: bun tests/issue-5943.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, runControlCommand } from "./control.ts";
import { EXE, runStandalone, tmpPath, makeZip } from "./util.ts";

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const filler = (n: number) =>
    Array.from({ length: 60 }, (_, i) => `<p>Filler paragraph ${n}.${i + 1} of section ${n}.</p>`).join("\n");

  const body: string[] = [];
  const nav: string[] = [];
  let playOrder = 0;
  const navPoint = (label: string, id: string) => {
    playOrder++;
    nav.push(
      `    <navPoint id="np${playOrder}" playOrder="${playOrder}">` +
        `<navLabel><text>${label}</text></navLabel>` +
        `<content src="ch.xhtml#${id}"/></navPoint>`,
    );
  };

  body.push(`<span id="outer"><div class="h">Outer</div>`);
  navPoint("outer (span)", "outer");
  for (let n = 1; n <= 3; n++) {
    body.push(`<span id="span${n}"><div class="h">Span-anchored section ${n}</div>`);
    body.push(filler(n));
    body.push(`<div id="div${n}" class="h">Div-anchored section ${n}</div>`);
    body.push(filler(n + 10));
    body.push(`</span>`);
    navPoint(`span-anchored section ${n}`, `span${n}`);
    navPoint(`div-anchored section ${n}`, `div${n}`);
  }
  body.push(`</span>`);

  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/book.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">span-anchor-repro</dc:identifier>` +
    `<dc:title>span anchor repro</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>` +
    `<item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>` +
    `<item id="ch" href="ch.xhtml" media-type="application/xhtml+xml"/>` +
    `</manifest><spine toc="ncx"><itemref idref="ch"/></spine></package>`;
  const ncx =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">` +
    `<head><meta name="dtb:uid" content="span-anchor-repro"/></head>` +
    `<docTitle><text>span anchor repro</text></docTitle>` +
    `<navMap>\n${nav.join("\n")}\n</navMap></ncx>`;
  const html =
    `<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE html>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>chapter</title></head>` +
    `<body>\n${body.join("\n")}\n</body></html>`;

  return makeZip([
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/book.opf", data: enc.encode(opf) },
    { name: "OEBPS/toc.ncx", data: enc.encode(ncx) },
    { name: "OEBPS/ch.xhtml", data: enc.encode(html) },
  ]);
}

function parsePages(toc: string): Map<string, number> {
  const out = new Map<string, number>();
  for (const line of toc.split("\n")) {
    const m = /^(.*)\|page=(\d+)$/.exec(line);
    if (m) {
      out.set(m[1]!.trim(), +m[2]!);
    }
  }
  return out;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5943");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "repro.epub");
  writeFileSync(epub, makeEpub());

  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestGetToc, [epub]);
  if (exitCode !== 0) {
    throw new Error(`issue-5943: TestGetToc failed: ${String(raw ?? "").trim()}`);
  }
  const pages = parsePages(String(raw ?? ""));
  const span = [1, 2, 3].map((n) => pages.get(`span-anchored section ${n}`));
  const div = [1, 2, 3].map((n) => pages.get(`div-anchored section ${n}`));
  if (span.some((p) => p == null) || div.some((p) => p == null)) {
    throw new Error(`issue-5943: missing TOC entries:\n${raw}`);
  }
  if (!(span[0]! < span[1]! && span[1]! < span[2]!)) {
    throw new Error(
      `issue-5943: nested span ids all land on the same page ` +
        `(span=${span.join(",")} div=${div.join(",")})\n${raw}`,
    );
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
