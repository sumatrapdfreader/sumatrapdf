// A sibling image with the book's base name (Calibre's "Title.epub" +
// "Title.jpg"), or else the cover an EPUB declares in its OPF, is the home
// page thumbnail instead of page 1 (issue #6236).
// issue-6236.jpg is a solid red 200x300 JPEG:
//   python -c "from PIL import Image; Image.new('RGB',(200,300),(220,30,30)).save('tests/issue-6236.jpg',quality=90)"
import { copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { loadPng, pngPixel, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const SRC_PDF = join(import.meta.dir, "issue-3219.pdf");
const COVER = join(import.meta.dir, "issue-6236.jpg");

function crc32(buf: Uint8Array): number {
  let c = 0xffffffff;
  for (const b of buf) {
    c ^= b;
    for (let k = 0; k < 8; k++) {
      c = c & 1 ? (c >>> 1) ^ 0xedb88320 : c >>> 1;
    }
  }
  return (c ^ 0xffffffff) >>> 0;
}

// zip with stored (uncompressed) entries, enough for an EPUB
function storedZip(entries: [string, Uint8Array][]): Buffer {
  const locals: Buffer[] = [];
  const centrals: Buffer[] = [];
  let offset = 0;
  for (const [name, data] of entries) {
    const nameBuf = Buffer.from(name, "utf8");
    const crc = crc32(data);
    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt32LE(crc, 14);
    local.writeUInt32LE(data.length, 18);
    local.writeUInt32LE(data.length, 22);
    local.writeUInt16LE(nameBuf.length, 26);
    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(20, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt32LE(crc, 16);
    central.writeUInt32LE(data.length, 20);
    central.writeUInt32LE(data.length, 24);
    central.writeUInt16LE(nameBuf.length, 28);
    central.writeUInt32LE(offset, 42);
    locals.push(local, nameBuf, Buffer.from(data));
    centrals.push(central, nameBuf);
    offset += 30 + nameBuf.length + data.length;
  }
  const centralSize = centrals.reduce((n, b) => n + b.length, 0);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralSize, 12);
  end.writeUInt32LE(offset, 16);
  return Buffer.concat([...locals, ...centrals, end]);
}

// EPUB whose first spine page is text and whose OPF names a cover image the
// EPUB 2 way (<meta name="cover">) or the EPUB 3 way (properties="cover-image");
// the cover is not in the spine
function writeEpubWithCover(path: string, version: 2 | 3): void {
  const utf8 = (s: string) => new TextEncoder().encode(s);
  const container = `<?xml version="1.0"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles>
</container>`;
  const opf = `<?xml version="1.0"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">
<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
<dc:identifier id="id">issue-6236</dc:identifier><dc:title>Cover test</dc:title><dc:language>en</dc:language>
${version === 2 ? '<meta name="cover" content="cover-img"/>' : ""}
</metadata>
<manifest>
<item id="page" href="page.xhtml" media-type="application/xhtml+xml"/>
<item id="cover-img" href="images/cover.jpg" media-type="image/jpeg"${version === 3 ? ' properties="cover-image"' : ""}/>
</manifest>
<spine><itemref idref="page"/></spine>
</package>`;
  const page = `<?xml version="1.0"?>
<html xmlns="http://www.w3.org/1999/xhtml"><head><title>p</title></head><body><p>Chapter one.</p></body></html>`;
  const zip = storedZip([
    ["mimetype", utf8("application/epub+zip")],
    ["META-INF/container.xml", utf8(container)],
    ["OEBPS/content.opf", utf8(opf)],
    ["OEBPS/page.xhtml", utf8(page)],
    ["OEBPS/images/cover.jpg", new Uint8Array(readFileSync(COVER))],
  ]);
  writeFileSync(path, zip);
}

// mirrors GetThumbnailPathTemp: md5 of the path, lowercase hex, .png
function thumbnailPath(appData: string, docPath: string): string {
  const hash = new Bun.CryptoHasher("md5").update(docPath).digest("hex");
  return join(appData, "sumatrapdfcache", `${hash}.png`);
}

async function waitForFile(path: string, what: string): Promise<void> {
  const deadline = Date.now() + 15_000;
  while (Date.now() < deadline) {
    if (existsSync(path)) {
      // the app writes the png in one go, but give it a moment to close it
      await sleep(200);
      return;
    }
    await sleep(100);
  }
  throw new Error(`issue-6236: ${what}: thumbnail not written: ${path}`);
}

function centerPixel(path: string): [number, number, number] {
  const img = loadPng(path);
  return pngPixel(img, Math.floor(img.w / 2), Math.floor(img.h / 2));
}

export async function testit(): Promise<void> {
  const root = tmpPath("issue-6236");
  rmSync(root, { recursive: true, force: true });
  const appData = join(root, "appdata");
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RememberOpenedFiles = true", "ShowStartPage = true", ""].join("\n"),
  );
  const withCover = join(root, "book.pdf");
  const plain = join(root, "plain.pdf");
  copyFileSync(SRC_PDF, withCover);
  copyFileSync(COVER, join(root, "book.jpg"));
  copyFileSync(SRC_PDF, plain);
  const epub2 = join(root, "embedded2.epub");
  writeEpubWithCover(epub2, 2);
  const epub3 = join(root, "embedded3.epub");
  writeEpubWithCover(epub3, 3);

  const { proc, client } = await launchControlled(["-appdata", appData, withCover, plain, epub2, epub3]);
  try {
    await client.waitForRenderIdle();
    await waitForFile(thumbnailPath(appData, withCover), "book.pdf");
    await waitForFile(thumbnailPath(appData, plain), "plain.pdf");
    await waitForFile(thumbnailPath(appData, epub2), "embedded2.epub");
    await waitForFile(thumbnailPath(appData, epub3), "embedded3.epub");
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const [r, g, b] = centerPixel(thumbnailPath(appData, withCover));
  if (r < 180 || g > 80 || b > 80) {
    throw new Error(`issue-6236: thumbnail should be the red cover, center pixel is rgb(${r},${g},${b})`);
  }
  const [r2, g2, b2] = centerPixel(thumbnailPath(appData, plain));
  if (r2 < 200 || g2 < 200 || b2 < 200) {
    throw new Error(`issue-6236: thumbnail without a cover should be page 1, center pixel is rgb(${r2},${g2},${b2})`);
  }
  for (const epub of [epub2, epub3]) {
    const [r3, g3, b3] = centerPixel(thumbnailPath(appData, epub));
    if (r3 < 180 || g3 > 80 || b3 > 80) {
      throw new Error(
        `issue-6236: ${epub} thumbnail should be its declared cover, center pixel is rgb(${r3},${g3},${b3})`,
      );
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
