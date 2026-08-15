// Regression test for issue #2258: right-to-left facing/book layout must be
// available for PDFs and restored from their per-document state.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sendCommand, waitForFrame } from "./win-automation.ts";

function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R] /Count 2 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
  ];
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

type R2LState = { r2l: number; available: number; raw: string };

async function waitForR2L(client: ControlClient, expected: number): Promise<R2LState> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestDisplayMode, ["r2l"]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    const match = /OK r2l=(\d+) available=(\d+)/.exec(raw);
    if (exitCode === 0 && match && parseInt(match[1]!, 10) === expected) {
      return { r2l: parseInt(match[1]!, 10), available: parseInt(match[2]!, 10), raw };
    }
    if (exitCode !== 2 && !(exitCode === 0 && match)) {
      throw new Error(`issue-2258: TestDisplayMode failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-2258: timed out waiting for r2l=${expected}: ${raw}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-2258.pdf");
  const appData = tmpPath("issue-2258-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  writeFileSync(pdf, makePdf());
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = true
RememberStatePerDocument = true
FileStates [
    [
        FilePath = ${pdf}
        UseDefaultState = false
        DisplayMode = facing
        DisplayR2L = true
    ]
]
`,
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const initial = await waitForR2L(client, 1);
      if (initial.available !== 1) {
        throw new Error(`issue-2258: Toggle Manga Mode is unavailable for a facing PDF: ${initial.raw}`);
      }

      const frame = await waitForFrame(proc.pid);
      sendCommand(frame, cmdId("CmdToggleMangaMode"));
      const toggled = await waitForR2L(client, 0);
      if (toggled.available !== 1) {
        throw new Error(`issue-2258: Toggle Manga Mode became unavailable: ${toggled.raw}`);
      }
    },
    ["-appdata", appData, pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
