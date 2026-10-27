// `run` scripts: a multi-key PDFObject.get() that reached an array indexed it
// with the first key instead of the current one, so
// get("Root", "Pages", "Kids", 1) returned the first page, not the second.
//
// Run:  bun tests/murun-get-array-index.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, makeBlankPdf, runStandalone, tmpPath } from "./util";

const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]);
var trailer = doc.getTrailer();
var want = trailer.get("Root").get("Pages").get("Kids").get(1).asIndirect();
var got = trailer.get("Root", "Pages", "Kids", 1).asIndirect();
if (got !== want) throw new Error("Kids[1]: got object " + got + ", want " + want);
var box = doc.loadPage(0).getObject().get("MediaBox", 2).asNumber();
if (box !== 612) throw new Error("MediaBox[2]: got " + box + ", want 612");
print("OK");
`;

export async function testit(): Promise<void> {
  const script = tmpPath("murun-get-array-index.js");
  const pdf = tmpPath("murun-get-array-index.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(pdf, makeBlankPdf(2), "latin1");

  const r = Bun.spawnSync([EXE, "run", script, pdf]);
  const out = r.stdout.toString() + r.stderr.toString();
  if (r.exitCode !== 0 || !out.includes("OK")) {
    throw new Error(`murun-get-array-index: exit ${r.exitCode}\n${out}`);
  }
  console.log("murun-get-array-index: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
