// What the in-place edit box shows has to be what MuPDF then renders. The box
// is measured with GDI in screen pixels; MuPDF lays free text out in PDF
// points, wrapping at the rect width less 2 * (2 * border width). Sizing the
// box from the pixel measurement alone left the annotation a few points too
// narrow, so a line the box showed whole came back wrapped.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  captureWindowPixels,
  findChildWindow,
  packCoords,
  postMessage,
  sendMessage,
  sleep,
  WM_CHAR,
  WM_COMMAND,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, pressEscape, sendCommand } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

const TEXT = "This is a text... and I'm here for it, with enough words to outgrow the box it started in";

function makeBlankPdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

async function selectedRect(client: ControlClient): Promise<Rect> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`free-text-edit-matches-render: no selected annotation: ${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function editActive(client: ControlClient): Promise<boolean> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  return /freeTextEdit active=1/.test(String(res[1] ?? ""));
}

async function waitForEdit(client: ControlClient, active: boolean): Promise<void> {
  const deadline = Date.now() + 5_000;
  while ((await editActive(client)) !== active) {
    if (Date.now() > deadline) {
      throw new Error(`free-text-edit-matches-render: in-place edit did not become ${active}`);
    }
    await sleep(40);
  }
}

// How many bands of rows inside `r` have dark pixels: one band per rendered
// line of text. The rect's own border is skipped by insetting.
function countTextLines(canvas: number, r: Rect): number {
  const shot = captureWindowPixels(canvas);
  if (!shot) {
    throw new Error("free-text-edit-matches-render: could not read the canvas pixels");
  }
  const inset = 4;
  const x0 = Math.max(0, r.x + inset);
  const x1 = Math.min(shot.w, r.x + r.dx - inset);
  const y0 = Math.max(0, r.y + inset);
  const y1 = Math.min(shot.h, r.y + r.dy - inset);
  let bands = 0;
  let inBand = false;
  let clean = 0;
  for (let y = y0; y < y1; y++) {
    let dark = false;
    for (let x = x0; x < x1; x++) {
      const i = (y * shot.w + x) * 4;
      const lum = (shot.data[i + 2]! * 3 + shot.data[i + 1]! * 6 + shot.data[i]!) / 10;
      if (lum < 128) {
        dark = true;
        break;
      }
    }
    if (dark) {
      if (!inBand) {
        bands++;
        inBand = true;
      }
      clean = 0;
    } else if (inBand && ++clean >= 2) {
      inBand = false;
    }
  }
  return bands;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("free-text-edit-matches-render");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "blank.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makeBlankPdf(), "latin1");
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

    // creating a free text annotation opens the in-place editor on it
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(120, 250));
    await waitForEdit(client, true);
    const box = findChildWindow(canvas, "Edit");
    if (!box) {
      throw new Error("free-text-edit-matches-render: placing a free text annotation did not open the editor");
    }

    // replace the placeholder with one long line
    postMessage(box, WM_CHAR, 1, 0); // Ctrl+A
    await sleep(120);
    for (const ch of TEXT) {
      postMessage(box, WM_CHAR, ch.charCodeAt(0), 0);
    }
    await sleep(400);

    postMessage(box, WM_CHAR, 0x0a, 0); // Ctrl+Enter
    await waitForEdit(client, false);
    await client.waitForRenderIdle();

    const rendered = await selectedRect(client);
    // deselect, so the marker and its handles are not counted as text
    await pressEscape(frame);
    await sleep(400);
    await client.waitForRenderIdle();

    const lines = countTextLines(canvas, rendered);
    if (lines !== 1) {
      throw new Error(
        `free-text-edit-matches-render: the box showed one line, MuPDF rendered ${lines} ` +
          `in ${rendered.dx}x${rendered.dy}`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("free-text-edit-matches-render: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
