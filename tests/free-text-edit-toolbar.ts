// Two things about editing a selected free text annotation in Edit PDF mode:
// resizing only moves the outline (the annotation is rewritten once, on mouse
// up, because re-laying out its text on every mouse move is slow), and the
// property row ends with a Delete Annotation button.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath, assemblePdf } from "./util.ts";
import {
  clientToScreen,
  findTopWindow,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  MK_LBUTTON,
  WM_COMMAND,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { clickAt, findCanvas, killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

type AnnotState = {
  rect: Rect;
  outline: Rect;
  rerenderPending: boolean;
  raw: string;
};

function makeBlankPdf(): string {
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ];
  return assemblePdf(objects);
}

function parseRect(m: RegExpExecArray | null): Rect {
  if (!m) {
    return { x: 0, y: 0, dx: 0, dy: 0 };
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function annotState(client: ControlClient): Promise<AnnotState> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const rect = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  const outline = / resizeOutline=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (res[0] !== 0 || !rect || !outline) {
    throw new Error(`free-text-edit-toolbar: could not read annotation state: ${raw}`);
  }
  return {
    rect: parseRect(rect),
    outline: parseRect(outline),
    rerenderPending: / resizeRerenderPending=1/.test(raw),
    raw,
  };
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /annotEditToolbar .*/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`free-text-edit-toolbar: could not read toolbar state\n${raw}`);
  }
  return m[0]!;
}

async function annotCount(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /annotations=(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`free-text-edit-toolbar: could not read annotation count\n${raw}`);
  }
  return +m[1]!;
}

function chipRect(dump: string, name: string): Rect {
  const m = new RegExp(`[=;]${name}:(-?\\d+),(-?\\d+),(\\d+),(\\d+)`).exec(dump);
  if (!m) {
    throw new Error(`free-text-edit-toolbar: chip "${name}" not found in ${dump}`);
  }
  return parseRect(m);
}

function sameRect(a: Rect, b: Rect, slack = 0): boolean {
  return (
    Math.abs(a.x - b.x) <= slack &&
    Math.abs(a.y - b.y) <= slack &&
    Math.abs(a.dx - b.dx) <= slack &&
    Math.abs(a.dy - b.dy) <= slack
  );
}

export async function testit(): Promise<void> {
  const dir = tmpPath("free-text-edit-toolbar");
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

    // the point form creates immediately and selects the annotation
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(150, 300));
    await sleep(400);
    await client.waitForRenderIdle();

    const before = await annotState(client);
    if (before.rect.dx <= 0 || before.outline.dx !== 0) {
      throw new Error(`free-text-edit-toolbar: unexpected initial state: ${before.raw}`);
    }

    // drag the bottom-right handle out by 60x40
    const hx = before.rect.x + before.rect.dx;
    const hy = before.rect.y + before.rect.dy;
    const endX = hx + 60;
    const endY = hy + 40;
    const end = clientToScreen(canvas, endX, endY);
    setCursorPos(end.x, end.y);
    sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(endX, endY));
    sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(hx, hy));
    sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(endX, endY));
    await sleep(200);

    const grown = { x: before.rect.x, y: before.rect.y, dx: before.rect.dx + 60, dy: before.rect.dy + 40 };
    // the drag has mouse capture, so a real mouse move landing on the canvas
    // moves the outline somewhere else; re-send ours until it settles
    let midDrag = await annotState(client);
    const settleBy = Date.now() + 3_000;
    while (!sameRect(midDrag.outline, grown, 2) && Date.now() < settleBy) {
      sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(endX, endY));
      await sleep(60);
      midDrag = await annotState(client);
    }
    if (!sameRect(midDrag.rect, before.rect)) {
      throw new Error(
        `free-text-edit-toolbar: annotation was rewritten during the drag: was ${JSON.stringify(before.rect)}\n${midDrag.raw}`,
      );
    }
    if (!sameRect(midDrag.outline, grown, 2)) {
      throw new Error(`free-text-edit-toolbar: outline did not follow the pointer: ${midDrag.raw}`);
    }
    if (midDrag.rerenderPending) {
      throw new Error(`free-text-edit-toolbar: outline-only resize scheduled a re-render: ${midDrag.raw}`);
    }

    sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(endX, endY));
    await sleep(300);
    await client.waitForRenderIdle();

    const after = await annotState(client);
    if (!sameRect(after.rect, grown, 2)) {
      throw new Error(`free-text-edit-toolbar: mouse up did not commit the new size: ${after.raw}`);
    }
    if (after.outline.dx !== 0) {
      throw new Error(`free-text-edit-toolbar: outline outlived the drag: ${after.raw}`);
    }

    // the property row ends with Delete Annotation, after Contents
    const dump = await toolbarDump(client);
    if (!/items=.*,contents,delete /.test(dump)) {
      throw new Error(`free-text-edit-toolbar: delete is not the last chip: ${dump}`);
    }
    // free text leads with the colour of its text, and its chips say what they
    // change rather than just "Color" / "Opacity" / "Border"
    if (!/items=textColor,color,/.test(dump)) {
      throw new Error(`free-text-edit-toolbar: text color is not the first chip: ${dump}`);
    }
    const wantTips: [string, string][] = [
      ["textColor", "Text Color"],
      ["color", "Background Color"],
      ["opacity", "Text Opacity"],
      ["border", "Border Width"],
      ["contents", "Edit text"],
    ];
    for (const [name, tip] of wantTips) {
      const m = new RegExp(`[=;]${name}:-?\\d+,-?\\d+,\\d+,\\d+:([^;\\n]*)`).exec(dump);
      if (!m) {
        throw new Error(`free-text-edit-toolbar: no "${name}" chip: ${dump}`);
      }
      if (m[1] !== tip) {
        throw new Error(`free-text-edit-toolbar: "${name}" tooltip is "${m[1]}", want "${tip}"`);
      }
    }
    if ((await annotCount(client)) !== 1) {
      throw new Error("free-text-edit-toolbar: expected one annotation before deleting");
    }

    const placed = parseRect(/ placed=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(dump));
    const del = chipRect(dump, "delete");
    const tbHwnd = findTopWindow(proc.pid!, "SumatraAnnotEditToolbar");
    if (!tbHwnd) {
      throw new Error("free-text-edit-toolbar: property row window not found");
    }
    await clickAt(tbHwnd, del.x - placed.x + Math.floor(del.dx / 2), del.y - placed.y + Math.floor(del.dy / 2));
    const deadline = Date.now() + 5_000;
    let n = await annotCount(client);
    while (n !== 0 && Date.now() < deadline) {
      await sleep(50);
      n = await annotCount(client);
    }
    if (n !== 0) {
      throw new Error(`free-text-edit-toolbar: delete button did not delete the annotation (n=${n})`);
    }
    if (!/annotEditToolbar visible=0/.test(await toolbarDump(client))) {
      throw new Error("free-text-edit-toolbar: property row stayed up after the annotation was deleted");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("free-text-edit-toolbar: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
