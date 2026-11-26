// Soft wrapping during in-place editing must match the committed annotation.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { cmdId, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG, makeBlankPdf } from "./util.ts";
import {
  captureWindowPixels,
  enumChildWindows,
  getClassName,
  localWindowPosArg,
  getControlText,
  packCoords,
  sendMessage,
  sendText,
  sleep,
  WM_CHAR,
  WM_COMMAND,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, pressEscape, sendCommandSync } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

const TEXT = "A long sentence with enough words to wrap inside the annotation border. ".repeat(10).trim();

async function selectedRect(client: ControlClient): Promise<Rect> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6604: no selected annotation: ${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function markupDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  return String(res[1] ?? "");
}

async function editActive(client: ControlClient): Promise<boolean> {
  return /freeTextEdit active=1/.test(await markupDump(client));
}

// ng's editor is a gpui text area on the frame, so WM_GETTEXT on that hwnd is
// the window title. The control dump is the text being edited.
async function boxText(client: ControlClient, box: number): Promise<string> {
  if (!USE_NG) {
    return getControlText(box);
  }
  const m = /freeTextEdit active=\d rect=-?\d+,-?\d+,-?\d+,-?\d+ text=([^\r\n]*)/.exec(await markupDump(client));
  return m?.[1] ?? "";
}

async function waitForEdit(client: ControlClient, active: boolean): Promise<void> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  while ((await editActive(client)) !== active) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6604: in-place edit did not become ${active}`);
    }
    await sleep(40);
  }
}

function findBox(canvas: number, pred: (text: string) => boolean): number {
  let found = 0;
  enumChildWindows(canvas, (hwnd) => {
    if (getClassName(hwnd) !== "Edit") {
      return true;
    }
    if (pred(getControlText(hwnd))) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// How many bands of rows inside `r` have dark pixels: one band per rendered
// line of text. The rect's own border is skipped by insetting.
function countTextLines(canvas: number, r: Rect): number {
  const shot = captureWindowPixels(canvas);
  if (!shot) {
    throw new Error("issue-6604: could not read the canvas pixels");
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
  const dir = tmpPath("issue-6604");
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

  // the right half of the work area, as in local runs: with Fit Page a bigger
  // window zooms in, and the box can no longer show the line whole
  const { proc, client, frame } = await launchControlled([
    "-window-pos",
    localWindowPosArg(),
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

    // creating a free text annotation opens the in-place editor on it
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(120, 250));
    await waitForEdit(client, true);
    // ng edits in a gpui text area on the frame. Orig's box is an Edit child.
    const box = USE_NG ? frame : findBox(canvas, (t) => t.startsWith("This is a text"));
    if (!box) {
      throw new Error("issue-6604: placing a free text annotation did not open the editor");
    }

    const initial = await selectedRect(client);

    // Replace the placeholder with one paragraph without explicit newlines.
    sendText(box, TEXT);
    const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
    let shown = await boxText(client, box);
    while (shown !== TEXT) {
      if (Date.now() > deadline) {
        throw new Error(`issue-6604: box text is "${shown}"`);
      }
      await sleep(40);
      shown = await boxText(client, box);
    }

    const live = /freeTextEdit active=1 rect=-?\d+,-?\d+,(\d+),(\d+)/.exec(await markupDump(client));
    if (!live || Math.abs(Number(live[1]) - initial.dx) > 1 || Number(live[2]) <= initial.dy) {
      throw new Error(
        `issue-6604: editing should grow only in height, was ${initial.dx}x${initial.dy}, got ${live?.[0]}`,
      );
    }

    sendMessage(box, WM_CHAR, 0x0a, 0); // Ctrl+Enter
    await waitForEdit(client, false);
    await client.waitForRenderIdle();

    const rendered = await selectedRect(client);
    if (Math.abs(rendered.dx - initial.dx) > 1 || rendered.dy <= initial.dy) {
      throw new Error(`issue-6604: committing should grow only in height, got ${rendered.dx}x${rendered.dy}`);
    }
    // deselect, so the marker and its handles are not counted as text
    await pressEscape(frame);
    await client.waitForRenderIdle();

    if (IS_MAC) {
      console.log("SKIP issue-6604: rendered line count needs a window DC capture");
      return;
    }

    const lines = countTextLines(canvas, rendered);
    if (lines < 2) {
      throw new Error(`issue-6604: expected multiple rendered lines, got ${lines}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6604: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
