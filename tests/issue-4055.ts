// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4055
//
// Feature: DefaultDisplayMode = page aspect picks layout and zoom from
// page 1 on first open of a PDF (no remembered FileState):
//
//   taller than wide -> continuous + fit width  (papers)
//   wider than tall  -> single page + fit page  (slides)
//
// Other DefaultDisplayMode values (the default is automatic) keep the
// chosen layout / DefaultZoom.
//
// Run:  bun tests/issue-4055.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SETTINGS_HEAD = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
DefaultZoom = fit page
`;

function makePdf(dx: number, dy: number): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ${dx} ${dy}] /Resources << >> >>`,
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

type ModeState = { mode: string; zoom: string; raw: string };

async function queryMode(client: ControlClient): Promise<ModeState> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestDisplayMode, ["get"]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /OK mode=(.+) presentation=\d+ fullscreen=\d+ zoom=(.+)$/.exec(raw);
      if (!m) {
        throw new Error(`issue-4055: could not parse: ${raw}`);
      }
      return { mode: m[1]!, zoom: m[2]!, raw };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-4055: TestDisplayMode failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-4055: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

function expectView(got: ModeState, mode: string, zoom: string, what: string): void {
  if (got.mode !== mode || got.zoom !== zoom) {
    throw new Error(`issue-4055 ${what}: want mode=${mode} zoom=${zoom}, got ${got.raw}`);
  }
}

async function run(pdf: string, extra: string, label: string): Promise<ModeState> {
  const dir = tmpPath(`issue-4055-${label}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `${SETTINGS_HEAD}${extra}\n`);
  return withControlledSumatra(EXE, (client) => queryMode(client), ["-appdata", dir, pdf]);
}

export async function testit(): Promise<void> {
  const portrait = tmpPath("issue-4055-portrait.pdf");
  const landscape = tmpPath("issue-4055-landscape.pdf");
  writeFileSync(portrait, makePdf(612, 792));
  writeFileSync(landscape, makePdf(792, 612));

  expectView(
    await run(portrait, "DefaultDisplayMode = page aspect", "portrait-on"),
    "continuous",
    "fit width",
    "portrait with page aspect",
  );
  expectView(
    await run(landscape, "DefaultDisplayMode = page aspect", "landscape-on"),
    "single page",
    "fit page",
    "landscape with page aspect",
  );
  expectView(
    await run(portrait, "DefaultDisplayMode = single page", "portrait-off"),
    "single page",
    "fit page",
    "portrait with DefaultDisplayMode = single page",
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
