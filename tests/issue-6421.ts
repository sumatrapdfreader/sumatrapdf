// issue #6421: an EPUB <video> / <audio> drew nothing. It is now a link to
// the media file in the EPUB, drawn as its poster image or a "[video: name]"
// label; clicking it opens the file in the system player.
// Run: bun tests/issue-6421.ts [--no-build]
import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, extractPageText, makeZip, runStandalone, tmpPath, type ZipEntry } from "./util";

const clipBytes = "fake m4v bytes";

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">issue-6421</dc:identifier>` +
    `<dc:title>Video</dc:title><dc:language>en</dc:language></metadata>` +
    `<manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `<item id="clip" href="clip.m4v" media-type="video/mp4"/>` +
    `<item id="movie" href="movie.mp4" media-type="video/mp4"/>` +
    `<item id="sound" href="sound.mp3" media-type="audio/mpeg"/>` +
    `<item id="poster" href="poster.svg" media-type="image/svg+xml"/>` +
    `</manifest><spine><itemref idref="c1"/></spine></package>`;
  const body =
    `<p>BEFORE</p>` +
    `<p><video src="clip.m4v" controls="controls"></video></p>` +
    `<video poster="poster.svg" width="200" height="100"><source src="movie.mp4" type="video/mp4"/></video>` +
    `<p><audio src="sound.mp3" controls="controls">no audio</audio></p>` +
    `<p>AFTER</p>`;
  const page =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>Video</title></head><body>${body}</body></html>`;
  const poster =
    `<svg xmlns="http://www.w3.org/2000/svg" width="200" height="100">` +
    `<rect width="200" height="100" fill="#3060c0"/></svg>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OPS/content.opf", data: enc.encode(opf) },
    { name: "OPS/c1.xhtml", data: enc.encode(page) },
    { name: "OPS/clip.m4v", data: enc.encode(clipBytes), store: true },
    { name: "OPS/movie.mp4", data: enc.encode("fake mp4"), store: true },
    { name: "OPS/sound.mp3", data: enc.encode("fake mp3"), store: true },
    { name: "OPS/poster.svg", data: enc.encode(poster) },
  ];
  return makeZip(entries);
}

export async function testit(): Promise<void> {
  const epub = tmpPath("issue-6421.epub");
  writeFileSync(epub, makeEpub());

  const text = extractPageText(epub, 1);
  for (const want of ["[video: clip.m4v]", "[audio: sound.mp3]"]) {
    if (!text.includes(want)) {
      throw new Error(`#6421: page text should include '${want}', got:\n${JSON.stringify(text)}`);
    }
  }
  if (text.includes("no audio")) {
    throw new Error(`#6421: fallback content of a playable <audio> should not be shown:\n${JSON.stringify(text)}`);
  }

  await withControlledSumatra(EXE, async (client: ControlClient) => {
    const res = await client.request(ControlCommand.TestPageLinks, [epub, 1]);
    const dump = String(res[1] ?? "");
    if (res[0] !== 0) {
      throw new Error(`#6421: TestPageLinks failed:\n${dump}`);
    }
    const want = [`OPS/clip.m4v size=${clipBytes.length}`, "OPS/movie.mp4 size=8", "OPS/sound.mp3 size=8"];
    for (const w of want) {
      const re = new RegExp(`^kind=launchEmbedded .* value=${w.replace(/\./g, "\\.")}$`, "m");
      if (!re.test(dump)) {
        throw new Error(`#6421: expected a launchEmbedded link '${w}', got:\n${dump}`);
      }
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
