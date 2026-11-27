// A script that sets a checkbox or radio group's value (this.getField("r").value
// = "b") stores it as a name, as the PDF spec has it and a click does, and
// switches the buttons with their own appearances. mupdf stored a text string
// in /V and had the appearances regenerated, replacing the document's own
// button drawing with a generic one (ext/patches/0106-pdf-script-button-value).
// Our radio-button click (SetWidget toggle in Annotation.cpp) uses the same
// pdf_set_field_value path.
//
// Run:  bun tests/pdf-script-radio-value.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]).asPDF();
doc.enableJS();
var page = doc.loadPage(0);
var widgets = page.getWidgets();
var button;
for (var i = 0; i < widgets.length; i++)
  if (widgets[i].getName() == "set") button = widgets[i];
button.eventEnter();
button.eventDown();
button.eventUp();
page.update();
var group = doc.newIndirect(4, 0);
var a = doc.newIndirect(5, 0), b = doc.newIndirect(6, 0);
print("V=" + group.get("V") + " Vname=" + group.get("V").isName());
print("a.AS=" + a.get("AS") + " a.on=" + a.get("AP").get("N").get("a"));
print("b.AS=" + b.get("AS") + " b.on=" + b.get("AP").get("N").get("b"));
`;

function makePdf(): string {
  const on = "0 0 1 rg 4 4 10 10 re f";
  const off = "0 0 0 RG 1 1 16 16 re S";
  const js = 'this.getField("r").value = "b";';
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R 7 0 R] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [5 0 R 6 0 R 7 0 R] >>",
    // the radio group, "a" on
    "<< /FT /Btn /Ff 49152 /T (r) /V /a /Kids [5 0 R 6 0 R] >>",
    "<< /Type /Annot /Subtype /Widget /Parent 4 0 R /AS /a /Rect [72 700 90 718] /P 3 0 R " +
      "/AP << /N << /a 8 0 R /Off 9 0 R >> >> >>",
    "<< /Type /Annot /Subtype /Widget /Parent 4 0 R /AS /Off /Rect [100 700 118 718] /P 3 0 R " +
      "/AP << /N << /b 10 0 R /Off 11 0 R >> >> >>",
    `<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /T (set) /Rect [72 650 172 670] /P 3 0 R ` +
      `/A << /S /JavaScript /JS (${js}) >> >>`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 18 18] /Length ${on.length} >>\nstream\n${on}\nendstream`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 18 18] /Length ${off.length} >>\nstream\n${off}\nendstream`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 18 18] /Length ${on.length} >>\nstream\n${on}\nendstream`,
    `<< /Type /XObject /Subtype /Form /BBox [0 0 18 18] /Length ${off.length} >>\nstream\n${off}\nendstream`,
  ]);
}

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-script-radio-value.js");
  const pdf = tmpPath("pdf-script-radio-value.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(pdf, makePdf(), "latin1");
  const r = Bun.spawnSync([EXE, "run", script, pdf]);
  const out = r.stdout.toString() + r.stderr.toString();
  const vName = /Vname=(\w+)/.exec(out)?.[1];
  const aAS = /a\.AS=(\S+)/.exec(out)?.[1]?.replace(/^\//, "");
  const bAS = /b\.AS=(\S+)/.exec(out)?.[1]?.replace(/^\//, "");
  const aOn = /a\.on=(.+)$/m.exec(out)?.[1];
  const bOn = /b\.on=(.+)$/m.exec(out)?.[1];
  if (r.exitCode !== 0 || vName === undefined) {
    throw new Error(`pdf-script-radio-value: exit ${r.exitCode}\n${out}`);
  }
  const bad: string[] = [];
  if (vName !== "true") bad.push("/V isn't a name");
  if (aAS !== "Off" || bAS !== "b") bad.push(`states a=${aAS} b=${bAS}, want a=Off b=b`);
  if (aOn !== "8 0 R" || bOn !== "10 0 R") bad.push(`own appearances replaced: a=${aOn} b=${bOn}`);
  if (bad.length > 0) {
    throw new Error(`pdf-script-radio-value: ${bad.join("; ")}\n${out}`);
  }
  console.log("pdf-script-radio-value: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
