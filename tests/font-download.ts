// A built-in font that isn't embedded downloads on first use, into the font
// cache, and has to match the sha1 prefix in its file name
// (src/shared/EmbeddedResources.cpp). Offline: a local server stands in for
// R2 (SUMATRA_FONT_URL_BASE) and the cache is a temp dir (SUMATRA_FONT_CACHE).
//
// Tangut text needs NotoSerifTangut, which no build embeds and Windows has no
// system font for.
//
// Run: bun tests/font-download.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { remoteFontName, writeMirror } from "../cmd/upload-fonts.ts";
import { EXE, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const name = "font-download";
export const kTangutFont = "NotoSerifTangut-Regular.otf";

// one paragraph of Tangut, in the default EPUB font
export function makeTangutEpub(): Buffer {
  const enc = new TextEncoder();
  const text = String.fromCodePoint(0x17000, 0x17001, 0x17002, 0x17003);
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">tangut</dc:identifier><dc:title>tangut</dc:title><dc:language>en</dc:language>` +
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

type Server = { url: string; requests: string[]; junk: boolean; stop: () => void };

// serves the mirror's files, or junk in their place when `junk` is set
function startServer(mirror: string): Server {
  const s: Server = { url: "", requests: [], junk: false, stop: () => {} };
  const server = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch(req) {
      const file = decodeURIComponent(new URL(req.url).pathname.slice(1));
      s.requests.push(file);
      const path = join(mirror, file);
      if (file.includes("/") || file.includes("\\") || !existsSync(path)) {
        return new Response("not found", { status: 404 });
      }
      return new Response(s.junk ? "<html>not a font</html>" : readFileSync(path));
    },
  });
  s.url = `http://127.0.0.1:${server.port}`;
  s.stop = () => server.stop(true);
  return s;
}

// -extract-text lays the page out, which loads its fonts. Async: the server
// runs on this thread and must answer while the app waits for it
async function extractText(epub: string, server: Server, cache: string): Promise<string> {
  const p = Bun.spawn([EXE, "-for-testing", "-extract-text", "1", epub], {
    env: { ...process.env, SUMATRA_FONT_URL_BASE: server.url, SUMATRA_FONT_CACHE: cache },
    stdout: "pipe",
    stderr: "pipe",
  });
  const [out, err] = await Promise.all([new Response(p.stdout).text(), new Response(p.stderr).text()]);
  await p.exited;
  return out + err;
}

export async function testit(): Promise<void> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  const mirror = join(dir, "mirror");
  const cache = join(dir, "cache");
  mkdirSync(cache, { recursive: true });
  writeMirror(mirror, [kTangutFont]);
  const remote = remoteFontName(kTangutFont);
  const cached = join(cache, remote);
  const epub = join(dir, "tangut.epub");
  writeFileSync(epub, makeTangutEpub());

  const server = startServer(mirror);
  try {
    // empty cache: one download, kept under its URL name
    let out = await extractText(epub, server, cache);
    if (!out.includes(`font '${kTangutFont}': downloading`) || server.requests.length !== 1) {
      throw new Error(`${name}: expected one download, requests: ${server.requests.join(", ")}\n${out.slice(-800)}`);
    }
    if (!existsSync(cached) || statSync(cached).size !== statSync(join(mirror, remote)).size) {
      throw new Error(`${name}: ${remote} wasn't saved whole in the cache`);
    }

    // in the cache: no download
    out = await extractText(epub, server, cache);
    if (out.includes("downloading") || server.requests.length !== 1) {
      throw new Error(`${name}: a cached font was downloaded again: ${server.requests.join(", ")}`);
    }

    // a download that isn't the font (a proxy's error page) is not kept
    rmSync(cached);
    server.junk = true;
    out = await extractText(epub, server, cache);
    if (!out.includes("doesn't match its sha1") || existsSync(cached)) {
      throw new Error(`${name}: a download with the wrong sha1 was kept\n${out.slice(-800)}`);
    }
  } finally {
    server.stop();
  }
  console.log(`PASS ${name}: downloads into the cache, reuses it, rejects a bad download`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
