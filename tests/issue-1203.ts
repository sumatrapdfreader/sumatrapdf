// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/1203
//
// ClickEdgeToTurnPage: a click (not a drag) on the left fifth of the canvas
// goes to the previous page, the right fifth to the next page.
//
// Run:  bun tests/issue-1203.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { clickAt, findCanvas, waitForFrame } from "./win-automation.ts";
import { getClientRect } from "./winapi.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";

// This test is about geometry -- which fifth of the canvas a click lands in,
// and which page the viewport then reports -- so it pins the window instead of
// taking the runner's layout (a work-area-sized window shows enough of the
// next page that GoToPrevPage leaves the reported page unchanged).
const WINDOW_POS = ["-window-pos", "900x700@40x40"];

const SETTINGS_HEAD = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
ShowToc = false
ShowFavorites = false
DefaultDisplayMode = single page
`;

function makePdf(): Buffer {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>`,
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

async function waitForPage(client: ControlClient, want: number, timeoutMs = 3000 * SLOW_BUILD_FACTOR): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let last = 0;
  while (Date.now() < deadline) {
    last = await queryPage(client);
    if (last === want) {
      return last;
    }
    await new Promise((r) => setTimeout(r, 30));
  }
  return last;
}

async function queryPage(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFavoriteNav, ["page"]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (exitCode === 0) {
      const m = /OK page=(\d+)/.exec(raw);
      if (!m) {
        throw new Error(`issue-1203: could not parse page: ${raw}`);
      }
      return parseInt(m[1]!, 10);
    }
    if (exitCode !== 2) {
      throw new Error(`issue-1203: page query failed: ${raw}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-1203: document never became ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

async function clickCanvasEdge(pid: number, side: "left" | "right" | "middle"): Promise<void> {
  const frame = await waitForFrame(pid);
  if (!frame) {
    throw new Error("issue-1203: no frame");
  }
  const canvas = findCanvas(frame);
  if (!canvas) {
    throw new Error("issue-1203: no canvas");
  }
  const rc = getClientRect(canvas);
  const dx = rc.right - rc.left;
  const dy = rc.bottom - rc.top;
  if (dx < 50 || dy < 50) {
    throw new Error(`issue-1203: canvas too small ${dx}x${dy}`);
  }
  const y = Math.floor(dy / 2);
  let x = Math.floor(dx / 2);
  if (side === "left") {
    x = Math.max(4, Math.floor(dx / 10));
  } else if (side === "right") {
    x = dx - Math.max(4, Math.floor(dx / 10));
  }
  await clickAt(canvas, x, y, 0);
}

async function clickEdgeUntilPage(
  client: ControlClient,
  pid: number,
  side: "left" | "right",
  want: number,
  label: string,
): Promise<void> {
  let last = 0;
  for (let i = 0; i < 3; i++) {
    await clickCanvasEdge(pid, side);
    last = await waitForPage(client, want, 1500 * SLOW_BUILD_FACTOR);
    if (last === want) {
      return;
    }
  }
  throw new Error(`${label}: ${side}-edge click should go to page ${want}, got ${last}`);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-1203.pdf");
  writeFileSync(pdf, makePdf());

  const onDir = tmpPath("issue-1203-on");
  rmSync(onDir, { recursive: true, force: true });
  mkdirSync(onDir, { recursive: true });
  writeFileSync(join(onDir, "SumatraPDF-settings.txt"), `${SETTINGS_HEAD}ClickEdgeToTurnPage = true\n`);

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await client.setNotificationsEnabled(false);
      await client.waitForRenderIdle();
      {
        const page = await waitForPage(client, 1);
        if (page !== 1) {
          throw new Error(`issue-1203 on: expected to start on page 1, got ${page}`);
        }
      }
      await clickEdgeUntilPage(client, proc.pid!, "right", 2, "issue-1203 on");
      await clickEdgeUntilPage(client, proc.pid!, "left", 1, "issue-1203 on");
      await clickCanvasEdge(proc.pid!, "middle");
      if ((await queryPage(client)) !== 1) {
        throw new Error("issue-1203 on: middle click should stay on page 1");
      }
    },
    [...WINDOW_POS, "-appdata", onDir, pdf],
  );

  const offDir = tmpPath("issue-1203-off");
  rmSync(offDir, { recursive: true, force: true });
  mkdirSync(offDir, { recursive: true });
  writeFileSync(join(offDir, "SumatraPDF-settings.txt"), `${SETTINGS_HEAD}ClickEdgeToTurnPage = false\n`);

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await client.setNotificationsEnabled(false);
      await client.waitForRenderIdle();
      {
        const page = await waitForPage(client, 1);
        if (page !== 1) {
          throw new Error(`issue-1203 off: expected to start on page 1, got ${page}`);
        }
      }
      await clickCanvasEdge(proc.pid!, "right");
      if ((await queryPage(client)) !== 1) {
        throw new Error("issue-1203 off: right-edge click should not turn the page");
      }
    },
    [...WINDOW_POS, "-appdata", offDir, pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
