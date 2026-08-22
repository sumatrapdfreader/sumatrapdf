// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5946
//
// First-open zoom for comic archives:
//   ComicBookUI.DefaultZoom unset -> fit page (not the global DefaultZoom)
//   ComicBookUI.DefaultZoom = fit width -> fit width
// PDFs still follow the global DefaultZoom.
//
// Run:  bun tests/issue-5946.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const CBZ = join(ROOT, "tests", "issue-1201.cbz");

const SETTINGS_HEAD = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
`;

function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
  ];
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (let i = 0; i < objs.length; i++) {
    pdf += off[i]!.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function queryZoom(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestDisplayMode, ["get"]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /OK mode=(.+) presentation=\d+ fullscreen=\d+ zoom=(.+)$/.exec(raw);
      if (!m) {
        throw new Error(`issue-5946: could not parse: ${raw}`);
      }
      return m[2]!;
    }
    if (exitCode !== 2) {
      throw new Error(`issue-5946: TestDisplayMode failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5946: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

async function openZoom(file: string, extra: string, label: string): Promise<string> {
  const dir = tmpPath(`issue-5946-${label}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `${SETTINGS_HEAD}${extra}\n`);
  return withControlledSumatra(EXE, (client) => queryZoom(client), ["-appdata", dir, file]);
}

export async function testit(): Promise<void> {
  if (!existsSync(CBZ)) {
    throw new Error(`issue-5946: missing fixture ${CBZ}`);
  }
  const pdf = tmpPath("issue-5946.pdf");
  writeFileSync(pdf, makePdf());

  const comicUnset = await openZoom(CBZ, "DefaultZoom = fit width", "comic-unset");
  if (comicUnset !== "fit page") {
    throw new Error(
      `issue-5946: comic with DefaultZoom=fit width and ComicBookUI.DefaultZoom unset: want fit page, got ${comicUnset}`,
    );
  }

  const pdfFitWidth = await openZoom(pdf, "DefaultZoom = fit width", "pdf-fit-width");
  if (pdfFitWidth !== "fit width") {
    throw new Error(`issue-5946: PDF with DefaultZoom=fit width: want fit width, got ${pdfFitWidth}`);
  }

  const comicSet = await openZoom(
    CBZ,
    `DefaultZoom = fit page
ComicBookUI [
    DefaultZoom = fit width
]`,
    "comic-fit-width",
  );
  if (comicSet !== "fit width") {
    throw new Error(`issue-5946: comic with ComicBookUI.DefaultZoom=fit width: want fit width, got ${comicSet}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
