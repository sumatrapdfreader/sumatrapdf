// #5329: annotation hover tooltips show contents and, when enabled, the author.
// FreeText only needs the author because its contents are already on the page.

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, assemblePdf } from "./util.ts";

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

enum AuthorMode {
  Default,
  Show,
}

async function readComments(pdf: string, mode: AuthorMode): Promise<string[]> {
  const showAuthor = mode === AuthorMode.Show;
  const appdata = tmpPath(`issue-5329-${showAuthor ? "author" : "default"}`);
  mkdirSync(appdata, { recursive: true });
  const settings = showAuthor ? "ShowAnnotationAuthorInTooltip = true\n" : "";
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), settings);

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
    ["-appdata", appdata, pdf],
  );
  return parseComments(raw);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-5329.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const defaults = await readComments(pdf, AuthorMode.Default);
  const wantDefaults = ["hello note", "just text"].sort();
  if (defaults.join("\n") !== wantDefaults.join("\n")) {
    throw new Error(`issue-5329: default comments=${JSON.stringify(defaults)} want ${JSON.stringify(wantDefaults)}`);
  }

  const withAuthor = await readComments(pdf, AuthorMode.Show);
  const wantWithAuthor = ["hello note|Author: Alice", "Author: Bob", "Author: Carol", "just text"].sort();
  if (withAuthor.join("\n") !== wantWithAuthor.join("\n")) {
    throw new Error(`issue-5329: author comments=${JSON.stringify(withAuthor)} want ${JSON.stringify(wantWithAuthor)}`);
  }
  console.log(`issue-5329: ${withAuthor.join("; ")}`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
