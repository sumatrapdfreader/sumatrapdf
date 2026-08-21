// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/2165
//
// Feature: SidebarOnRight puts the bookmarks / favorites sidebar on the right
// of the window (left is the default).
//
// Run:  bun tests/issue-2165.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SETTINGS_HEAD = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
ShowFavorites = true
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

type Layout = {
  pref: number;
  favVis: number;
  favX: number;
  canvasX: number;
  raw: string;
};

async function queryLayout(client: ControlClient): Promise<Layout> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestSidebarLayout, []);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /OK pref=(\d+) tocVis=\d+ favVis=(\d+) tocX=-?\d+ favX=(-?\d+) canvasX=(-?\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-2165: could not parse: ${raw}`);
      }
      const layout: Layout = {
        pref: parseInt(m[1]!, 10),
        favVis: parseInt(m[2]!, 10),
        favX: parseInt(m[3]!, 10),
        canvasX: parseInt(m[4]!, 10),
        raw,
      };
      // Fav and canvas can share a rect for a frame before RelayoutFrame
      // separates them (SidebarOnRight); that used to fail the side check.
      if (layout.favVis === 1 && layout.favX >= 0 && layout.canvasX >= 0 && layout.favX !== layout.canvasX) {
        return layout;
      }
    } else if (exitCode !== 2) {
      throw new Error(`issue-2165: TestSidebarLayout failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-2165: sidebar never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

async function runWithSettings(pdf: string, extraSettings: string, body: (client: ControlClient) => Promise<void>) {
  const dir = tmpPath(`issue-2165-${extraSettings.includes("true") ? "right" : "left"}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `${SETTINGS_HEAD}${extraSettings}\n`);
  await withControlledSumatra(EXE, body, ["-appdata", dir, pdf]);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-2165.pdf");
  writeFileSync(pdf, makePdf());

  await runWithSettings(pdf, `SidebarOnRight = false`, async (client) => {
    const got = await queryLayout(client);
    if (got.pref !== 0) {
      throw new Error(`issue-2165 default: want pref=0, got ${got.raw}`);
    }
    if (got.favX >= got.canvasX) {
      throw new Error(`issue-2165 default: favorites should be left of canvas, got ${got.raw}`);
    }
  });

  await runWithSettings(pdf, `SidebarOnRight = true`, async (client) => {
    const got = await queryLayout(client);
    if (got.pref !== 1) {
      throw new Error(`issue-2165 right: want pref=1, got ${got.raw}`);
    }
    if (got.favX <= got.canvasX) {
      throw new Error(`issue-2165 right: favorites should be right of canvas, got ${got.raw}`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
