// Which annotations can be moved and resized follows the PDF spec and mupdf:
// /Rect is required on every annotation, and mupdf's rect_subtypes (what
// pdf_annot_has_rect() answers yes to) is the set whose position is theirs to
// change. RichMedia and 3D are in that set; our list used to leave them out.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  MK_LBUTTON,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Sel = { rect: { x: number; y: number; dx: number; dy: number }; canResize: boolean; raw: string };

// PDF rects, all clear of each other so a click picks the one we mean
const SQUARE = [40, 40, 140, 140];
const CASES: [string, number[]][] = [
  ["RichMedia", [200, 500, 400, 700]],
  ["3D", [200, 200, 400, 400]],
];

function makePdf(): string {
  const annots = ["4 0 R", "5 0 R", "6 0 R"];
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${annots.join(" ")}] >>`,
    `<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [${SQUARE.join(" ")}] /C [1 0 0] /BS << /W 1 >> >>`,
    `<< /Type /Annot /Subtype /RichMedia /P 3 0 R /Rect [${CASES[0]![1].join(" ")}] /F 4 >>`,
    `<< /Type /Annot /Subtype /3D /P 3 0 R /Rect [${CASES[1]![1].join(" ")}] /F 4 >>`,
  ]);
}

async function selected(client: ControlClient): Promise<Sel | null> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+) canResize=(\d)/.exec(raw);
  if (!m) {
    return null;
  }
  return {
    rect: { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! },
    canResize: m[5] === "1",
    raw,
  };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-moveable-types");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "media.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nRestoreSession = false\nShowStartPage = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    appdata,
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
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    // the square tells us where the page is on screen; neither RichMedia nor
    // 3D is in the markup-annots dump
    const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    const m = /type=Square page=1 rect=\S+ screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    if (!m) {
      throw new Error(`annot-moveable-types: no square to anchor on\n${raw}`);
    }
    const scale = +m[4]! / (SQUARE[3]! - SQUARE[1]!);
    const ox = +m[1]! - SQUARE[0]! * scale;
    const oy = +m[2]! - (792 - SQUARE[3]!) * scale;

    for (const [name, r] of CASES) {
      const x = Math.round(ox + r[0]! * scale) + 50;
      const y = Math.round(oy + (792 - r[3]!) * scale) + 50;
      await clickAt(canvas, x, y);
      await sleep(250);
      const before = await selected(client);
      if (!before) {
        throw new Error(`annot-moveable-types: a ${name} annotation could not be selected`);
      }
      if (!before.canResize) {
        throw new Error(`annot-moveable-types: a ${name} annotation is not resizable: ${before.raw}`);
      }

      const to = { x: x + 40, y: y + 30 };
      const sp = clientToScreen(canvas, to.x, to.y);
      setCursorPos(sp.x, sp.y);
      sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x, y));
      sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
      sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(to.x, to.y));
      sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(to.x, to.y));
      await sleep(300);
      await client.waitForRenderIdle();

      const after = await selected(client);
      if (!after) {
        throw new Error(`annot-moveable-types: the ${name} annotation lost its selection`);
      }
      const dx = after.rect.x - before.rect.x;
      const dy = after.rect.y - before.rect.y;
      if (Math.abs(dx - 40) > 2 || Math.abs(dy - 30) > 2) {
        throw new Error(
          `annot-moveable-types: dragging a ${name} annotation moved it by ${dx},${dy}, want 40,30: ${after.raw}`,
        );
      }
      if (Math.abs(after.rect.dx - before.rect.dx) > 2 || Math.abs(after.rect.dy - before.rect.dy) > 2) {
        throw new Error(`annot-moveable-types: the drag resized the ${name} annotation: ${after.raw}`);
      }
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("annot-moveable-types: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
