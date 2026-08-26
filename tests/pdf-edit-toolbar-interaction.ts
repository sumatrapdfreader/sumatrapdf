// Edit PDF mode makes annotations directly interactive: hover outlines them,
// Ctrl+click enters Edit PDF mode and selects the annotation, and a plain
// click in that mode shows a compact property row.

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { assemblePdf, runStandalone, tmpPath } from "./util";
import {
  captureWindowToPng,
  clientToScreen,
  getClientRect,
  MK_CONTROL,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  WM_MOUSEMOVE,
} from "./winapi";
import { clickAt, findCanvas, killAndWait, launchControlled } from "./win-automation";

type AnnotState = {
  screen: { x: number; y: number; dx: number; dy: number };
  screens: { x: number; y: number; dx: number; dy: number }[];
  selected: boolean;
  hover: boolean;
  editToolbar: boolean;
  notification: boolean;
  selectedHover: boolean;
  overlay: {
    visible: boolean;
    rows: number;
    above: boolean;
    rect: { x: number; y: number; dx: number; dy: number } | null;
    anchor: { x: number; y: number; dx: number; dy: number } | null;
  };
  raw: string;
};

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R 5 0 R 6 0 R] >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 680 220 710] " +
      "/QuadPoints [72 710 220 710 72 680 220 680] /C [1 1 0] /CA 0.5 " +
      "/T (Ada) /M (D:20260824123400Z) " +
      "/Contents (A deliberately long annotation comment that must be shortened before it is shown in the hover card) >>",
    "<< /Type /Annot /Subtype /Highlight /P 3 0 R /Rect [72 50 220 80] " +
      "/QuadPoints [72 80 220 80 72 50 220 50] /C [1 1 0] >>",
    "<< /Type /Annot /Subtype /Stamp /P 3 0 R /Rect [350 380 470 500] /Name /Approved >>",
  ];
  return assemblePdf(objs);
}

