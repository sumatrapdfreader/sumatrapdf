// Edit PDF mode: Ctrl+X (CmdCutAnnotation) copies an annotation and marks it
// for deletion; the original goes away when the copy is pasted, so the cut
// annotation moves instead of being duplicated (issue #5222). A second paste
// is a plain copy: only the first one consumes the cut, and the copy is dated
// now rather than inheriting the original's date.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  getClientRect,
  getPopupMenuHandle,
  packCoords,
  postMessage,
  readMenuTree,
  sendMessage,
  sleep,
  setCursorPos,
  VK_ESCAPE,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_MOUSEMOVE,
  type MenuItem,
} from "./winapi.ts";
import {
  clickAt,
  findCanvas,
  killAndWait,
  launchControlled,
  openContextMenu,
  pressEscape,
  sendCommand,
  waitForContextMenu,
} from "./win-automation.ts";

type Square = { x: number; y: number; dx: number; dy: number };

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Annots [4 0 R] >>",
    "<< /Type /Annot /Subtype /Square /P 3 0 R /Rect [72 420 192 540] /C [1 0 0] /IC [1 1 0.6] /BS << /W 2 >> " +
      "/T (Ada) /M (D:20200101000000Z) >>",
  ];
  return assemblePdf(objs);
}

function parseSquares(raw: string): Square[] {
  const out: Square[] = [];
  const re = /type=Square[^\n]*screen=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/g;
  for (const m of raw.matchAll(re)) {
    out.push({ x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! });
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
      throw new Error(`annot-cut-paste: could not read markup state\n${raw}`);
    }
    await sleep(50);
  }
}

// poll until the page holds `want` annotations, else report what it holds
async function waitForAnnotCount(client: ControlClient, want: number, what: string) {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const state = await markupState(client);
    if (state.annotations === want && state.squares.length === want) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(
        `annot-cut-paste: ${what} (annotations=${state.annotations} squares=${state.squares.length}, want ${want})\n${state.raw}`,
      );
    }
    await sleep(50);
  }
}

// the page context menu at a client point of the canvas, then dismiss it
async function contextMenuAt(canvas: number, x: number, y: number): Promise<MenuItem[]> {
  const s = clientToScreen(canvas, x, y);
  openContextMenu(canvas, s.x, s.y);
  const popup = await waitForContextMenu(3000);
  if (!popup) {
    return [];
  }
  const hmenu = getPopupMenuHandle(popup);
  const items = hmenu ? readMenuTree(hmenu) : [];
  postMessage(popup, WM_KEYDOWN, VK_ESCAPE, 0);
  await sleep(150);
  return items;
}

function findMenuItem(items: MenuItem[], text: string): MenuItem | null {
  for (const it of items) {
    if (it.text === text) {
      return it;
    }
    const found = it.items ? findMenuItem(it.items, text) : null;
    if (found) {
      return found;
    }
  }
  return null;
}

