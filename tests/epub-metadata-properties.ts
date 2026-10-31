// Document Properties of an EPUB shows the package's metadata: publisher, identifier,
// source, language, dates and a description stored as escaped HTML, here without a
// dc: prefix. The OPF follows what a user reported for a retail EPUB.
//
// Run: bun tests/epub-metadata-properties.ts [--no-build]
import { writeFileSync } from "node:fs";
import { ControlCommand } from "./control.ts";
import { cmdId, makeZip, runStandalone, tmpPath, USE_NG, type ZipEntry } from "./util.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";
import { enumChildWindows, enumWindows, getClassName, getControlText, getWindowPid, getWindowText } from "./winapi.ts";

const kDescription =
  "&lt;i&gt;&lt;b&gt;The Club &lt;/b&gt;&lt;/i&gt;&lt;b&gt;meets the Play.&lt;/b&gt;&lt;BR&gt;" +
  "Mark plans a trip&amp;#8212;a last one.&lt;BR&gt; &lt;BR&gt;" +
  "Sequence is 3 &amp;lt; 4. " +
  "The companions keep talking on the road. ".repeat(20);

export function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/9781982142605.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" unique-identifier="eisbn" version="3.0" xml:lang="en-us">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:opf="http://www.idpf.org/2007/opf">` +
    `<dc:title>Together</dc:title><dc:creator>An Author</dc:creator>` +
    `<dc:publisher>Gallery/Scout Press</dc:publisher>` +
    `<dc:identifier id="eisbn">9781982142605</dc:identifier>` +
    `<dc:source id="srcisbn">9781982142582</dc:source>` +
    `<meta property="source-of" refines="#srcisbn">pagination</meta>` +
    `<dc:language>EN-US</dc:language>` +
    `<dc:date>2021-07-06</dc:date>` +
    `<meta property="dcterms:modified">2021-08-09T12:00:00Z</meta>` +
    `<description xmlns="http://purl.org/dc/elements/1.1/">${kDescription}</description>` +
    `</metadata><manifest>` +
    `<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>` +
    `</manifest><spine><itemref idref="c1"/></spine></package>`;
  const page =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<html xmlns="http://www.w3.org/1999/xhtml"><head><title>c</title></head><body><p>Text</p></body></html>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/9781982142605.opf", data: enc.encode(opf) },
    { name: "OEBPS/c1.xhtml", data: enc.encode(page) },
  ];
  return makeZip(entries);
}

function parseProps(raw: string): Record<string, string> {
  const out: Record<string, string> = {};
  for (const line of raw.split(/\r?\n/)) {
    const eq = line.indexOf("=");
    if (eq > 0) {
      out[line.slice(0, eq)] = line.slice(eq + 1);
    }
  }
  return out;
}

async function dialogText(pid: number, frame: number): Promise<string> {
  sendCommand(frame, cmdId("CmdProperties"));
  for (let i = 0; i < 100; i++) {
    let text = "";
    enumWindows((h) => {
      if (getWindowPid(h) !== pid || getWindowText(h) !== "Document Properties") {
        return true;
      }
      enumChildWindows(h, (c) => {
        if (getClassName(c) === "Edit") {
          text = getControlText(c);
          return false;
        }
        return true;
      });
      return false;
    });
    if (text) {
      return text;
    }
    await new Promise((r) => setTimeout(r, 100));
  }
  throw new Error("epub-metadata-properties: no Document Properties dialog");
}

export async function testit(): Promise<void> {
  const epub = tmpPath("epub-metadata-properties.epub");
  writeFileSync(epub, makeEpub());

  const { proc, client, frame } = await launchControlled([epub]);
  try {
    await client.waitForRenderIdle();
    const res = await client.request(ControlCommand.TestDocumentProperties);
    const props = parseProps(String(res[1] ?? ""));

    const wanted: Record<string, string> = {
      title: "Together",
      author: "An Author",
      publisher: "Gallery/Scout Press",
      identifier: "9781982142605",
      source: "9781982142582",
      language: "EN-US",
      creationDate: "2021-07-06",
      modDate: "2021-08-09T12:00:00Z",
    };
    for (const [key, val] of Object.entries(wanted)) {
      if (props[key] !== val) {
        throw new Error(`epub-metadata-properties: ${key}: wanted '${val}', got ${JSON.stringify(props)}`);
      }
    }

    const start = "The Club meets the Play. Mark plans a trip—a last one. Sequence is 3 < 4. The companions";
    if (!props.subject?.startsWith(start)) {
      throw new Error(`epub-metadata-properties: description: ${props.subject}`);
    }

    // ng's dialog is gpui, with no Edit to read
    if (!USE_NG) {
      const text = await dialogText(proc.pid!, frame);
      for (const line of [
        /Publisher: +Gallery\/Scout Press/,
        /Identifier: +9781982142605/,
        /Source: +9781982142582/,
        /Language: +EN-US/,
        /Created: +\S*2021/,
        /Modified: +\S*2021/,
        /Subject: +The Club meets/,
      ]) {
        if (!line.test(text)) {
          throw new Error(`epub-metadata-properties: no ${line} in the dialog:\n${text}`);
        }
      }
    }
    await client.quit();
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
