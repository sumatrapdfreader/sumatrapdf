// Stress: moving a page (and undoing it) while the view and the sidebar's
// thumbnails render. Moving pages rebuilds the engine's per-page state; a
// render already holding the old state used to build its display list from a
// dropped page and crash (fz_bound_text, or mupdf's "This never happens"
// assert in path.c).
//
// Timing-dependent, so it's ad-hoc: a pass is a clean run, not a proof.
// Run: bun tests/ad-hoc-thumbnails-move-race.ts [--no-build] [rounds]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const kPages = 30;

// pages full of stroked and filled text and lines: slow enough to render that
// a move lands in the middle of one
function makeHeavyPdf(): string {
  const objs: string[] = [];
  const pageObj = (i: number) => 4 + i * 2;
  objs[1] = "<< /Type /Catalog /Pages 2 0 R >>";
  const kids = Array.from({ length: kPages }, (_, i) => `${pageObj(i)} 0 R`).join(" ");
  objs[2] = `<< /Type /Pages /Count ${kPages} /Kids [${kids}] >>`;
  objs[3] = "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>";
  for (let i = 0; i < kPages; i++) {
    let content = "";
    for (let line = 0; line < 70; line++) {
      const y = 780 - line * 11;
      content += `BT /F1 9 Tf 2 Tr ${20 + (line % 7)} ${y} Td (page ${i + 1} line ${line} the quick brown fox jumps) Tj ET\n`;
      content += `${20 + line} ${y - 2} m ${590 - line} ${y - 3} l S\n`;
    }
    objs[pageObj(i)] =
      `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 3 0 R >> >> /Contents ${pageObj(i) + 1} 0 R >>`;
    objs[pageObj(i) + 1] = `<< /Length ${content.length} >>\nstream\n${content}endstream`;
  }
  return assemblePdf(objs.slice(1));
}

async function pageEdit(client: ControlClient, args: (string | number)[]): Promise<string> {
  const res = await client.request(ControlCommand.TestPageEdit, args);
  return String(res[1] ?? "");
}

export async function testit(rounds = 40): Promise<void> {
  const dir = tmpPath("thumbnails-move-race");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "heavy.pdf");
  writeFileSync(pdf, makeHeavyPdf(), "latin1");

  const { proc, client, frame } = await launchControlled(["-page", "18", pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await sleep(300);
    for (let i = 0; i < rounds; i++) {
      // page 18 below page 19, as with the mouse
      await pageEdit(client, ["move", "18", 20]);
      await sleep((i % 5) * 7);
      sendCommand(frame, cmdId("CmdUndo"));
      await sleep((i % 3) * 5);
    }
    const s = await pageEdit(client, ["", "", 0]);
    if (!/pages=30/.test(s)) {
      throw new Error(`ad-hoc-thumbnails-move-race: unexpected state: ${s}`);
    }
    console.log(`ad-hoc-thumbnails-move-race: OK (${rounds} rounds)`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  const n = Number(process.argv.find((a) => /^\d+$/.test(a)) ?? 40);
  await runStandalone(() => testit(n));
}
