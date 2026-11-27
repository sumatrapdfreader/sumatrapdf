// Applying a redaction rewrites the /ActualText of the tagged structure element
// whose text it removes. mupdf wrote the redacted string to /Alt instead, so the
// original /ActualText, with the redacted words, stayed in the saved file
// (mupdf bug 709973, ext/patches/backport-redact-actualtext).
//
// Run:  bun tests/redact-actualtext.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

const SECRET = "SECRETWORD";
const TEXT = `${SECRET} KEEPWORD`;

// redact the secret word, save, and print what the structure element says now
const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]).asPDF();
var page = doc.loadPage(0);
var hits = page.search("${SECRET}");
if (hits.length !== 1) throw new Error("search found " + hits.length);
var q = hits[0][0];
var annot = page.createAnnotation("Redact");
annot.setRect([Math.min(q[0], q[4]), Math.min(q[1], q[3]), Math.max(q[2], q[6]), Math.max(q[5], q[7])]);
annot.update();
page.applyRedactions();
doc.save(scriptArgs[1]);

var out = Document.openDocument(scriptArgs[1]).asPDF();
var elem = out.getTrailer().Root.StructTreeRoot.K[0];
var actual = elem.get("ActualText");
print("ActualText=" + (actual.isNull() ? "<none>" : actual.asString()));
`;

// One page with "SECRETWORD KEEPWORD" in a marked-content span whose
// structure element carries the same /ActualText.
function makeTaggedPdf(): string {
  const content = `/Span << /MCID 0 >> BDC BT /F1 24 Tf 72 700 Td (${TEXT}) Tj ET EMC`;
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /MarkInfo << /Marked true >> /StructTreeRoot 6 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 5 0 R >> >> " +
      "/Contents 4 0 R /StructParents 0 >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    "<< /Type /StructTreeRoot /K [7 0 R] /ParentTree << /Nums [0 [7 0 R]] >> >>",
    `<< /Type /StructElem /S /Span /P 6 0 R /Pg 3 0 R /K 0 /ActualText (${TEXT}) >>`,
  ]);
}

export async function testit(): Promise<void> {
  const script = tmpPath("redact-actualtext.js");
  const pdf = tmpPath("redact-actualtext.pdf");
  const out = tmpPath("redact-actualtext-out.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(pdf, makeTaggedPdf(), "latin1");

  const r = Bun.spawnSync([EXE, "run", script, pdf, out]);
  const res = r.stdout.toString() + r.stderr.toString();
  const actual = /^ActualText=(.*)$/m.exec(res)?.[1];
  if (r.exitCode !== 0 || actual === undefined) {
    throw new Error(`redact-actualtext: exit ${r.exitCode}\n${res}`);
  }
  if (actual.includes(SECRET) || !actual.includes("KEEPWORD")) {
    throw new Error(`redact-actualtext: /ActualText after redaction is "${actual}"\n${res}`);
  }
  console.log("redact-actualtext: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
