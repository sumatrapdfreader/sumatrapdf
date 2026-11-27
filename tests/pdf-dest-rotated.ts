// A link or bookmark destination on a page rotated 90 or 270 degrees: the
// page's x becomes the screen's y. mupdf kept or dropped each coordinate by
// the wrong axis, so [page /XYZ null 500 0] (top 500, no left: the usual form)
// went to a made-up height and lost the real one, and /FitH 500 scrolled to a
// y made from 0 (ext/patches/0095-pdf-dest-rotated-page-axes).
//
// Run:  bun tests/pdf-dest-rotated.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

// Destination as "type x y", NaN for a coordinate the destination leaves open.
const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]);
var names = ["topOnly", "top0", "leftOnly", "left0", "fitH", "upright"];
for (var i = 0; i < names.length; i++) {
  var d = doc.resolveLinkDestination("#nameddest=" + names[i]);
  print(names[i] + " " + d.type + " " + d.x + " " + d.y);
}
`;

// page 1 rotated 90 degrees, page 2 upright
function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /Dests 5 0 R >>",
    "<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Rotate 90 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /topOnly [3 0 R /XYZ null 500 0] /top0 [3 0 R /XYZ 0 500 0] " +
      "/leftOnly [3 0 R /XYZ 100 null 0] /left0 [3 0 R /XYZ 100 0 0] " +
      "/fitH [3 0 R /FitH 500] /upright [4 0 R /XYZ null 500 0] >>",
  ]);
}

type Dest = { type: string; x: number; y: number };

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-dest-rotated.js");
  const pdf = tmpPath("pdf-dest-rotated.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(pdf, makePdf(), "latin1");

  const r = Bun.spawnSync([EXE, "run", script, pdf]);
  const out = r.stdout.toString() + r.stderr.toString();
  const d: Record<string, Dest> = {};
  for (const m of out.matchAll(/^(\w+) (\S+) (\S+) (\S+)$/gm)) {
    d[m[1]!] = { type: m[2]!, x: Number(m[3]), y: Number(m[4]) };
  }
  const fail = (what: string) => {
    throw new Error(`pdf-dest-rotated: ${what}\n${out}`);
  };
  if (r.exitCode !== 0 || Object.keys(d).length !== 6) {
    fail(`exit ${r.exitCode}`);
  }

  // top 500 on the rotated page is a screen x; the screen y is unknown
  if (d.topOnly!.x !== d.top0!.x || !Number.isNaN(d.topOnly!.y)) {
    fail(`/XYZ null 500: got x=${d.topOnly!.x} y=${d.topOnly!.y}, want x=${d.top0!.x} y=NaN`);
  }
  // left 100 is a screen y; the screen x is unknown
  if (d.leftOnly!.y !== d.left0!.y || !Number.isNaN(d.leftOnly!.x)) {
    fail(`/XYZ 100 null: got x=${d.leftOnly!.x} y=${d.leftOnly!.y}, want x=NaN y=${d.left0!.y}`);
  }
  // FitH keeps fitting the width, but its top is a screen x on a rotated
  // page: no screen y, so the page top, not a y made from 0
  if (d.fitH!.type !== "FitH" || !Number.isNaN(d.fitH!.y)) {
    fail(`/FitH 500: got ${d.fitH!.type} y=${d.fitH!.y}, want FitH y=NaN`);
  }
  // an upright page is unchanged: top 500 of 792 is 292 from the top
  if (d.upright!.y !== 292 || !Number.isNaN(d.upright!.x)) {
    fail(`upright /XYZ null 500: got x=${d.upright!.x} y=${d.upright!.y}`);
  }
  console.log("pdf-dest-rotated: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