// The hover card shows the annotation's date. It is not shown for the selected
// annotation, so deselect first.
async function hoverDate(client: ControlClient, frame: number, canvas: number, x: number, y: number): Promise<string> {
  await pressEscape(frame);
  await sleep(150);
  const s = clientToScreen(canvas, x, y);
  setCursorPos(s.x, s.y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, packCoords(x, y));
  const deadline = Date.now() + 5_000;
  for (;;) {
    const raw = String((await client.request(ControlCommand.TestMarkupAnnots, []))[1] ?? "");
    const m = /date=(\d{4})-\d{2}-\d{2}/.exec(raw);
    if (m) {
      return m[1]!;
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-cut-paste: no hover card date at ${x},${y}
${raw}`);
    }
    await sleep(60);
  }
}

// Ctrl+C / Ctrl+V are bound to CmdCopySelection / CmdPasteClipboardImage,
// which hand off to the annotation commands; the menu still has to advertise
// the key the user presses.
function requireAccel(items: MenuItem[], text: string, accel: string): void {
  const it = findMenuItem(items, text);
  if (!it) {
    throw new Error(`annot-cut-paste: "${text}" is not in the page context menu`);
  }
  if (it.accel !== accel) {
    throw new Error(`annot-cut-paste: "${text}" shows shortcut "${it.accel ?? ""}", want "${accel}"`);
  }
}

function requireEnabled(items: MenuItem[], text: string): void {
  const it = findMenuItem(items, text);
  if (!it) {
    throw new Error(`annot-cut-paste: "${text}" is not in the page context menu`);
  }
  if (it.disabled) {
    throw new Error(`annot-cut-paste: "${text}" is disabled in the page context menu`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-cut-paste");
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
    if (state.squares.length !== 1) {
      throw new Error(`annot-cut-paste: expected one square on the page\n${state.raw}`);
    }
    const original = state.squares[0]!;
    const mid = { x: original.x + Math.floor(original.dx / 2), y: original.y + Math.floor(original.dy / 2) };
    await clickAt(canvas, mid.x, mid.y);
    state = await markupState(client);
    if (!state.selected) {
      throw new Error(`annot-cut-paste: click did not select the square\n${state.raw}`);
    }

    // the context menu offers Cut for the annotation under the cursor, with
    // the shortcuts that reach these commands
    const menu = await contextMenuAt(canvas, mid.x, mid.y);
    requireEnabled(menu, "Cut Annotation");
    requireAccel(menu, "Cut Annotation", "Ctrl + X");
    requireAccel(menu, "Copy Annotation", "Ctrl + C");
    requireAccel(menu, "Paste Annotation", "Ctrl + V");
    for (const name of ["Apply Redactions", "Save changes", "Save to new file", "Discard changes"]) {
      if (!findMenuItem(menu, name)) {
        throw new Error(`annot-cut-paste: "${name}" is not in the Annotations submenu`);
      }
    }
    // nothing has been edited yet and there are no redaction marks
    for (const name of ["Apply Redactions", "Save changes", "Save to new file", "Discard changes"]) {
      if (!findMenuItem(menu, name)!.disabled) {
        throw new Error(`annot-cut-paste: "${name}" must be disabled with nothing to act on`);
      }
    }

    // cut: the annotation must still be there, it goes away on paste
    sendMessage(frame, WM_COMMAND, cmdId("CmdCutAnnotation"), packCoords(mid.x, mid.y));
    await sleep(150);
    state = await markupState(client);
    if (state.annotations !== 1) {
      throw new Error(`annot-cut-paste: cut must not delete the annotation yet\n${state.raw}`);
    }

    // ... and Paste once something has been cut
    requireEnabled(await contextMenuAt(canvas, mid.x, mid.y), "Paste Annotation");

    const cr = getClientRect(canvas);
    const paste = {
      x: Math.min(cr.right - 40, original.x + original.dx + 40),
      y: Math.min(cr.bottom - 40, original.y + original.dy + 30),
    };
    sendMessage(frame, WM_COMMAND, cmdId("CmdPasteAnnotation"), packCoords(paste.x, paste.y));
    await client.waitForRenderIdle();

    state = await waitForAnnotCount(client, 1, "paste of a cut must move the annotation, not copy it");
    const moved = state.squares[0]!;
    if (Math.abs(moved.x - paste.x) > 16 || Math.abs(moved.y - paste.y) > 16) {
      throw new Error(
        `annot-cut-paste: moved square top-left (${moved.x},${moved.y}) is not at the mouse ` +
          `(${paste.x},${paste.y})\n${state.raw}`,
      );
    }
    if (Math.abs(moved.dx - original.dx) > 8 || Math.abs(moved.dy - original.dy) > 8) {
      throw new Error(
        `annot-cut-paste: moved size ${moved.dx}x${moved.dy} != original ${original.dx}x${original.dy}\n${state.raw}`,
      );
    }
    if (!state.selected) {
      throw new Error(`annot-cut-paste: pasted annotation was not selected\n${state.raw}`);
    }

    // the cut is consumed: pasting again copies
    const paste2 = { x: Math.max(8, original.x - 20), y: Math.max(8, original.y - 20) };
    sendMessage(frame, WM_COMMAND, cmdId("CmdPasteAnnotation"), packCoords(paste2.x, paste2.y));
    await client.waitForRenderIdle();
    state = await waitForAnnotCount(client, 2, "second paste of a cut must be a copy");

    // the copy is a new annotation, so it carries today's date and not the
    // 2020 one the square in the file was written with
    const copy = state.squares.reduce((best, sq) =>
      Math.hypot(sq.x - paste2.x, sq.y - paste2.y) < Math.hypot(best.x - paste2.x, best.y - paste2.y) ? sq : best,
    );
    const copyYear = await hoverDate(
      client,
      frame,
      canvas,
      copy.x + Math.floor(copy.dx / 2),
      copy.y + Math.floor(copy.dy / 2),
    );
    const thisYear = String(new Date().getFullYear());
    if (copyYear !== thisYear) {
      throw new Error(`annot-cut-paste: pasted copy is dated ${copyYear}, want ${thisYear}`);
    }
    state = await markupState(client);

    // A pending cut must not outlive the annotation it points at. Cut one and
    // delete it by hand: the paste that follows can only be a copy (an asan
    // build turns a stale pointer here into a use-after-free).
    const doomed = state.squares[0]!;
    const doomedMid = {
      x: doomed.x + Math.floor(doomed.dx / 2),
      y: doomed.y + Math.floor(doomed.dy / 2),
    };
    sendMessage(frame, WM_COMMAND, cmdId("CmdCutAnnotation"), packCoords(doomedMid.x, doomedMid.y));
    await sleep(150);
    sendMessage(frame, WM_COMMAND, cmdId("CmdDeleteAnnotation"), packCoords(doomedMid.x, doomedMid.y));
    await client.waitForRenderIdle();
    await waitForAnnotCount(client, 1, "Delete must remove the cut annotation");
    sendMessage(frame, WM_COMMAND, cmdId("CmdPasteAnnotation"), packCoords(paste.x, paste.y + 60));
    await client.waitForRenderIdle();
    state = await waitForAnnotCount(client, 2, "paste after the cut annotation was deleted must copy");

    // same for a document reload, which frees every annotation of the tab
    const last = state.squares[0]!;
    sendMessage(
      frame,
      WM_COMMAND,
      cmdId("CmdCutAnnotation"),
      packCoords(last.x + Math.floor(last.dx / 2), last.y + Math.floor(last.dy / 2)),
    );
    await sleep(150);
    sendCommand(frame, cmdId("CmdDiscardChanges"));
    await client.waitForRenderIdle();
    await waitForAnnotCount(client, 1, "discarding changes must restore the file's single annotation");
    sendMessage(frame, WM_COMMAND, cmdId("CmdPasteAnnotation"), packCoords(paste.x, paste.y));
    await client.waitForRenderIdle();
    await waitForAnnotCount(client, 2, "paste after a reload must copy");
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("annot-cut-paste: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
