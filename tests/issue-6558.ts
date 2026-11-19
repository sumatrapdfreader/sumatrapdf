// #6558: ShowDocumentErrors = false keeps the "Errors in document" notification
// from showing on every open (Show Errors stays in the context menu). And the
// closepath-without-current-point warning that iText-modified journal PDFs give
// on every page is harmless, so it isn't counted as a document error.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, assemblePdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";

function pdfWithContent(stream: string): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R >>",
    `<< /Length ${stream.length} >>\nstream\n${stream}\nendstream`,
  ]);
}

// a broken startxref: MuPDF repairs the file and warns
function brokenPdf(): string {
  return pdfWithContent("0 0 m 100 100 l S").replace(/startxref\s+\d+/, "startxref 999999");
}

type Errors = { errors: boolean; shown: boolean; raw: string };

async function docErrors(client: ControlClient): Promise<Errors> {
  const [code, raw] = await client.request(ControlCommand.TestDocErrors, []);
  const m = /^OK errors=(\d) shown=(\d)/.exec(String(raw ?? ""));
  if (code !== 0 || !m) throw new Error(`issue-6558: TestDocErrors: ${raw}`);
  return { errors: m[1] === "1", shown: m[2] === "1", raw: String(raw) };
}

// the notification comes up once the page is loaded: wait for it, or for long
// enough that it would have
async function settled(client: ControlClient, wantShown: boolean): Promise<Errors> {
  const deadline = Date.now() + 2_000 * SLOW_BUILD_FACTOR;
  let e: Errors;
  do {
    await client.waitForRenderIdle();
    e = await docErrors(client);
    if (wantShown === e.shown && (e.shown || Date.now() > deadline - 1_000)) return e;
    await sleep(100);
  } while (Date.now() < deadline);
  return e;
}

async function check(
  dir: string,
  pdf: string,
  settings: string,
  what: string,
  wantErrors: boolean,
  wantShown: boolean,
) {
  const appdata = join(dir, `appdata-${what}`);
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), settings);
  await withControlledSumatra(
    EXE,
    async (client) => {
      const e = await settled(client, wantShown);
      if (e.errors !== wantErrors || e.shown !== wantShown) {
        throw new Error(`issue-6558: ${what}: want errors=${wantErrors} shown=${wantShown}, got ${e.raw}`);
      }
    },
    ["-appdata", appdata, pdf],
  );
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6558");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const broken = join(dir, "broken.pdf");
  writeFileSync(broken, brokenPdf(), "latin1");
  const closepath = join(dir, "closepath.pdf");
  // `h` first: a closepath with no current point, as iText 7 writes
  writeFileSync(closepath, pdfWithContent("h 0 0 m 100 100 l S"), "latin1");

  await check(dir, broken, "", "default", true, true);
  await check(dir, broken, "ShowDocumentErrors = false\n", "setting-off", true, false);
  await check(dir, closepath, "", "closepath", false, false);
}

if (import.meta.main) {
  await runStandalone(testit);
}
