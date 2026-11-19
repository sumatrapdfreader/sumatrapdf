// Test for PR #6522: changing the layout while a smooth wheel scroll is still
// animating must not leave the view blank. The animation chased a y offset
// computed for the old layout; in a non-continuous view ScrollYTo doesn't
// clamp, so the page ended up far off screen until the next input.
//
// Scrolls a few wheel notches, then (while the animation runs) toggles
// Continuous or zooms, and checks the page it was on is still on screen.

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra, type ControlClient } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";
import { findCanvas, waitForFrame } from "./win-automation.ts";
import { getWindowRect, packCoords, postMessage, sleep } from "./winapi.ts";

const WM_MOUSEWHEEL = 0x020a;
const WHEEL_DELTA = 120;
const kPageCount = 30;
// first scroll away from the top, so a stale target lies outside the page
const kPreScrollNotches = 20;
const kWheelNotches = 6;

// kPageCount tall pages, each with a different shade so a blank view is obvious
function buildPdf(): Uint8Array {
  const objs: string[] = ["<< /Type /Catalog /Pages 2 0 R >>", ""];
  const kids: string[] = [];
  for (let i = 0; i < kPageCount; i++) {
    const pageObj = objs.length + 1;
    const content = `0 0 ${(i % 10) / 10} rg 50 50 500 700 re f`;
    kids.push(`${pageObj} 0 R`);
    objs.push(`<< /Type /Page /Parent 2 0 R /MediaBox [0 0 600 800] /Contents ${pageObj + 1} 0 R >>`);
    objs.push(`<< /Length ${content.length} >>\nstream\n${content}\nendstream`);
  }
  objs[1] = `<< /Type /Pages /Kids [${kids.join(" ")}] /Count ${kPageCount} >>`;
  let pdf = "%PDF-1.4\n";
  const offsets: number[] = [];
  objs.forEach((o, i) => {
    offsets.push(pdf.length);
    pdf += `${i + 1} 0 obj\n${o}\nendobj\n`;
  });
  const xref = pdf.length;
  pdf += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    pdf += `${String(off).padStart(10, "0")} 00000 n \n`;
  }
  pdf += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return new TextEncoder().encode(pdf);
}

// "OK page=3 x=0 y=12.5 visible=2" => { page: 3, visible: 2 }
async function scrollState(client: ControlClient): Promise<{ page: number; visible: number }> {
  const [, raw] = await client.request(ControlCommand.TestScrollState, []);
  const s = String(raw ?? "").trim();
  const m = /page=(\d+) .* visible=(\d+)/.exec(s);
  if (!s.startsWith("OK ") || !m) {
    throw new Error(`issue-6522: TestScrollState: ${s}`);
  }
  return { page: Number(m[1]), visible: Number(m[2]) };
}

async function invoke(client: ControlClient, cmd: string): Promise<void> {
  await client.request(ControlCommand.TestInvokeCommand, [cmd]);
}

function wheelDown(canvas: number, notches: number): void {
  const r = getWindowRect(canvas);
  const x = Math.round((r.left + r.right) / 2);
  const y = Math.round((r.top + r.bottom) / 2);
  for (let i = 0; i < notches; i++) {
    postMessage(canvas, WM_MOUSEWHEEL, (-WHEEL_DELTA << 16) >>> 0, packCoords(x, y));
  }
}

// setup puts the view in its starting layout; change is the command sent while
// the wheel scroll animates
type Case = { name: string; setup: string[]; change: string };

const cases: Case[] = [
  { name: "continuous off", setup: [], change: "CmdToggleContinuousView" },
  { name: "zoom to fit page", setup: ["CmdSinglePageView", "CmdZoom400"], change: "CmdZoomFitPage" },
];

// the change lands at different points of the animation, which takes ~0.5s
const kDelaysMs = [0, 50, 150];

async function runCase(pdfPath: string, appdata: string, c: Case, delayMs: number): Promise<string | null> {
  return await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await client.setNotificationsEnabled(false);
      await client.waitForRenderIdle();
      for (const cmd of c.setup) {
        await invoke(client, cmd);
      }
      await client.waitForRenderIdle();

      const frame = await waitForFrame(proc.pid);
      const canvas = findCanvas(frame);
      if (!canvas) {
        throw new Error("issue-6522: no canvas");
      }
      // waitForRenderIdle also waits for the smooth scroll to settle
      wheelDown(canvas, kPreScrollNotches);
      await sleep(100);
      await client.waitForRenderIdle();
      const before = await scrollState(client);

      wheelDown(canvas, kWheelNotches);
      await sleep(delayMs);
      await invoke(client, c.change);
      await client.waitForRenderIdle();
      const after = await scrollState(client);

      const where = `${c.name} (change after ${delayMs} ms)`;
      if (after.visible === 0) {
        return `${where}: no page on screen`;
      }
      // the notches can reach the next page before the change lands, not further
      if (after.page < before.page || after.page > before.page + 1) {
        return `${where}: moved from page ${before.page} to ${after.page}`;
      }
      return null;
    },
    ["-appdata", appdata, pdfPath],
  );
}

export async function testit(): Promise<void> {
  const pdfPath = tmpPath("issue-6522.pdf");
  writeFileSync(pdfPath, buildPdf());
  const appdata = tmpPath("issue-6522-appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), "SmoothScroll = true\n");

  const failures: string[] = [];
  for (const c of cases) {
    for (const delayMs of kDelaysMs) {
      const failure = await runCase(pdfPath, appdata, c, delayMs);
      if (failure) {
        failures.push(failure);
      }
    }
  }
  if (failures.length > 0) {
    throw new Error(`issue-6522:\n  ${failures.join("\n  ")}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
