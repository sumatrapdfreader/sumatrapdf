// Test for issue #1724: "Slight inconsistency on how Link Contents are
// displayed".
//
// A link annotation can describe itself in /Contents ("Text to be displayed for
// the annotation ... in human-readable form", PDF 1.7 12.5.2). SumatraPDF shows
// the URL for links that go outside the document, but a link to a place inside
// the document has no URL and showed nothing at all, so its /Contents never
// reached the reader.
//
// The test asks the engine what each link on the page has to show: the two links
// into the document must now offer their /Contents, and the two external ones
// must still offer their URL rather than their /Contents.

import { writeFileSync } from "node:fs";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, assemblePdf } from "./util";

const URL = "https://www.sumatrapdfreader.org";

// one page, four link annotations: to a page inside the document and to a URL,
// each with and without a /Border, all four with a /Contents
function makePdf(): string {
  const annots =
    `<< /Type /Annot /Subtype /Link /Rect [50 45 120 60] /Border [0 0 1] ` +
    `/Contents (dest with border) /Dest [4 0 R /XYZ 0 0 null] >>` +
    `<< /Type /Annot /Subtype /Link /Rect [150 45 220 60] /Border [0 0 0] ` +
    `/Contents (dest no border) /Dest [4 0 R /XYZ 0 0 null] >>` +
    `<< /Type /Annot /Subtype /Link /Rect [50 95 120 110] /Border [0 0 1] ` +
    `/Contents (uri with border) /A << /S /URI /URI (${URL}) >> >>` +
    `<< /Type /Annot /Subtype /Link /Rect [150 95 220 110] /Border [0 0 0] ` +
    `/Contents (uri no border) /A << /S /URI /URI (${URL}) >> >>`;
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 150] /Annots [${annots}] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 300 150] >>`,
  ];
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-1724.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  await withControlledSumatra(EXE, async (client) => {
    const res = await client.request(ControlCommand.TestPageLinks, [pdf, 1]);
    const out = String(res[1] ?? res[0] ?? "");
    const values = [...out.matchAll(/^kind=(\S+) page=(-?\d+).* value=(.*)$/gm)].map((m) => ({
      kind: m[1]!,
      value: m[3]!.trim(),
    }));
    if (values.length !== 4) {
      throw new Error(`expected 4 links, got ${values.length}:\n${out}`);
    }

    const inDoc = values.filter((v) => v.kind === "destinationMupdf");
    const external = values.filter((v) => v.kind === "launchURL");
    if (inDoc.length !== 2 || external.length !== 2) {
      throw new Error(`expected 2 links inside the document and 2 external ones:\n${out}`);
    }
    // a link into the document has no URL, so it must offer its /Contents -
    // whether or not the link has a border
    for (const v of inDoc) {
      if (!v.value.startsWith("dest ")) {
        throw new Error(`a link into the document shows '${v.value}' instead of its /Contents:\n${out}`);
      }
    }
    // an external link keeps showing where it goes
    for (const v of external) {
      if (v.value !== URL) {
        throw new Error(`an external link shows '${v.value}' instead of its URL:\n${out}`);
      }
    }
    console.log(`  links into the document show their /Contents: ${inDoc.map((v) => `'${v.value}'`).join(", ")} ✓`);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