async function annotState(client: ControlClient): Promise<AnnotState> {
  const deadline = Date.now() + 5_000;
  let raw = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkupAnnots, []);
    raw = String(res[1] ?? "");
    const screen = /screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
    const state = /state selected=(\d+) hover=(\d+) editToolbar=(\d+) notification=(\d+) selectedHover=(\d+)/.exec(raw);
    const overlay =
      /overlay visible=(\d+)(?: rows=(\d+) above=(\d+) rect=(-?\d+),(-?\d+),(\d+),(\d+) anchor=(-?\d+),(-?\d+),(\d+),(\d+))?/.exec(
        raw,
      );
    if (res[0] === 0 && screen && state && overlay) {
      const screens = Array.from(raw.matchAll(/screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g), (m) => ({
        x: +m[1]!,
        y: +m[2]!,
        dx: +m[3]!,
        dy: +m[4]!,
      }));
      return {
        screen: { x: +screen[1]!, y: +screen[2]!, dx: +screen[3]!, dy: +screen[4]! },
        screens,
        selected: state[1] === "1",
        hover: state[2] === "1",
        editToolbar: state[3] === "1",
        notification: state[4] === "1",
        selectedHover: state[5] === "1",
        overlay: {
          visible: overlay[1] === "1",
          rows: +(overlay[2] ?? 0),
          above: overlay[3] === "1",
          rect: overlay[4] ? { x: +overlay[4], y: +overlay[5]!, dx: +overlay[6]!, dy: +overlay[7]! } : null,
          anchor: overlay[8] ? { x: +overlay[8], y: +overlay[9]!, dx: +overlay[10]!, dy: +overlay[11]! } : null,
        },
        raw,
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
    if (!state.hover || state.notification || state.overlay.visible) {
      throw new Error(`pdf-edit-toolbar-interaction: hover showed a Ctrl+click hint or hover card\n${state.raw}`);
    }

    const stampDeadline = Date.now() + 5_000;
    let stamp: { x: number; y: number; dx: number; dy: number } | undefined;
    while (Date.now() < stampDeadline) {
      const m = /type=Stamp[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(state.raw);
      if (m) {
        stamp = { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
        break;
      }
      await sleep(50);
      state = await annotState(client);
    }
    if (!stamp || stamp.dx <= 0 || stamp.dy <= 0) {
      throw new Error(`pdf-edit-toolbar-interaction: stamp screen rect missing\n${state.raw}`);
    }
    await clickAt(canvas, stamp.x + Math.floor(stamp.dx / 2), stamp.y + Math.floor(stamp.dy / 2));
    state = await annotState(client);
    if (state.selected || state.editToolbar) {
      throw new Error(`pdf-edit-toolbar-interaction: click selected a stamp outside Edit PDF mode\n${state.raw}`);
    }

    await clickAt(canvas, centerX, centerY, 200, MK_CONTROL);
    state = await annotState(client);
    if (!state.editToolbar || !state.selected || state.notification) {
      throw new Error(
        `pdf-edit-toolbar-interaction: Ctrl+click did not enter Edit PDF mode ` +
          `(edit=${state.editToolbar} selected=${state.selected} notif=${state.notification})`,
      );
    }
    if (!/annotEditToolbar visible=1 n=\d+ items=.*color.*contents/.test(state.raw)) {
      throw new Error(`pdf-edit-toolbar-interaction: Ctrl+click did not show the compact property row\n${state.raw}`);
    }

    const cr = getClientRect(canvas);
    await clickAt(canvas, Math.max(5, cr.right - 10), Math.max(5, cr.bottom - 10));
    state = await annotState(client);
    if (state.selected || !state.editToolbar) {
      throw new Error(
        `pdf-edit-toolbar-interaction: empty-page click did not deselect while staying in Edit PDF ` +
          `(selected=${state.selected} edit=${state.editToolbar})`,
      );
    }

    await moveAndWaitForHover(client, canvas, Math.max(5, cr.right - 10), Math.max(5, cr.bottom - 10), false);
    state = await annotState(client);
    if (state.overlay.visible) {
      throw new Error("pdf-edit-toolbar-interaction: hover card stayed visible away from annotations");
    }
    const editCenterX = state.screen.x + Math.floor(state.screen.dx / 2);
    const editCenterY = state.screen.y + Math.floor(state.screen.dy / 2);
    state = await moveAndWaitForHover(client, canvas, editCenterX, editCenterY, true);
    if (!state.hover || state.notification || !state.overlay.visible || !state.overlay.rect || !state.overlay.anchor) {
      throw new Error(`pdf-edit-toolbar-interaction: edit-mode hover state is wrong (${JSON.stringify(state)})`);
    }
    for (const row of ["contents", "color", "opacity", "author", "date", "rect"]) {
      if (!state.raw.includes(`row ${row}=`)) {
        throw new Error(`pdf-edit-toolbar-interaction: hover card is missing ${row}\n${state.raw}`);
      }
    }
    const shownContents = /^row contents=(.*)$/m.exec(state.raw)?.[1] ?? "";
    if (!shownContents.endsWith("...") || shownContents.length !== 35) {
      throw new Error(`pdf-edit-toolbar-interaction: contents were not shortened: ${shownContents}`);
    }
    if (!state.raw.includes("row author=Ada") || !state.raw.includes("row date=2026-08-24 12:34 UTC")) {
      throw new Error(`pdf-edit-toolbar-interaction: annotation metadata is wrong\n${state.raw}`);
    }
    if (state.raw.includes("row page=") || /^row (?:date|rect)=.*\.\.\.$/m.test(state.raw)) {
      throw new Error(`pdf-edit-toolbar-interaction: page was shown or Date/Rect was shortened\n${state.raw}`);
    }

    const canvasOrigin = clientToScreen(canvas, 0, 0);
    const topOverlay = state.overlay.rect;
    const topAnnot = state.overlay.anchor;
    if (
      state.overlay.above ||
      topOverlay.x !== canvasOrigin.x + topAnnot.x ||
      topOverlay.y < canvasOrigin.y + topAnnot.y + topAnnot.dy
    ) {
      throw new Error(`pdf-edit-toolbar-interaction: top annotation card is not left-aligned below it\n${state.raw}`);
    }

    const bottomAnnot = state.screens[1];
    if (!bottomAnnot) {
      throw new Error(`pdf-edit-toolbar-interaction: bottom annotation was not loaded\n${state.raw}`);
    }
    state = await moveAndWaitForHover(
      client,
      canvas,
      bottomAnnot.x + Math.floor(bottomAnnot.dx / 2),
      bottomAnnot.y + Math.floor(bottomAnnot.dy / 2),
      true,
    );
    const bottomOverlay = state.overlay.rect;
    const bottomAnchor = state.overlay.anchor;
    const canvasRect = getClientRect(canvas);
    if (
      !state.overlay.visible ||
      !state.overlay.above ||
      !bottomOverlay ||
      !bottomAnchor ||
      bottomOverlay.y + bottomOverlay.dy > canvasOrigin.y + bottomAnchor.y ||
      bottomOverlay.y < canvasOrigin.y ||
      bottomOverlay.y + bottomOverlay.dy > canvasOrigin.y + canvasRect.bottom
    ) {
      throw new Error(
        `pdf-edit-toolbar-interaction: bottom annotation card was not kept above/in the canvas\n${state.raw}`,
      );
    }
    if (state.raw.includes("row contents=") || state.raw.includes("row page=")) {
      throw new Error(`pdf-edit-toolbar-interaction: empty Contents or Page was shown\n${state.raw}`);
    }

    state = await moveAndWaitForHover(client, canvas, editCenterX, editCenterY, true);
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
    if (!state.selected || !state.selectedHover) {
      throw new Error("pdf-edit-toolbar-interaction: plain click did not select the annotation");
    }
    if (!/annotEditToolbar visible=1 n=\d+ items=.*color.*contents/.test(state.raw)) {
      throw new Error(`pdf-edit-toolbar-interaction: compact property row did not appear\n${state.raw}`);
    }

    state = await moveAndWaitForHover(
      client,
      canvas,
      bottomAnnot.x + Math.floor(bottomAnnot.dx / 2),
      bottomAnnot.y + Math.floor(bottomAnnot.dy / 2),
      true,
    );
    await clickAt(
      canvas,
      bottomAnnot.x + Math.floor(bottomAnnot.dx / 2),
      bottomAnnot.y + Math.floor(bottomAnnot.dy / 2),
    );
    state = await annotState(client);
    if (!state.selected || !state.selectedHover) {
      throw new Error("pdf-edit-toolbar-interaction: later click did not select the annotation");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
