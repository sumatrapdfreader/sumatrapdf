// #5329: annotation hover tooltip should show author and contents together,
// and FreeText should show the author (the contents are already on the page).

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(): string {
  const annots = [
    "<< /Type /Annot /Subtype /Text /Rect [50 700 70 720] /T (Alice) /Contents (hello note) >>",
    "<< /Type /Annot /Subtype /FreeText /Rect [50 600 250 640] /T (Bob) /Contents (visible text) /DA (/Helv 12 Tf 0 g) >>",
    "<< /Type /Annot /Subtype /Highlight /Rect [72 500 200 520] /QuadPoints [72 520 200 520 72 500 200 500] /Contents (just text) >>",
    "<< /Type /Annot /Subtype /Text /Rect [50 400 70 420] /T (Carol) >>",
    "<< /Type /Annot /Subtype /FreeText /Rect [50 300 250 340] /Contents (no author) /DA (/Helv 12 Tf 0 g) >>",
  ].join(" ");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots}] >>`,
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  const size = objs.length + 1;
  body += `xref\n0 ${size}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += off.toString().padStart(10, "0") + " 00000 n \n";
  }
  body += `trailer\n<< /Size ${size} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
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
  const pdf = tmpPath("issue-5329.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const raw = await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      const res = await client.request(ControlCommand.TestPageComments, [pdf, 1]);
      const exitCode = res[0] as number;
      const text = String(res[1] ?? "");
      if (exitCode !== 0) {
        throw new Error(`issue-5329: ${text.trim()}`);
      }
      return text;
    },
    [pdf],
  );

  const got = parseComments(raw);
  const want = ["Alice|hello note", "Anonymous", "Bob", "Carol", "just text"].sort();
  if (got.join("\n") !== want.join("\n")) {
    throw new Error(`issue-5329: comments=${JSON.stringify(got)} want ${JSON.stringify(want)}`);
  }
  console.log(`issue-5329: ${got.join("; ")}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
