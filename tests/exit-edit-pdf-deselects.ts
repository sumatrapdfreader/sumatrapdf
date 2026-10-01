// Leaving Edit PDF mode leaves no annotation-editing UI behind: the selected
// annotation is deselected (so its marker and resize handles stop being
// painted), its property row goes away, and a half-finished placement is
// cancelled instead of outliving the mode.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

type State = {
  selected: boolean;
  editToolbar: boolean;
  propertyRow: boolean;
  placing: boolean;
  square: Rect | null;
  raw: string;
};

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [1 0 0] /BS << /W 2 >> >>",
  ]);
}

async function state(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const st = /state selected=(\d+) hover=\d+ editToolbar=(\d)/.exec(raw);
  const row = /annotEditToolbar visible=(\d)/.exec(raw);
  if (res[0] !== 0 || !st || !row) {
    throw new Error(`exit-edit-pdf-deselects: could not read state\n${raw}`);
  }
  const sq = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  return {
    selected: st[1] === "1",
    editToolbar: st[2] === "1",
    propertyRow: row[1] === "1",
    // any placement mode still active
    placing: /Placement active=1/.test(raw),
    square: sq ? { x: +sq[1]!, y: +sq[2]!, dx: +sq[3]!, dy: +sq[4]! } : null,
    raw,
  };
}

function sameRect(a: Rect | null, b: Rect | null): boolean {
  return !!a && !!b && a.x === b.x && a.y === b.y && a.dx === b.dx && a.dy === b.dy;
}

function clickSquare(canvas: number, sq: Rect): Promise<void> {
  return clickAt(canvas, sq.x + Math.floor(sq.dx / 2), sq.y + Math.floor(sq.dy / 2), 0);
}

// The Edit PDF toolbar row shrinks the canvas and fit-page zooms again. Two
// matching reads means that move has finished, so the click lands on the square.
async function stableState(client: ControlClient): Promise<State> {
  await client.waitForRenderIdle();
  let prev = await state(client);
  const deadline = Date.now() + 2000;
  for (;;) {
    await sleep(40);
    const cur = await state(client);
    if (sameRect(prev.square, cur.square)) {
      return cur;
    }
    if (Date.now() > deadline) {
      return cur;
    }
    prev = cur;
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("exit-edit-pdf-deselects");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "square.pdf");
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

    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    let s = await stableState(client);
    if (!s.editToolbar || !s.square) {
      throw new Error(`exit-edit-pdf-deselects: Edit PDF mode did not turn on\n${s.raw}`);
    }
    const aimed = s.square;
    await clickSquare(canvas, aimed);
    s = await state(client);
    // one more click if fit-page moved the square after the stable sample
    if ((!s.selected || !s.propertyRow) && s.square && !sameRect(s.square, aimed)) {
      await clickSquare(canvas, s.square);
      s = await state(client);
    }
    if (!s.selected || !s.propertyRow) {
      throw new Error(
        `exit-edit-pdf-deselects: the click did not select the square\naimed=${aimed.x},${aimed.y},${aimed.dx},${aimed.dy}\n${s.raw}`,
      );
    }

    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    s = await state(client);
    if (s.editToolbar) {
      throw new Error(`exit-edit-pdf-deselects: Edit PDF mode did not turn off\n${s.raw}`);
    }
    if (s.selected) {
      throw new Error(`exit-edit-pdf-deselects: the annotation stayed selected\n${s.raw}`);
    }
    if (s.propertyRow) {
      throw new Error(`exit-edit-pdf-deselects: the property row stayed up\n${s.raw}`);
    }

    // a placement started but not finished must not outlive the mode either
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    sendCommandSync(frame, cmdId("CmdCreateAnnotLine"));
    s = await state(client);
    if (!s.placing) {
      throw new Error(`exit-edit-pdf-deselects: line placement did not start\n${s.raw}`);
    }
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    s = await state(client);
    if (s.placing) {
      throw new Error(`exit-edit-pdf-deselects: placement mode outlived Edit PDF mode\n${s.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("exit-edit-pdf-deselects: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
