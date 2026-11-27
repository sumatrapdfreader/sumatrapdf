// A form field's text that mixes characters the simple appearance writer
// can't encode in the encoding it picks is drawn through the rich (HTML)
// layout. The check accepted a character if any 8-bit encoding had it, but
// the writer picks one by script, and neutral characters take the script
// before them: "x ≈ 5" was drawn as "x · 5" and "Привет ™" as "Привет •" (ext/patches/0109-pdf-appearance-encoding-check).
//
// Run:  bun tests/pdf-field-mixed-scripts.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

// "≈" is only in KOI8 and "™" only in cp1252; the rest must keep working
const VALUES = ["x ≈ 5", "Привет ™", "Čas 5", "Plain text", "Привет мир", "a ° Д"];

// sets each field and prints the text of each field's appearance
const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]).asPDF();
var values = JSON.parse(scriptArgs[1]);
var page = doc.loadPage(0);
var widgets = page.getWidgets();
for (var i = 0; i < widgets.length; i++) {
  widgets[i].setTextValue(values[i]);
  widgets[i].update();
}
print("TEXT-START");
for (var j = 0; j < widgets.length; j++)
  print(widgets[j].toDisplayList().toStructuredText().asText());
print("TEXT-END");
`;

function makePdf(n: number): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [${Array.from({ length: n }, (_, i) => `${4 + i} 0 R`).join(" ")}] /DA (/Helv 12 Tf 0 g) /DR << /Font << /Helv ${4 + n} 0 R >> >> >> >>`,
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${Array.from({ length: n }, (_, i) => `${4 + i} 0 R`).join(" ")}] >>`,
  ];
  for (let i = 0; i < n; i++) {
    const y = 700 - i * 40;
    objs.push(`<< /Type /Annot /Subtype /Widget /FT /Tx /T (f${i}) /Rect [72 ${y} 372 ${y + 20}] /P 3 0 R >>`);
  }
  objs.push("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-field-mixed-scripts.js");
  const pdf = tmpPath("pdf-field-mixed-scripts.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(pdf, makePdf(VALUES.length), "latin1");
  const r = Bun.spawnSync([EXE, "run", script, pdf, JSON.stringify(VALUES)]);
  const out = r.stdout.toString() + r.stderr.toString();
  const text = /TEXT-START\r?\n([\s\S]*)TEXT-END/.exec(out)?.[1];
  if (r.exitCode !== 0 || text === undefined) {
    throw new Error(`pdf-field-mixed-scripts: exit ${r.exitCode}\n${out}`);
  }
  const missing = VALUES.filter((v) => !text.includes(v));
  if (missing.length > 0) {
    throw new Error(`pdf-field-mixed-scripts: not drawn as typed: ${JSON.stringify(missing)}\npage text:\n${text}`);
  }
  console.log("pdf-field-mixed-scripts: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
