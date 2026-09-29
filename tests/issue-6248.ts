// Discussion #6248: a .docx opened (MuPDF renders it) but was sniffed as XPS:
// its zip has _rels/.rels like every Office Open XML file, so Save As offered
// .xps and Properties said XPS. It is detected as DOCX now.
//
// Run: bun tests/issue-6248.ts [--no-build]

import { rmSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { runStandalone, tmpPath, writeStoredZip } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { sleep } from "./winapi.ts";

const CONTENT_TYPES = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
<Default Extension="xml" ContentType="application/xml"/>
<Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>`;

const RELS = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>`;

// MuPDF's docx reader requires the document's relationships part
const DOC_RELS = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"></Relationships>`;

const DOCUMENT = `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
<w:body><w:p><w:r><w:t>Hello from a docx.</w:t></w:r></w:p></w:body>
</w:document>`;

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

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6248");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const docx = join(dir, "doc.docx");
  writeStoredZip(docx, [
    { name: "[Content_Types].xml", data: Buffer.from(CONTENT_TYPES) },
    { name: "_rels/.rels", data: Buffer.from(RELS) },
    { name: "word/_rels/document.xml.rels", data: Buffer.from(DOC_RELS) },
    { name: "word/document.xml", data: Buffer.from(DOCUMENT) },
  ]);

  const { proc, client } = await launchControlled([docx]);
  try {
    const deadline = Date.now() + 10_000;
    let props: Record<string, string> = {};
    for (;;) {
      const res = await client.request(ControlCommand.TestDocumentProperties, []);
      const raw = String(res[1] ?? "");
      if (res[0] === 0) {
        props = parseProps(raw);
        break;
      }
      if (res[0] !== 2 || Date.now() > deadline) {
        throw new Error(`issue-6248: the docx did not open: ${raw.trim()}`);
      }
      await sleep(150);
    }
    for (const key of ["defaultExt", "fileTypeExt"]) {
      if (props[key]?.toLowerCase() !== ".docx") {
        throw new Error(`issue-6248: ${key} is '${props[key]}', expected .docx`);
      }
    }
    console.log("issue-6248: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
