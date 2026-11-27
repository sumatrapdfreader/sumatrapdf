// Two faces of one font collection (.ttc) added to a PDF, e.g. by text stamps
// in MS Gothic and MS PGothic, are two fonts. mupdf found an added font by a
// digest of its file, which the faces share, so the second face got the first
// one's font object and drew with its glyphs and widths
// (ext/patches/0101-pdf-font-resource-ttc-face).
//
// Run:  bun tests/pdf-ttc-font-faces.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

const TTC = "C:/Windows/Fonts/msgothic.ttc";

const SCRIPT = `
var doc = new PDFDocument();
var a = doc.addFont(new Font("face0", scriptArgs[0], 0));
var b = doc.addFont(new Font("face1", scriptArgs[0], 1));
var again = doc.addFont(new Font("face0", scriptArgs[0], 0));
print("face0=" + a + " face1=" + b + " again=" + again);
`;

export async function testit(): Promise<void> {
  if (!existsSync(TTC)) {
    console.log(`SKIP pdf-ttc-font-faces: no ${TTC}`);
    return;
  }
  const script = tmpPath("pdf-ttc-font-faces.js");
  writeFileSync(script, SCRIPT);
  const r = Bun.spawnSync([EXE, "run", script, TTC]);
  const out = r.stdout.toString() + r.stderr.toString();
  const m = /face0=(\S+ \S+ R) face1=(\S+ \S+ R) again=(\S+ \S+ R)/.exec(out);
  if (r.exitCode !== 0 || !m) {
    throw new Error(`pdf-ttc-font-faces: exit ${r.exitCode}\n${out}`);
  }
  if (m[1] === m[2]) {
    throw new Error(`pdf-ttc-font-faces: faces 0 and 1 share the font object ${m[1]}`);
  }
  if (m[1] !== m[3]) {
    throw new Error(`pdf-ttc-font-faces: face 0 added again is a new font ${m[3]}, want ${m[1]}`);
  }
  console.log("pdf-ttc-font-faces: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
