// #6509: Extract Pages works for documents MuPDF opens that aren't PDF (XPS,
// EPUB, ...), writing the chosen pages of the open document to a new PDF.
// The XPS pages are told apart by width; the EPUB result must keep its text.
//
// Run: bun tests/issue-6509.ts [--no-build] [-ng]

import { readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { pdfPageCount } from "./print-util.ts";
import { extractPageText, makeZip, ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

// XPS units are 1/96 inch; the PDF has the same width in points (x 72/96)
const XPS_WIDTHS = [300, 400, 500];

function xpsFile(): Buffer {
  const enc = (s: string) => new TextEncoder().encode(s);
  const ns = "http://schemas.microsoft.com/xps/2005/06";
  const pages = XPS_WIDTHS.map(
    (w, i) =>
      `<FixedPage xmlns="${ns}" Width="${w}" Height="200" xml:lang="en">` +
      `<Path Data="M 10,10 L ${50 + i * 20},10 L ${50 + i * 20},100 Z" Fill="#FF0000"/></FixedPage>`,
  );
  return makeZip([
    {
      name: "[Content_Types].xml",
      data: enc(
        `<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">` +
          `<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>` +
          `<Default Extension="fdseq" ContentType="application/vnd.ms-package.xps-fixeddocumentsequence+xml"/>` +
          `<Default Extension="fdoc" ContentType="application/vnd.ms-package.xps-fixeddocument+xml"/>` +
          `<Default Extension="fpage" ContentType="application/vnd.ms-package.xps-fixedpage+xml"/></Types>`,
      ),
    },
    {
      name: "_rels/.rels",
      data: enc(
        `<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">` +
          `<Relationship Type="http://schemas.microsoft.com/xps/2005/06/fixedrepresentation" ` +
          `Target="/FixedDocSeq.fdseq" Id="R0"/></Relationships>`,
      ),
    },
    {
      name: "FixedDocSeq.fdseq",
      data: enc(
        `<FixedDocumentSequence xmlns="${ns}"><DocumentReference Source="/Documents/1/FixedDoc.fdoc"/>` +
          `</FixedDocumentSequence>`,
      ),
    },
    {
      name: "Documents/1/FixedDoc.fdoc",
      data: enc(
        `<FixedDocument xmlns="${ns}">` +
          XPS_WIDTHS.map((_, i) => `<PageContent Source="Pages/${i + 1}.fpage"/>`).join("") +
          `</FixedDocument>`,
      ),
    },
    ...pages.map((p, i) => ({ name: `Documents/1/Pages/${i + 1}.fpage`, data: enc(p) })),
  ]);
}

function pageWidths(pdf: string): number[] {
  const text = readFileSync(pdf, "latin1");
  return [...text.matchAll(/\/MediaBox\s*\[\s*0\s+0\s+([\d.]+)/g)].map((m) => Math.round(+m[1]!));
}

async function extract(doc: string, dest: string, pages: string): Promise<void> {
  rmSync(dest, { force: true });
  const { proc, client } = await launchControlled([doc]);
  try {
    await client.waitForRenderIdle();
    const res = await client.request(ControlCommand.TestExtractPages, [dest, pages]);
    if (res[0] !== 0) {
      throw new Error(`issue-6509: extract from ${doc}: ${String(res[1] ?? "").trim()}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const xps = tmpPath("issue-6509.xps");
  writeFileSync(xps, xpsFile());
  const xpsOut = tmpPath("issue-6509-xps.pdf");
  await extract(xps, xpsOut, "2-3");
  const widths = pageWidths(xpsOut);
  const want = XPS_WIDTHS.slice(1).map((w) => Math.round((w * 72) / 96));
  if (JSON.stringify(widths) !== JSON.stringify(want)) {
    throw new Error(
      `issue-6509: XPS pages 2-3 gave page widths ${JSON.stringify(widths)}, want ${JSON.stringify(want)}`,
    );
  }

  const epub = join(ROOT, "tests", "issue-6095.epub");
  const epubOut = tmpPath("issue-6509-epub.pdf");
  await extract(epub, epubOut, "1-2");
  const n = pdfPageCount(epubOut);
  if (n !== 2) {
    throw new Error(`issue-6509: EPUB pages 1-2 gave ${n} pages, want 2`);
  }
  if (extractPageText(epubOut).trim().length === 0) {
    throw new Error("issue-6509: the PDF extracted from the EPUB has no text");
  }
  console.log("issue-6509: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
