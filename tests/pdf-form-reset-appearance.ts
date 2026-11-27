// Resetting a form (a Reset button, a script's resetForm) switches a checkbox
// or radio button to its default state using the PDF's own appearances. mupdf
// also marked it to get a new appearance, so the document's own checkbox
// drawing was replaced by mupdf's generic one
// (ext/patches/0105-pdf-form-reset-keeps-appearance).
//
// Run:  bun tests/pdf-form-reset-appearance.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

// click the Reset button, update the page, report the checkbox's state and
// its normal appearances
const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]).asPDF();
var page = doc.loadPage(0);
var widgets = page.getWidgets();
var button, check;
for (var i = 0; i < widgets.length; i++) {
  if (widgets[i].getName() == "reset") button = widgets[i];
  if (widgets[i].getName() == "agree") check = widgets[i];
}
button.eventEnter();
button.eventDown();
button.eventUp();
page.update();
var obj = check.getObject();
var n = obj.get("AP").get("N");
print("AS=" + obj.get("AS") + " Yes=" + n.get("Yes") + " Off=" + n.get("Off"));
`;

function makePdf(): string {
  const yes = "0 0 1 rg 2 2 14 14 re f";
  const off = "0 0 0 RG 1 1 16 16 re S";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R 5 0 R] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
    // checked, no /DV: a reset turns it off
    "<< /Type /Annot /Subtype /Widget /FT /Btn /T (agree) /V /Yes /AS /Yes /Rect [72 700 90 718] /P 3 0 R " +
      "/AP << /N << /Yes 6 0 R /Off 7 0 R >> >> >>",
    "<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /T (reset) /Rect [72 650 172 670] /P 3 0 R " +
      "/A << /S /ResetForm >> >>",
    `<< /Type /XObject /Subtype /Form /BBox [0 0 18 18] /Length ${yes.length} >>\nstream\n${yes}\nendstream`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 18 18] /Length ${off.length} >>\nstream\n${off}\nendstream`,
  ]);
}

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-form-reset-appearance.js");
  const pdf = tmpPath("pdf-form-reset-appearance.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(pdf, makePdf(), "latin1");
  const r = Bun.spawnSync([EXE, "run", script, pdf]);
  const out = r.stdout.toString() + r.stderr.toString();
  const m = /AS=(\S+) Yes=(.+?) Off=(.+)$/m.exec(out);
  if (r.exitCode !== 0 || !m) {
    throw new Error(`pdf-form-reset-appearance: exit ${r.exitCode}\n${out}`);
  }
  if (m[1]!.replace(/^\//, "") !== "Off") {
    throw new Error(`pdf-form-reset-appearance: the reset left the checkbox ${m[1]}`);
  }
  if (m[2] !== "6 0 R" || m[3] !== "7 0 R") {
    throw new Error(`pdf-form-reset-appearance: the checkbox's own appearances were replaced: ${m[0]}`);
  }
  console.log("pdf-form-reset-appearance: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
