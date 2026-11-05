// Test for issue #6221: CmdToggleMirror shows pages mirrored left to right, and
// what's on the page is clicked where it's seen.
//
// Page 1 has a black square on its left half that is also a link to page 2.
// Mirrored, the square must be drawn on the right half, and a click there must
// follow the link.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, sleep } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

// page 1: the square at x 40..140 of a 400 pt wide page, a link over it
function makePdf(): string {
  const square = "0 0 0 rg 40 150 100 100 re f";
  const objs: string[] = [];
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  objs[2] = "<< /Type /Pages /Count 2 /Kids [3 0 R 5 0 R] >>";
  objs[3] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 400] /Contents 4 0 R /Annots [6 0 R] >>";
  objs[4] = `<< /Length ${square.length} >>\nstream\n${square}\nendstream`;
  objs[5] = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 400] >>";
  objs[6] = "<< /Type /Annot /Subtype /Link /Rect [40 150 140 250] /Border [0 0 0] /Dest [5 0 R /Fit] >>";
  return assemblePdf(objs.slice(1));
}

// the average x of the dark pixels, in canvas client coordinates
function darkCenterX(canvas: number): { x: number; y: number; n: number } {
  const px = captureWindowPixels(canvas);
  if (!px) {
    return { x: 0, y: 0, n: 0 };
  }
  let sx = 0;
  let sy = 0;
  let n = 0;
  for (let y = 0; y < px.h; y++) {
    for (let x = 0; x < px.w; x++) {
      const i = (y * px.w + x) * 4;
      if (px.data[i] < 40 && px.data[i + 1] < 40 && px.data[i + 2] < 40) {
        sx += x;
        sy += y;
        n++;
      }
    }
  }
  return n ? { x: sx / n, y: sy / n, n } : { x: 0, y: 0, n: 0 };
}

async function currentPage(client: ControlClient): Promise<number> {
  const [, raw] = await client.request(ControlCommand.TestScrollState, []);
  const m = /page=(\d+)/.exec(String(raw ?? ""));
  return m ? Number(m[1]) : 0;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6221");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "issue-6221.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), "RestoreSession = false\nShowToc = false\n");

  // single page, fit page: the page is centered in the canvas
  const { proc, client, frame } = await launchControlled([
    "-appdata",
    dir,
    "-view",
    "single page",
    "-zoom",
    "fit page",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    const midX = captureWindowPixels(canvas)!.w / 2;

    const before = darkCenterX(canvas);
    if (before.n === 0 || before.x >= midX) {
      throw new Error(`issue-6221: the square isn't on the left before mirroring: ${JSON.stringify(before)}`);
    }

    await client.request(ControlCommand.TestInvokeCommand, ["CmdToggleMirror"]);
    let after = before;
    for (let i = 0; i < 50 && after.x < midX; i++) {
      await sleep(100);
      after = darkCenterX(canvas);
    }
    if (after.x <= midX || Math.abs(after.x - midX - (midX - before.x)) > 4) {
      throw new Error(`issue-6221: mirrored square at x=${after.x}, want ${2 * midX - before.x}`);
    }

    // the link is where the square is now
    await clickAt(canvas, Math.round(after.x), Math.round(after.y));
    let page = 0;
    for (let i = 0; i < 30 && page !== 2; i++) {
      await sleep(100);
      page = await currentPage(client);
    }
    if (page !== 2) {
      throw new Error(`issue-6221: clicking the mirrored link left page ${page}, want 2`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
