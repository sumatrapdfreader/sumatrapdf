// Polygon, polyline and ink annotations are stored as Vertices / InkList, not a
// user-settable /Rect. Other PDF editors still let you drag them; Sumatra now
// translates that geometry on move. Dragging an annotation is an Edit PDF mode
// gesture, so the test turns that mode on first.
//
// Run: bun tests/move-polygon-polyline-ink.ts [--no-build]

import { writeFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, assemblePdf } from "./util.ts";
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
import { findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const DRAG = 48;

type ShapeDump = {
  type: string;
  page: number;
  rect: { x: number; y: number; dx: number; dy: number };
  screen: { x: number; y: number; dx: number; dy: number };
};

function makePdf(): string {
  const polygon =
    "<< /Type /Annot /Subtype /Polygon /Rect [50 500 150 620] /Vertices [50 500 150 500 100 620] " +
    "/C [1 0 0] /Border [0 0 2] /P 3 0 R >>";
  const polyline =
    "<< /Type /Annot /Subtype /PolyLine /Rect [230 500 330 620] /Vertices [230 500 330 560 230 620] " +
    "/C [0 0 1] /Border [0 0 2] /P 3 0 R >>";
  const ink =
    "<< /Type /Annot /Subtype /Ink /Rect [430 500 530 620] /InkList [[430 500 480 560 530 500 480 620]] " +
    "/C [0 0.5 0] /Border [0 0 2] /P 3 0 R >>";
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [${polygon} ${polyline} ${ink}] >>`,
  ];
  return assemblePdf(objs);
}

function parseShapes(dump: string): ShapeDump[] {
  const out: ShapeDump[] = [];
  for (const line of dump.split("\n")) {
    const m =
      /^type=(\S+) page=(\d+) rect=([^,]+),([^,]+),([^,]+),([^,]+) screen=([^,]+),([^,]+),([^,]+),([^,]+)$/.exec(
        line.trim(),
      );
    if (!m) {
      continue;
    }
    out.push({
      type: m[1]!,
      page: +m[2]!,
      rect: { x: +m[3]!, y: +m[4]!, dx: +m[5]!, dy: +m[6]! },
      screen: { x: +m[7]!, y: +m[8]!, dx: +m[9]!, dy: +m[10]! },
    });
  }
  return out;
}

async function dumpShapes(client: ControlClient): Promise<{ raw: string; shapes: ShapeDump[] }> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    const raw = String(res[1] ?? "");
    const shapes = parseShapes(raw);
    if (shapes.length >= 3) {
      return { raw, shapes };
    }
    if (Date.now() > deadline) {
      throw new Error(`move-polygon-polyline-ink: expected 3 shape annots\n${raw}`);
    }
    await sleep(50);
  }
}

function requireShape(shapes: ShapeDump[], type: string, raw: string): ShapeDump {
  const s = shapes.find((a) => a.type === type);
  if (!s) {
    throw new Error(`move-polygon-polyline-ink: no ${type}\n${raw}`);
  }
  if (s.screen.dx < 4 || s.screen.dy < 4) {
    throw new Error(`move-polygon-polyline-ink: ${type} has empty screen rect ${JSON.stringify(s.screen)}\n${raw}`);
  }
  return s;
}

async function dragAnnot(canvas: number, s: ShapeDump): Promise<void> {
  const x0 = s.screen.x + Math.floor(s.screen.dx / 2);
  const y0 = s.screen.y + Math.floor(s.screen.dy / 2);
  const x1 = x0 + DRAG;
  const y1 = y0 + DRAG;
  const p0 = clientToScreen(canvas, x0, y0);
  const p1 = clientToScreen(canvas, x1, y1);
  setCursorPos(p0.x, p0.y);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x0, y0));
  setCursorPos(p1.x, p1.y);
  sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x1, y1));
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(x1, y1));
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("move-polygon-polyline-ink.pdf");
  writeFileSync(pdf, makePdf());
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("move-polygon-polyline-ink: no canvas");
    }

    // outside Edit PDF mode a click on an annotation stays a page click
    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    const before = await dumpShapes(client);
    for (const type of ["Polygon", "PolyLine", "Ink"]) {
      const start = requireShape(before.shapes, type, before.raw);
      await dragAnnot(canvas, start);
      await client.waitForRenderIdle();
      const after = await dumpShapes(client);
      const moved = requireShape(after.shapes, type, after.raw);
      const dx = moved.screen.x - start.screen.x;
      const dy = moved.screen.y - start.screen.y;
      if (Math.abs(dx - DRAG) > 8 || Math.abs(dy - DRAG) > 8) {
        throw new Error(
          `move-polygon-polyline-ink: ${type} did not move by ${DRAG}px ` +
            `(dx=${dx}, dy=${dy})\nbefore=${JSON.stringify(start)}\nafter=${JSON.stringify(moved)}\n${after.raw}`,
        );
      }
    }
    console.log("move-polygon-polyline-ink: polygon, polyline and ink moved ✓");
    sendCommand(frame, cmdId("CmdDiscardChanges"));
    await client.waitForRenderIdle();
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
