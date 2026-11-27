// An abandoned PDF edit must not leave the font cache pointing at the objects
// it rolled back. Our 0089 purges the cached fonts, colorspaces and images of
// the objects undo / redo swap; abandoning an operation (an edit that failed
// part way, e.g. a stamp that threw) swaps them back too but purged nothing,
// so the next edit reused a font object that no longer existed and drew in a
// fallback font (ext/patches/0104-pdf-abandon-purge-resource-cache).
//
// Run:  bun tests/pdf-abandon-font-cache.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

const SCRIPT = `
var doc = new PDFDocument();
doc.enableJournal();
var font = new Font("Times-Roman");
doc.beginOperation("abandoned");
var a = doc.addSimpleFont(font, "Latin");
doc.abandonOperation();
doc.beginOperation("kept");
var b = doc.addSimpleFont(font, "Latin");
doc.endOperation();
var obj = b.resolve();
var isFont = obj !== null && obj.isDictionary() && String(obj.get("Type")) == "/Font";
print("first=" + a + " second=" + b + " isFont=" + isFont);
`;

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-abandon-font-cache.js");
  writeFileSync(script, SCRIPT);
  const r = Bun.spawnSync([EXE, "run", script]);
  const out = r.stdout.toString() + r.stderr.toString();
  const m = /isFont=(\w+)/.exec(out);
  if (r.exitCode !== 0 || !m) {
    throw new Error(`pdf-abandon-font-cache: exit ${r.exitCode}\n${out}`);
  }
  if (m[1] !== "true") {
    throw new Error(`pdf-abandon-font-cache: the font added after an abandoned edit isn't a font\n${out}`);
  }
  console.log("pdf-abandon-font-cache: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
