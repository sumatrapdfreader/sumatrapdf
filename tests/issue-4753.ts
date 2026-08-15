// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/4753
//
// Feature: a Fullscreen.DisplayMode setting so presentation (Ctrl+L) and
// windowed fullscreen (Shift+Ctrl+L / F11) can use a different page layout
// than the regular window.
//
//   Fullscreen.DisplayMode = facing, DefaultDisplayMode = single page
//     window -> single page
//     presentation / fullscreen -> facing, then back to single page on exit
//
//   Fullscreen.DisplayMode empty (default), DefaultDisplayMode = continuous
//     presentation still forces single page (old behavior)
//     windowed fullscreen keeps continuous
//
// Run:  bun tests/issue-4753.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const SETTINGS_HEAD = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
`;

function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
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

type ModeState = { mode: string; presentation: number; fullscreen: number; raw: string };

async function queryMode(client: ControlClient, action: string): Promise<ModeState> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestDisplayMode, [action]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /OK mode=(.+) presentation=(\d+) fullscreen=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-4753: could not parse: ${raw}`);
      }
      return {
        mode: m[1]!,
        presentation: parseInt(m[2]!, 10),
        fullscreen: parseInt(m[3]!, 10),
        raw,
      };
    }
    if (exitCode !== 2) {
      throw new Error(`issue-4753: TestDisplayMode failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-4753: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

function expectState(got: ModeState, mode: string, presentation: number, fullscreen: number, what: string): void {
  if (got.mode !== mode || got.presentation !== presentation || got.fullscreen !== fullscreen) {
    throw new Error(
      `issue-4753 ${what}: want mode=${mode} presentation=${presentation} fullscreen=${fullscreen}, got ${got.raw}`,
    );
  }
}

async function runWithSettings(pdf: string, extraSettings: string, body: (client: ControlClient) => Promise<void>) {
  const dir = tmpPath(`issue-4753-${extraSettings.includes("facing") ? "facing" : "default"}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), `${SETTINGS_HEAD}${extraSettings}\n`);
  await withControlledSumatra(EXE, body, ["-appdata", dir, pdf]);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-4753.pdf");
  writeFileSync(pdf, makePdf());

  await runWithSettings(
    pdf,
    `DefaultDisplayMode = single page
Fullscreen [
    DisplayMode = facing
]
`,
    async (client) => {
      expectState(await queryMode(client, "get"), "single page", 0, 0, "window with Fullscreen.DisplayMode = facing");
      expectState(await queryMode(client, "presentation"), "facing", 1, 0, "presentation enter");
      expectState(await queryMode(client, "presentation"), "single page", 0, 0, "presentation exit");
      expectState(await queryMode(client, "fullscreen"), "facing", 0, 1, "windowed fullscreen enter");
      expectState(await queryMode(client, "fullscreen"), "single page", 0, 0, "windowed fullscreen exit");
    },
  );

  await runWithSettings(
    pdf,
    `DefaultDisplayMode = continuous
`,
    async (client) => {
      expectState(await queryMode(client, "get"), "continuous", 0, 0, "window with empty Fullscreen.DisplayMode");
      expectState(
        await queryMode(client, "presentation"),
        "single page",
        1,
        0,
        "default presentation still single page",
      );
      expectState(await queryMode(client, "presentation"), "continuous", 0, 0, "default presentation exit");
      expectState(
        await queryMode(client, "fullscreen"),
        "continuous",
        0,
        1,
        "default windowed fullscreen keeps layout",
      );
      expectState(await queryMode(client, "fullscreen"), "continuous", 0, 0, "default windowed fullscreen exit");
    },
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
