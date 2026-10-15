// The annotation comment card holds an Annotation* into the engine. A reload
// replaces the engine; the next repaint repositioned the card through the
// freed annotation and crashed in AnnotationIsLive.
//
// Run: bun tests/annot-text-popup-reload.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, SLOW_BUILD_FACTOR, USE_NG, cmdId, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { FRAME_CLASS, clickAt, findCanvas, sendCommandSync } from "./win-automation.ts";
import { findTopWindow, getClientRect, isWindowVisible, sleep, waitForTopWindow } from "./winapi.ts";

const POPUP_CLASS = "SumatraAnnotTextPopup";
// ng draws the card in the canvas; its overlay probe (ng's wire number) says
// whether it is up
const kNgTestOverlayState = 102 as ControlCommand;

type Rect = { x: number; y: number; dx: number; dy: number };

async function markup(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
}

// canvas coords of the square, once annotation load has published it
function squareScreen(raw: string): Rect | null {
  const m = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (!m) {
    return null;
  }
  const r = { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
  if (r.dx <= 0 || r.dy <= 0) {
    return null;
  }
  return r;
}

// a point on the square that is also inside the canvas
function hitPoint(canvas: number, sq: Rect): { x: number; y: number } {
  const cr = getClientRect(canvas);
  const x0 = Math.max(sq.x, 0);
  const y0 = Math.max(sq.y, 0);
  const x1 = Math.min(sq.x + sq.dx, cr.right);
  const y1 = Math.min(sq.y + sq.dy, cr.bottom);
  return { x: Math.floor((x0 + x1) / 2), y: Math.floor((y0 + y1) / 2) };
}

// orig's card is a window of its own
async function cardShown(client: ControlClient, pid: number): Promise<boolean> {
  if (!USE_NG) {
    const popup = findTopWindow(pid, POPUP_CLASS);
    return popup !== 0 && isWindowVisible(popup);
  }
  const raw = String((await client.request(kNgTestOverlayState, []))[1] ?? "");
  return /annotTextPopup=1/.test(raw);
}

// one page covered by a Square annotation with a comment
function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R >>`,
    `<< /Type /Pages /Kids [3 0 R] /Count 1 >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> /Annots [4 0 R] >>`,
    `<< /Type /Annot /Subtype /Square /Rect [20 20 592 772] /C [1 0 0] /Contents (a comment) >>`,
  ];
  let pdf = "%PDF-1.7\n";
  const off: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    off[i] = pdf.length;
    pdf += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xref = pdf.length;
  const n = objs.length + 1;
  pdf += `xref\n0 ${n}\n0000000000 65535 f \n`;
  for (const o of off) {
    pdf += o.toString().padStart(10, "0") + " 00000 n \n";
  }
  pdf += `trailer\n<< /Size ${n} /Root 1 0 R >>\nstartxref\n${xref}\n%%EOF\n`;
  return pdf;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-text-popup-reload");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();

      // Click the square, not the canvas center. Fit-page and annotation load
      // can both land after the first paint, and a moving cursor turns one
      // click into a drag, so retry until the card is up.
      const canvas = findCanvas(frame);
      let raw = "";
      const found = await pollUntil(
        async () => {
          raw = await markup(client);
          return squareScreen(raw);
        },
        (r): r is Rect => r !== null,
        {
          timeoutMs: 8000 * SLOW_BUILD_FACTOR,
          error: () => `annotation not on screen\n${raw}`,
        },
      );
      let sq = found!;
      const shownBy = Date.now() + 4000 * SLOW_BUILD_FACTOR;
      while (!(await cardShown(client, proc.pid!))) {
        if (Date.now() > shownBy) {
          throw new Error(`comment card did not show\n${raw}`);
        }
        raw = await markup(client);
        sq = squareScreen(raw) ?? sq;
        const pt = hitPoint(canvas, sq);
        await clickAt(canvas, pt.x, pt.y, 0);
        await sleep(50);
      }

      // reload, then repaint: the card must not outlive the old engine
      sendCommandSync(frame, cmdId("CmdReloadDocument"));
      await client.waitForRenderIdle();
      for (let i = 0; i < 5; i++) {
        sendCommandSync(frame, cmdId("CmdScrollDown"));
        await client.waitForRenderIdle();
        await sleep(20);
      }
      if (await cardShown(client, proc.pid!)) {
        throw new Error("comment card still shown after reload");
      }
    },
    ["-zoom", "fit page", pdf],
  );
  rmSync(dir, { recursive: true, force: true });
  console.log("annot-text-popup-reload: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
