// Edit PDF mode: Ctrl+C (CmdCopySelection) copies the selected annotation
// and Ctrl+V (CmdPasteClipboardImage) pastes it with top-left at the mouse.

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { getClientRect, packCoords, sendMessage, sleep, WM_COMMAND } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Square = { x: number; y: number; dx: number; dy: number; w: number; h: number };

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    // no /IC: an unfilled square, so a paste that writes a black fill shows up
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [1 0 0] /BS << /W 2 >> >>",
  ];
  return assemblePdf(objs);
}

function parseSquares(raw: string): Square[] {
  const out: Square[] = [];
  // rect= is in PDF units (the mupdf bounds), screen= in canvas pixels
  const re =
    /type=Square[^\n]*rect=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+),(-?[\d.]+) screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  for (const m of raw.matchAll(re)) {
    out.push({ w: +m[3]!, h: +m[4]!, x: +m[5]!, y: +m[6]!, dx: +m[7]!, dy: +m[8]! });
  }
  return out;
}

async function markupState(
  client: ControlClient,
): Promise<{ raw: string; selected: boolean; annotations: number; squares: Square[] }> {
  const deadline = Date.now() + 5_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const count = /annotations=(\d+)/.exec(raw);
    const state = /state selected=(\d+)/.exec(raw);
    if (res[0] === 0 && count && state) {
      return {
        raw,
        selected: state[1] === "1",
        annotations: +count[1]!,
        squares: parseSquares(raw),
      };
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-copy-paste: could not read markup state\n${raw}`);
    }
    await sleep(50);
  }
}

// number of /IC entries with color components. mupdf writes "/IC[]" for an
// unfilled shape, "/IC[0 0 0]" if the paste gave it a black fill
function countFilledShapes(pdf: string): number {
  let n = 0;
  let i = 0;
  for (;;) {
    i = pdf.indexOf("/IC", i);
    if (i < 0) {
      return n;
    }
    const rest = pdf.slice(i + 3).trimStart();
    if (rest.startsWith("[") && !rest.slice(1).trimStart().startsWith("]")) {
      n++;
    }
    i += 3;
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-copy-paste");
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

    sendCommand(frame, cmdId("CmdToggleEditPDF"));
    await sleep(300);

    let state = await markupState(client);
    if (state.squares.length !== 1 || state.squares[0]!.dx <= 0 || state.squares[0]!.dy <= 0) {
      throw new Error(`annot-copy-paste: expected one square on the page\n${state.raw}`);
    }
    const original = state.squares[0]!;
    await clickAt(canvas, original.x + Math.floor(original.dx / 2), original.y + Math.floor(original.dy / 2));
    state = await markupState(client);
    if (!state.selected) {
      throw new Error(`annot-copy-paste: click did not select the square\n${state.raw}`);
    }

    sendCommand(frame, cmdId("CmdCopySelection"));
    await sleep(100);

    const cr = getClientRect(canvas);
    const paste = {
      x: Math.min(cr.right - 40, original.x + original.dx + 40),
      y: Math.min(cr.bottom - 40, original.y + original.dy + 30),
    };
    sendMessage(frame, WM_COMMAND, cmdId("CmdPasteClipboardImage"), packCoords(paste.x, paste.y));
    await client.waitForRenderIdle();

    const deadline = Date.now() + 5_000;
    for (;;) {
      state = await markupState(client);
      if (state.annotations === 2 && state.squares.length === 2) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(
          `annot-copy-paste: paste did not create a second square ` +
            `(annotations=${state.annotations} squares=${state.squares.length})\n${state.raw}`,
        );
      }
      await sleep(50);
    }

    const pasted = state.squares.find((s) => Math.abs(s.x - original.x) > 8 || Math.abs(s.y - original.y) > 8);
    if (!pasted) {
      throw new Error(`annot-copy-paste: both squares still at the original position\n${state.raw}`);
    }
    const dx = Math.abs(pasted.x - paste.x);
    const dy = Math.abs(pasted.y - paste.y);
    if (dx > 16 || dy > 16) {
      throw new Error(
        `annot-copy-paste: pasted top-left (${pasted.x},${pasted.y}) is not at the mouse ` +
          `(${paste.x},${paste.y})\n${state.raw}`,
      );
    }
    if (Math.abs(pasted.dx - original.dx) > 8 || Math.abs(pasted.dy - original.dy) > 8) {
      throw new Error(
        `annot-copy-paste: pasted size ${pasted.dx}x${pasted.dy} != original ${original.dx}x${original.dy}\n${state.raw}`,
      );
    }
    // exact in PDF units: copying the border-expanded bounds and pasting them back
    // as /Rect would grow the annotation on every copy -> paste round trip
    if (Math.abs(pasted.w - original.w) > 0.05 || Math.abs(pasted.h - original.h) > 0.05) {
      throw new Error(
        `annot-copy-paste: pasted bounds ${pasted.w}x${pasted.h} != original ${original.w}x${original.h}\n${state.raw}`,
      );
    }
    if (!state.selected) {
      throw new Error(`annot-copy-paste: pasted annotation was not selected\n${state.raw}`);
    }

    // save and look at the file: the pasted square must stay unfilled
    sendCommand(frame, cmdId("CmdSaveAnnotations"));
    const saveDeadline = Date.now() + 5_000;
    let saved = "";
    for (;;) {
      saved = readFileSync(pdf, "latin1");
      if (saved.split("/Square").length - 1 === 2) {
        break;
      }
      if (Date.now() > saveDeadline) {
        throw new Error("annot-copy-paste: saved file has no second Square annotation");
      }
      await sleep(100);
    }
    const filled = countFilledShapes(saved);
    if (filled !== 0) {
      throw new Error(`annot-copy-paste: pasted square got an interior color (${filled} filled)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("annot-copy-paste: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
