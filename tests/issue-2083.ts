// #2083: Acrobat-style form-field hover tooltips are /TU on a /Widget
// (pdfcomment / hyperref buttons). They are usually /Ff read-only and live
// on MuPDF's widget list, not the markup-annot list.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, assemblePdf } from "./util.ts";

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R 5 0 R 6 0 R] >> >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R 6 0 R 7 0 R] >>",
    "<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /Rect [50 700 150 720] /T (tip1) /TU (Optical Flow) /F 4 >>",
    "<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /Rect [50 600 150 620] /T (tip2) /TU (Block RAM) /F 4 >>",
    "<< /Type /Annot /Subtype /Widget /FT /Btn /Ff 65536 /Rect [50 500 150 520] /T (notip) /F 4 >>",
    "<< /Type /Annot /Subtype /Square /Rect [50 400 150 420] /Contents (square note) >>",
  ];
  return assemblePdf(objs);
}

function parseComments(raw: string): string[] {
  return raw
    .split("\n")
    .map((l) => l.trim())
    .filter((l) => l.startsWith("comment="))
    .map((l) => l.slice("comment=".length))
    .sort();
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-2083.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const raw = await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      const res = await client.request(ControlCommand.TestPageComments, [pdf, 1]);
      const exitCode = res[0] as number;
      const text = String(res[1] ?? "");
      if (exitCode !== 0) {
        throw new Error(`issue-2083: ${text.trim()}`);
      }
      return text;
    },
    [pdf],
  );

  const got = parseComments(raw);
  const want = ["Block RAM", "Optical Flow", "square note"].sort();
  if (got.join("\n") !== want.join("\n")) {
    throw new Error(`issue-2083: comments=${JSON.stringify(got)} want ${JSON.stringify(want)}`);
  }
  console.log(`issue-2083: ${got.join("; ")}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
