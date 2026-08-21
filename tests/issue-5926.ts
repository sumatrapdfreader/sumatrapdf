// issue #5926: a markdown file whose name has a non-ASCII character ("Bericht ä.md")
// only showed a 404. WebView2 percent-encodes such a name in the url it asks us
// for, and the percent-decoding shortened the buffer in place without telling the
// caller: it kept the *encoded* length, so the lookup path came out as
// "Bericht ä.md\0.md" and matched no file. Decoding now returns a new string
// (url::DecodeTemp) instead of mutating one.
//
// Two things have to work for the document to be shown, and both go through that
// decoding:
//   - the page itself is served, which the test sees by navigating to a heading
//     far down the document -- a 404 page has nothing to scroll
//   - a link in it resolves, here to a .pdf whose name also has an umlaut and a
//     space, which the app opens in a tab (discussion #5924)
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control";
import { EXE, makeMinimalPdf, runStandalone, tmpPath } from "./util";

// the .md and the .pdf it links to, both with a space and an umlaut in the name
const MD_NAME = "umlaut ä test.md";
const PDF_NAME = "umlaut ä doc.pdf";
// how WebView2 reports a link to PDF_NAME: utf-8, percent-encoded
const PDF_HREF = "umlaut%20%C3%A4%20doc.pdf";

const TARGET_DEST_NO = 3; // the file, "Start", then "Target Heading"
const MIN_TARGET_SCROLL_Y = 500;

async function tocNavigate(client: ControlClient, destNo: number, expected: string): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [destNo, MIN_TARGET_SCROLL_Y]);
    const exitCode = res[0] as number;
    const output = String(res[1] ?? "").trim();
    if (exitCode === 0 && output.startsWith(expected)) {
      return output;
    }
    if (exitCode !== 2 || !output.startsWith("NOTREADY")) {
      throw new Error(`issue-5926: navigating a document named '${MD_NAME}' failed: ${output}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5926: '${MD_NAME}' never rendered (404?): ${output}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

function makeFolder(): { dir: string; appdata: string } {
  const dir = tmpPath("issue-5926-data");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const paragraphs = Array.from(
    { length: 120 },
    (_, i) => `Absatz ${i + 1}: genug Text, damit die Überschrift unter dem Fenster liegt.`,
  );
  writeFileSync(
    join(dir, MD_NAME),
    ["# Start", `[das Handbuch](./${PDF_NAME})`, ...paragraphs, "## Target Heading", "Zielinhalt."].join("\n\n"),
  );
  writeFileSync(join(dir, PDF_NAME), makeMinimalPdf("Handbuch"));

  const appdata = tmpPath("issue-5926-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["MarkdownUI [", "\tUseFixedPageUI = false", "]", "RestoreSession = false", "ShowStartPage = false", ""].join("\n"),
  );
  return { dir, appdata };
}

export async function testit(): Promise<void> {
  const { dir, appdata } = makeFolder();

  await withControlledSumatra(
    EXE,
    async (client) => {
      // the page is served and scrollable: it rendered rather than 404ing
      await tocNavigate(client, TARGET_DEST_NO, "NAVIGATING");
      const landed = await tocNavigate(client, 0, "OK");
      console.log(`issue-5926: '${MD_NAME}' ${landed}`);

      // and a link inside it resolves to the umlaut .pdf next to it
      await client.request(ControlCommand.TestMarkdownFollowLink, [PDF_HREF, 1]);
      await new Promise((resolve) => setTimeout(resolve, 800));
      const res = await client.request(ControlCommand.TestMarkdownFollowLink, ["", 0]);
      const dump = String(res[1] ?? "");
      const wantSuffix = `file=${join(dir, PDF_NAME)}`;
      const opened = dump
        .split("\n")
        .some((l) => l.startsWith("tab ") && l.includes("current=1") && l.endsWith(wantSuffix));
      if (!opened) {
        throw new Error(`issue-5926: link to '${PDF_NAME}' did not open it as the current tab\noutput:\n${dump}`);
      }
    },
    ["-appdata", appdata, join(dir, MD_NAME)],
  );

  console.log("issue-5926: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
