// Fallback fonts that aren't embedded come from the system before a download:
// - math, music, symbols and emoji, which mupdf asks for directly: on Windows
//   Segoe UI Symbol / Emoji, elsewhere a system font with the character
//   (src/shared/EmbeddedResources.cpp)
// - CJK text in a non-embedded PDF font: on Windows the CJK UI font every
//   install has (Malgun Gothic for Korean) when the classic one (Batang) isn't
//   installed (src/shared/mupdf/mupdf_load_system_font.c). On Linux / macOS a
//   system font only counts when it has the character: without a Korean font
//   the text must fall back to mupdf's downloaded font, not draw boxes.
// A local server stands in for R2 (SUMATRA_FONT_URL_BASE) and logs requests.
// For ng on Linux pass -exe out/linux/dbg/SumatraPDF.
//
// Run: bun tests/font-windows-substitutes.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC, IS_WIN } from "./host.ts";
import { assemblePdf, EXE, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const name = "font-system-substitutes";

// one paragraph in the default EPUB font, which has none of these characters
function makeEpub(text: string): Buffer {
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
    `<dc:identifier id="id">symbols</dc:identifier><dc:title>symbols</dc:title><dc:language>en</dc:language>` +
    `</metadata><manifest><item id="c0" href="c0.xhtml" media-type="application/xhtml+xml"/></manifest>` +
    `<spine><itemref idref="c0"/></spine></package>`;
  const xhtml =
    `<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml">` +
    `<head><title>t</title></head><body><p>${text}</p></body></html>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    { name: "OEBPS/c0.xhtml", data: enc.encode(xhtml) },
  ];
  return makeZip(entries);
}

// Korean text in Batang, a CID font the PDF doesn't embed
function makeKoreanPdf(): string {
  // 한국어 as UCS-2 for UniKS-UCS2-H
  const content = "BT /F1 24 Tf 72 700 Td <D55CAD6DC5B4> Tj ET";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> /Contents 4 0 R >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    "<< /Type /Font /Subtype /Type0 /BaseFont /Batang /Encoding /UniKS-UCS2-H /DescendantFonts [6 0 R] >>",
    "<< /Type /Font /Subtype /CIDFontType0 /BaseFont /Batang " +
      "/CIDSystemInfo << /Registry (Adobe) /Ordering (Korea1) /Supplement 2 >> /FontDescriptor 7 0 R >>",
    "<< /Type /FontDescriptor /FontName /Batang /Flags 6 /FontBBox [0 -140 1000 880] /ItalicAngle 0 " +
      "/Ascent 880 /Descent -120 /CapHeight 700 /StemV 80 >>",
  ]);
}

// -extract-text lays the page out, which loads its fonts
async function extractText(path: string, fontUrlBase: string, cache: string): Promise<string> {
  const p = Bun.spawn([EXE, "-for-testing", "-extract-text", "1", path], {
    env: { ...process.env, SUMATRA_FONT_URL_BASE: fontUrlBase, SUMATRA_FONT_CACHE: cache },
    stdout: "pipe",
    stderr: "pipe",
  });
  const [out, err] = await Promise.all([new Response(p.stdout).text(), new Response(p.stderr).text()]);
  await p.exited;
  return out + err;
}

// whether fontconfig has a font for the language (Linux)
function linuxHasFontFor(lang: string): boolean {
  const p = Bun.spawnSync(["fc-list", `:lang=${lang}`]);
  return p.exitCode === 0 && p.stdout.toString().trim().length > 0;
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  const cache = join(dir, "cache");
  mkdirSync(cache, { recursive: true });

  const requests: string[] = [];
  const server = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch(req) {
      requests.push(new URL(req.url).pathname.slice(1));
      return new Response("not found", { status: 404 });
    },
  });
  const base = `http://127.0.0.1:${server.port}`;
  try {
    // math bold A, a braille cell, a G clef, an emoji
    const symbols = String.fromCodePoint(0x1d400, 0x2800, 0x1d11e, 0x1f600);
    const epub = join(dir, "symbols.epub");
    writeFileSync(epub, makeEpub(symbols));
    const out = await extractText(epub, base, cache);
    // Segoe UI Symbol, the first fallback asked for, covers all four
    const want = IS_WIN ? "using Windows' seguisym.ttf" : "using a system font with U+";
    if (!out.includes(want)) {
      throw new Error(`${name}: symbols didn't use a system font:\n${out.slice(-1200)}`);
    }
    const symbolRequests = requests.splice(0);

    const pdf = join(dir, "korean.pdf");
    writeFileSync(pdf, makeKoreanPdf(), "latin1");
    const outPdf = await extractText(pdf, base, cache);

    // Windows has Malgun Gothic, macOS Apple SD Gothic Neo; Linux maybe nothing
    const haveKoreanFont = IS_WIN || IS_MAC || linuxHasFontFor("ko");
    const droid = requests.some((r) => r.startsWith("DroidSansFallbackFull"));
    if (haveKoreanFont && droid) {
      throw new Error(`${name}: downloaded a CJK font although the system has one: ${requests.join(", ")}`);
    }
    if (!haveKoreanFont && !droid) {
      throw new Error(`${name}: no Korean system font and no CJK download: ${requests.join(", ")}`);
    }
    // 한국 as UTF-8 hex, as -extract-text prints it. Without a Korean font the
    // download fails (the server has no fonts), so nothing lays the text out
    if (haveKoreanFont && !outPdf.includes("ed 95 9c ea b5 ad")) {
      throw new Error(`${name}: Korean text wasn't extracted:\n${outPdf.slice(-1200)}`);
    }
    if (IS_WIN && symbolRequests.length > 0) {
      throw new Error(`${name}: expected no symbol font downloads, requests: ${symbolRequests.join(", ")}`);
    }
  } finally {
    server.stop(true);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
