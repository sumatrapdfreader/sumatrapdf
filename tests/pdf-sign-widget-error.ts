// Creating a signature field that fails (here: the form's /Fields isn't an
// array) must report the error. mupdf deleted the half-made widget and then
// returned it as if it had worked, so Sign PDF went on to sign a widget that
// was no longer on the page (ext/patches/0096-pdf-signature-widget-error).
//
// Run:  bun tests/pdf-sign-widget-error.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, assemblePdf, runStandalone, tmpPath } from "./util";

const SCRIPT = `
var doc = Document.openDocument(scriptArgs[0]).asPDF();
var page = doc.loadPage(0);
var threw = "";
try {
  page.createSignature("Sig1");
} catch (e) {
  threw = String(e);
}
print("threw=" + threw);
print("widgets=" + page.getWidgets().length);
`;

export async function testit(): Promise<void> {
  const script = tmpPath("pdf-sign-widget-error.js");
  const pdf = tmpPath("pdf-sign-widget-error.pdf");
  writeFileSync(script, SCRIPT);
  writeFileSync(
    pdf,
    assemblePdf([
      "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields 5 >> >>",
      "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
      "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    ]),
    "latin1",
  );

  const r = Bun.spawnSync([EXE, "run", script, pdf]);
  const out = r.stdout.toString() + r.stderr.toString();
  const threw = /^threw=(.*)$/m.exec(out)?.[1];
  const widgets = /^widgets=(\d+)$/m.exec(out)?.[1];
  if (r.exitCode !== 0 || threw === undefined || widgets === undefined) {
    throw new Error(`pdf-sign-widget-error: exit ${r.exitCode}\n${out}`);
  }
  if (!threw.includes("not an array") || widgets !== "0") {
    throw new Error(`pdf-sign-widget-error: want an error and no widget, got threw="${threw}" widgets=${widgets}`);
  }
  console.log("pdf-sign-widget-error: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
