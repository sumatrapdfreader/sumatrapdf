// Edit PDF mode makes annotations directly interactive: hover outlines them,
// a plain click selects them, and the normal Ctrl+click hint stays hidden.

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { cmdId, runStandalone, tmpPath } from "./util";
import {
  captureWindowToPng,
  clientToScreen,
  getClientRect,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  WM_COMMAND,
  WM_MOUSEMOVE,
} from "./winapi";
import { clickAt, findCanvas, killAndWait, launchControlled } from "./win-automation";

type AnnotState = {
  screen: { x: number; y: number; dx: number; dy: number };
  selected: boolean;
  hover: boolean;
  editToolbar: boolean;
  notification: boolean;
};

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 680 220 710] " +
      "/QuadPoints [72 710 220 710 72 680 220 680] /C [1 1 0] >>",
  ];
  let body = "%PDF-1.4\n";
  const offsets: number[] = [];
  for (let i = 0; i < objs.length; i++) {
    offsets.push(body.length);
    body += `${i + 1} 0 obj\n${objs[i]}\nendobj\n`;
  }
  const xrefStart = body.length;
  body += `xref\n0 ${objs.length + 1}\n0000000000 65535 f \n`;
  for (const off of offsets) {
    body += `${off.toString().padStart(10, "0")} 00000 n \n`;
  }
  body += `trailer\n<< /Size ${objs.length + 1} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  return body;
}

async function annotState(client: ControlClient): Promise<AnnotState> {
  const deadline = Date.now() + 5_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const screen = /screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    const state = /state selected=(\d+) hover=(\d+) editToolbar=(\d+) notification=(\d+)/.exec(raw);
    if (res[0] === 0 && screen && state) {
      return {
        screen: { x: +screen[1]!, y: +screen[2]!, dx: +screen[3]!, dy: +screen[4]! },
        selected: state[1] === "1",
        hover: state[2] === "1",
        editToolbar: state[3] === "1",
        notification: state[4] === "1",
      };
    }
    if (Date.now() > deadline) {
      throw new Error(`pdf-edit-toolbar-interaction: annotation never loaded\n${raw}`);
    }
    await sleep(50);
  }
}

function moveMouse(canvas: number, x: number, y: number): void {
  const screen = clientToScreen(canvas, x, y);
  setCursorPos(screen.x, screen.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x, y));
}

async function moveAndWaitForHover(
  client: ControlClient,
  canvas: number,
  x: number,
  y: number,
  expected: boolean,
): Promise<AnnotState> {
  const deadline = Date.now() + 2_000;
  let state: AnnotState;
  for (;;) {
    moveMouse(canvas, x, y);
    await sleep(40);
    state = await annotState(client);
    if (state.hover === expected) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(`pdf-edit-toolbar-interaction: hover did not become ${expected} (${JSON.stringify(state)})`);
    }
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("pdf-edit-toolbar-interaction");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "highlight.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "RestoreSession = false\nShowStartPage = false\nShowAnnotationNotification = true\n",
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
    const canvas = findCanvas(frame);
    let state = await annotState(client);
    const centerX = state.screen.x + Math.floor(state.screen.dx / 2);
    const centerY = state.screen.y + Math.floor(state.screen.dy / 2);
    state = await moveAndWaitForHover(client, canvas, centerX, centerY, true);
    if (!state.hover || !state.notification) {
      throw new Error(`pdf-edit-toolbar-interaction: normal hover did not show the annotation hint`);
    }

    sendMessage(frame, WM_COMMAND, cmdId("CmdTogglePdfAnnotationsToolbar"), 0);
    await sleep(300);
    await client.waitForRenderIdle();
    state = await annotState(client);
    if (!state.editToolbar || state.notification) {
      throw new Error(
        `pdf-edit-toolbar-interaction: Edit PDF mode state is wrong ` +
          `(edit=${state.editToolbar} notification=${state.notification})`,
      );
    }

    const cr = getClientRect(canvas);
    await moveAndWaitForHover(client, canvas, Math.max(5, cr.right - 10), Math.max(5, cr.bottom - 10), false);
    state = await annotState(client);
    const editCenterX = state.screen.x + Math.floor(state.screen.dx / 2);
    const editCenterY = state.screen.y + Math.floor(state.screen.dy / 2);
    state = await moveAndWaitForHover(client, canvas, editCenterX, editCenterY, true);
    if (!state.hover || state.notification) {
      throw new Error(`pdf-edit-toolbar-interaction: edit-mode hover state is wrong (${JSON.stringify(state)})`);
    }

    // Remove unrelated startup/zoom notifications before comparing pixels.
    await client.setNotificationsEnabled(false);
    await moveAndWaitForHover(client, canvas, Math.max(5, cr.right - 10), Math.max(5, cr.bottom - 10), false);
    const plainPng = join(dir, "plain.png");
    if (!captureWindowToPng(canvas, plainPng)) {
      throw new Error("pdf-edit-toolbar-interaction: plain capture failed");
    }
    await moveAndWaitForHover(client, canvas, editCenterX, editCenterY, true);
    const hoverPng = join(dir, "hover.png");
    if (!captureWindowToPng(canvas, hoverPng)) {
      throw new Error("pdf-edit-toolbar-interaction: hover capture failed");
    }
    if (readFileSync(plainPng).equals(readFileSync(hoverPng))) {
      throw new Error("pdf-edit-toolbar-interaction: hover did not draw an annotation bounding box");
    }

    await clickAt(canvas, editCenterX, editCenterY);
    state = await annotState(client);
    if (!state.selected) {
      throw new Error("pdf-edit-toolbar-interaction: plain click did not select the annotation");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
