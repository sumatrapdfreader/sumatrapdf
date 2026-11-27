// A combo box whose options are [export value, display text] pairs stores the
// export value in /V, but must show the display text. mupdf drew /V, so a box
// set to "Banana" showed "2" (ext/patches/0099-pdf-combo-display-text).
//
// Run:  bun tests/pdf-combo-display.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]).asPDF();
var w = doc.loadPage(0).getWidgets()[0];
w.setChoiceValue("2");
w.update();
print("ap=" + w.getObject().get("AP").get("N").readStream().asString().replace(/\\n/g, " "));
`;

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-combo-display.js");
  const pdf = tmpPath("pdf-combo-display.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R] /DA (/Helv 12 Tf 0 g) " +
        "/DR << /Font << /Helv 5 0 R >> >> >> >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
      "<< /Type /Annot /Subtype /Widget /FT /Ch /Ff 131072 /T (fruit) /Rect [72 700 272 720] /P 3 0 R " +
        "/Opt [[(1) (Apple)] [(2) (Banana)]] >>",
      "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
    ]),
    "latin1",
  );

  const r = Bun.spawnSync([EXE, "run", script, pdf]);
  const out = r.stdout.toString() + r.stderr.toString();
  const ap = /^ap=(.*)$/m.exec(out)?.[1];
  if (r.exitCode !== 0 || ap === undefined) {
    throw new Error(`pdf-combo-display: exit ${r.exitCode}\n${out}`);
  }
  if (!ap.includes("(Banana)")) {
    throw new Error(`pdf-combo-display: appearance doesn't show "Banana": ${ap}`);
  }
  console.log("pdf-combo-display: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
