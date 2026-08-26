// #2873: annotation hover tips showed one line break per carriage return, so a
// short note rendered as a tall column of blank lines. An old SumatraPDF
// round-trip bug grew the separator by a CR on every save ("\r\n" -> "\r\r\n" ->
// "\r\r\r\n"), and files written by it are still out there. A CR run that ends
// in a LF must collapse to one break; a run of bare CRs (PDF's own line
// separator) keeps one break each; trailing blank lines are dropped.

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, assemblePdf } from "./util.ts";

// \r and \n inside a PDF literal string are written as the escapes \r and \n so
// the bytes survive whatever line endings the file itself uses
type Case = { name: string; contents: string; want: string };

const cases: Case[] = [
  { name: "plain-crlf", contents: String.raw`a\r\nb`, want: "a\nb" },
  { name: "legacy-2-cr", contents: String.raw`a\r\r\nb`, want: "a\nb" },
  { name: "legacy-4-cr", contents: String.raw`a\r\r\r\r\nb`, want: "a\nb" },
  // a real blank line: each \r\n collapses on its own, the empty line survives
  { name: "blank-line", contents: String.raw`a\r\n\r\nb`, want: "a\n\nb" },
  // bare CRs are PDF's own separator: one break each, so this is a blank line
  { name: "bare-cr-run", contents: String.raw`a\r\rb`, want: "a\n\nb" },
  { name: "trailing-junk", contents: String.raw`a\r\r\n\r\r\n`, want: "a" },
];

function makePdf(): string {
  const annots = cases
    .map(
      (c, i) =>
        `<< /Type /Annot /Subtype /Text /Rect [50 ${700 - i * 40} 70 ${720 - i * 40}] /Contents (${c.contents}) >>`,
    )
    .join(" ");
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots}] >>`,
  ];
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-2873.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const raw = await withControlledSumatra(
    EXE,
    async (client: ControlClient) => {
      const res = await client.request(ControlCommand.TestPageComments, [pdf, 1]);
      const exitCode = res[0] as number;
      const text = String(res[1] ?? "");
      if (exitCode !== 0) {
        throw new Error(`issue-2873: ${text.trim()}`);
      }
      return text;
    },
    [pdf],
  );

  // TestPageComments reports each comment on one line with \n written as |
  const got = raw
    .split("\n")
    .map((l) => l.trim())
    .filter((l) => l.startsWith("comment="))
    .map((l) => l.slice("comment=".length));
  if (got.length !== cases.length) {
    throw new Error(`issue-2873: got ${got.length} comments, want ${cases.length}: ${JSON.stringify(got)}`);
  }
  for (let i = 0; i < cases.length; i++) {
    const c = cases[i];
    const want = c.want.replaceAll("\n", "|");
    if (got[i] !== want) {
      throw new Error(`issue-2873: ${c.name}: got ${JSON.stringify(got[i])}, want ${JSON.stringify(want)}`);
    }
    if (got[i].includes("\r")) {
      throw new Error(`issue-2873: ${c.name}: carriage return left in ${JSON.stringify(got[i])}`);
    }
  }
  console.log(`issue-2873: ${cases.length} notes normalized (${got.join(" / ")})`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
