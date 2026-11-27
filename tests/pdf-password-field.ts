// A password text field (/Ff bit 14) shows its value masked, and the
// appearance stream written into the PDF must be masked too. mupdf wrote the
// typed text as is, so it could be read from the saved file
// (ext/patches/0098-pdf-password-field-appearance).
//
// Run:  bun tests/pdf-password-field.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

const SECRET = "hunter2";

const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]).asPDF();
var page = doc.loadPage(0);
var widgets = page.getWidgets();
for (var i = 0; i < widgets.length; i++) {
  var w = widgets[i];
  w.setTextValue("${SECRET}");
  w.update();
  var ap = w.getObject().get("AP").get("N").readStream().asString();
  print(w.getName() + " ap=" + ap.replace(/\\n/g, " "));
}
`;

// a password field and, as a control, a plain text field
function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R 5 0 R] /DA (/Helv 12 Tf 0 g) " +
      "/DR << /Font << /Helv 6 0 R >> >> >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R] >>",
    "<< /Type /Annot /Subtype /Widget /FT /Tx /T (pass) /Ff 8192 /Rect [72 700 272 720] /P 3 0 R >>",
    "<< /Type /Annot /Subtype /Widget /FT /Tx /T (plain) /Rect [72 650 272 670] /P 3 0 R >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
  ]);
}

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-password-field.js");
  const pdf = tmpPath("pdf-password-field.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(pdf, makePdf(), "latin1");

  const r = Bun.spawnSync([EXE, "run", script, pdf]);
  const out = r.stdout.toString() + r.stderr.toString();
  const pass = /^pass ap=(.*)$/m.exec(out)?.[1];
  const plain = /^plain ap=(.*)$/m.exec(out)?.[1];
  if (r.exitCode !== 0 || pass === undefined || plain === undefined) {
    throw new Error(`pdf-password-field: exit ${r.exitCode}\n${out}`);
  }
  if (!plain.includes(SECRET)) {
    throw new Error(`pdf-password-field: plain field's appearance has no text: ${plain}`);
  }
  if (pass.includes(SECRET) || !pass.includes("*".repeat(SECRET.length))) {
    throw new Error(`pdf-password-field: password field's appearance isn't masked: ${pass}`);
  }
  console.log("pdf-password-field: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
