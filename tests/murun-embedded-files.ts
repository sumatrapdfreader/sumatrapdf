// `run` scripts: mupdf's JS helpers for embedded files and name trees threw on
// a PDF without /Names (getEmbeddedFiles, insertEmbeddedFile, loadNameTree) and
// on any name tree with /Kids (_loadNameTreeRec called an undefined function).
//
// Run:  bun tests/murun-embedded-files.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, makeBlankPdf, runStandalone, tmpPath } from "./util";

const SCRIPT = `
function check(what, got, want) {
  if (got !== want) throw new Error(what + ": got " + got + ", want " + want);
}
var doc = Document.openDocument(scriptArgs[0]);
check("files without /Names", Object.keys(doc.getEmbeddedFiles()).length, 0);
check("Dests without /Names", Object.keys(doc.loadNameTree("Dests")).length, 0);
doc.insertEmbeddedFile("a.txt", doc.addEmbeddedFile("a.txt", "text/plain", new Buffer("a"), new Date(), new Date(), false));
check("files after insert", Object.keys(doc.getEmbeddedFiles()).join(), "a.txt");
doc.deleteEmbeddedFile("a.txt");
check("files after delete", Object.keys(doc.getEmbeddedFiles()).length, 0);

var kids = Document.openDocument(scriptArgs[1]);
check("Dests with /Kids", Object.keys(kids.loadNameTree("Dests")).join(), "d1,d2");
print("OK");
`;

// a /Dests name tree split over two /Kids leaves
function makeKidsPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /Names << /Dests << /Kids [4 0 R 5 0 R] >> >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Limits [(d1) (d1)] /Names [(d1) [3 0 R /Fit]] >>",
    "<< /Limits [(d2) (d2)] /Names [(d2) [3 0 R /Fit]] >>",
  ]);
}

export async function testit(): Promise<void> {
  const script = tmpPath("murun-embedded-files.js");
  const blank = tmpPath("murun-embedded-files-blank.pdf");
  const kids = tmpPath("murun-embedded-files-kids.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(blank, makeBlankPdf(), "latin1");
  writeFileSync(kids, makeKidsPdf(), "latin1");

  const r = Bun.spawnSync([EXE, "run", script, blank, kids]);
  const out = r.stdout.toString() + r.stderr.toString();
  if (r.exitCode !== 0 || !out.includes("OK")) {
    throw new Error(`murun-embedded-files: exit ${r.exitCode}\n${out}`);
  }
  console.log("murun-embedded-files: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
