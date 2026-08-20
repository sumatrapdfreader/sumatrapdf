// Regression test for issue #2022: PDF Catalog /PageLayout and
// /ViewerPreferences /Direction must be honored on first open, and a
// remembered DisplayR2L = false must not clobber a document that states R2L.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

type PdfOpts = {
  layout?: string;
  direction?: string;
  pages?: number;
};

function makePdf(opts: PdfOpts = {}): Buffer {
  const pages = opts.pages ?? 3;
  const kids: string[] = [];
  const objs: string[] = [];
  let catalog = "<< /Type /Catalog /Pages 2 0 R";
  if (opts.layout) {
    catalog += ` /PageLayout /${opts.layout}`;
  }
  if (opts.direction) {
    catalog += " /ViewerPreferences << /Direction /" + opts.direction + " >>";
  }
  catalog += " >>";
  objs.push(catalog);
  const firstPage = 3;
  for (let i = 0; i < pages; i++) {
    kids.push(`${firstPage + i} 0 R`);
  }
  objs.push(`<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${pages} >>`);
  for (let i = 0; i < pages; i++) {
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`);
  }
  let pdf = "%PDF-1.7\n%\xe2\xe3\xcf\xd3\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets[i] = Buffer.byteLength(pdf, "latin1");
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = Buffer.byteLength(pdf, "latin1");
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const offset of offsets) {
    pdf += `${offset.toString().padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return Buffer.from(pdf, "latin1");
}

async function waitDisplay(
  client: ControlClient,
): Promise<{ mode: string; r2l: number; rawMode: string; rawR2l: string }> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const modeRes = await client.request(ControlCommand.TestDisplayMode, ["get"]);
    const r2lRes = await client.request(ControlCommand.TestDisplayMode, ["r2l"]);
    const modeCode = modeRes[0] as number;
    const r2lCode = r2lRes[0] as number;
    const rawMode = String(modeRes[1] ?? "").trim();
    const rawR2l = String(r2lRes[1] ?? "").trim();
    const modeMatch = /OK mode=(.+) presentation=\d+ fullscreen=\d+ zoom=(.+)$/.exec(rawMode);
    const r2lMatch = /OK r2l=(\d+) available=(\d+)/.exec(rawR2l);
    if (modeCode === 0 && r2lCode === 0 && modeMatch && r2lMatch) {
      return { mode: modeMatch[1]!, r2l: parseInt(r2lMatch[1]!, 10), rawMode, rawR2l };
    }
    if (modeCode !== 2 && modeCode !== 0) {
      throw new Error(`issue-2022: TestDisplayMode get failed: ${rawMode}`);
    }
    if (r2lCode !== 2 && r2lCode !== 0) {
      throw new Error(`issue-2022: TestDisplayMode r2l failed: ${rawR2l}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-2022: timed out waiting for display: mode=${rawMode} r2l=${rawR2l}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

async function openWithSettings(
  pdf: string,
  settings: string,
  label: string,
): Promise<{ mode: string; r2l: number; rawMode: string; rawR2l: string }> {
  const dir = tmpPath(`issue-2022-${label}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings);
  return withControlledSumatra(EXE, (client) => waitDisplay(client), ["-appdata", dir, pdf]);
}

const SETTINGS_HEAD = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
DefaultDisplayMode = automatic
`;

export async function testit(): Promise<void> {
  const r2lRight = tmpPath("issue-2022-r2l-right.pdf");
  const l2rLeft = tmpPath("issue-2022-l2r-left.pdf");
  const none = tmpPath("issue-2022-none.pdf");
  writeFileSync(r2lRight, makePdf({ layout: "TwoColumnRight", direction: "R2L" }));
  writeFileSync(l2rLeft, makePdf({ layout: "TwoColumnLeft", direction: "L2R" }));
  writeFileSync(none, makePdf({}));

  const mangaOff = `${SETTINGS_HEAD}ComicBookUI [
    CbxMangaMode = false
]
`;
  const mangaOn = `${SETTINGS_HEAD}ComicBookUI [
    CbxMangaMode = true
]
`;

  const gotR2L = await openWithSettings(r2lRight, mangaOff, "r2l-first");
  if (gotR2L.r2l !== 1) {
    throw new Error(`issue-2022 R2L first open: want r2l=1, got ${gotR2L.rawR2l}`);
  }
  if (gotR2L.mode !== "continuous book view") {
    throw new Error(`issue-2022 TwoColumnRight: want continuous book view, got ${gotR2L.rawMode}`);
  }

  const gotL2R = await openWithSettings(l2rLeft, mangaOn, "l2r-manga-on");
  if (gotL2R.r2l !== 0) {
    throw new Error(`issue-2022 L2R must beat CbxMangaMode: want r2l=0, got ${gotL2R.rawR2l}`);
  }
  if (gotL2R.mode !== "continuous facing") {
    throw new Error(`issue-2022 TwoColumnLeft: want continuous facing, got ${gotL2R.rawMode}`);
  }

  const gotNone = await openWithSettings(none, mangaOn, "none-manga-on");
  if (gotNone.r2l !== 1) {
    throw new Error(`issue-2022 undeclared + CbxMangaMode: want r2l=1, got ${gotNone.rawR2l}`);
  }

  const rememberedOff = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = true
RememberStatePerDocument = true
DefaultDisplayMode = automatic
FileStates [
    [
        FilePath = ${r2lRight}
        UseDefaultState = false
        DisplayMode = continuous book view
        DisplayR2L = false
    ]
]
`;
  const gotRemembered = await openWithSettings(r2lRight, rememberedOff, "r2l-remembered-off");
  if (gotRemembered.r2l !== 1) {
    throw new Error(
      `issue-2022 remembered DisplayR2L=false must not clobber catalog R2L: want r2l=1, got ${gotRemembered.rawR2l}`,
    );
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
