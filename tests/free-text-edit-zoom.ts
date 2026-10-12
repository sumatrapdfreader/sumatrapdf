// Zooming while the free text in-place editor is open must not change the
// annotation: the box follows the new zoom, so accepting it writes back the
// same page rect. It used to keep its pixel size, and the next click grew the
// annotation to it.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, USE_NG, makeBlankPdf } from "./util.ts";
import { getClientRect, packCoords, sendMessage, WM_COMMAND } from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

type State = { rect: Rect; editing: boolean; editRect: Rect; raw: string };

async function state(client: ControlClient): Promise<State> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /type=FreeText page=1 rect=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(raw);
  const e = /freeTextEdit active=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (res[0] !== 0 || !m || !e) {
    throw new Error(`free-text-edit-zoom: could not read state\n${raw}`);
  }
  return {
    rect: { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! },
    editing: e[1] === "1",
    editRect: { x: +e[2]!, y: +e[3]!, dx: +e[4]!, dy: +e[5]! },
    raw,
  };
}

function sameRect(a: Rect, b: Rect, tol: number): boolean {
  return (
    Math.abs(a.x - b.x) <= tol &&
    Math.abs(a.y - b.y) <= tol &&
    Math.abs(a.dx - b.dx) <= tol &&
    Math.abs(a.dy - b.dy) <= tol
  );
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    console.log("free-text-edit-zoom: skipped on ng (no in-place editor)");
    return;
  }
  const dir = tmpPath("free-text-edit-zoom");
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
    "100",
    pdf,
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));
    await client.waitForRenderIdle();

    // the point form creates the annotation and opens the in-place editor
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(100, 200));
    await client.waitForRenderIdle();
    const created = await state(client);
    if (!created.editing) {
      throw new Error(`free-text-edit-zoom: editor did not open\n${created.raw}`);
    }

    sendCommandSync(frame, cmdId("CmdZoomOut"));
    await client.waitForRenderIdle();
    const zoomed = await state(client);
    if (!zoomed.editing || zoomed.editRect.dx >= created.editRect.dx) {
      throw new Error(
        `free-text-edit-zoom: editor did not shrink with the zoom: was ${JSON.stringify(created.editRect)}\n${zoomed.raw}`,
      );
    }

    // clicking empty page accepts the editor
    const cr = getClientRect(canvas);
    await clickAt(canvas, cr.right - 10, cr.bottom - 10, 0);
    await client.waitForRenderIdle();
    const after = await state(client);
    if (after.editing) {
      throw new Error(`free-text-edit-zoom: editor stayed open\n${after.raw}`);
    }
    if (!sameRect(after.rect, created.rect, 0.01)) {
      throw new Error(
        `free-text-edit-zoom: accepting the editor changed the annotation: was ${JSON.stringify(created.rect)}\n${after.raw}`,
      );
    }
  } finally {
    await killAndWait(proc);
  }

  console.log("free-text-edit-zoom: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
