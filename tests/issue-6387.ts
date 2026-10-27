// With one free text selected in Edit PDF, double-clicking another free text
// edits it in place. The first click only deselected and the double-click was
// dropped, so the edit opened only on a second double-click.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makeBlankPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import {
  clientToScreen,
  enumChildWindows,
  getClassName,
  getClientRect,
  getControlText,
  packCoords,
  sendMessage,
  setCursorPos,
  sleep,
  MK_LBUTTON,
  VK_ESCAPE,
  WM_COMMAND,
  WM_KEYDOWN,
  WM_KEYUP,
  WM_LBUTTONDBLCLK,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

async function editState(client: ControlClient): Promise<{ active: boolean; rect: Rect; raw: string }> {
  const res = await client.request(ControlCommand.TestMarkupAnnots, []);
  const raw = String(res[1] ?? "");
  const m = /freeTextEdit active=(\d) rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6387: no freeTextEdit line\n${raw}`);
  }
  return { active: m[1] === "1", rect: { x: +m[2]!, y: +m[3]!, dx: +m[4]!, dy: +m[5]! }, raw };
}

async function waitForEdit(client: ControlClient, active: boolean, what: string) {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const s = await editState(client);
    if (s.active === active) {
      return s;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6387: ${what}\n${s.raw}`);
    }
    await sleep(40);
  }
}

async function selectedRect(client: ControlClient): Promise<Rect> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const raw = String(res[1] ?? "").trim();
  const m = / annotRect=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(raw);
  if (res[0] !== 0 || !m) {
    throw new Error(`issue-6387: no selected annotation: ${raw}`);
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

async function doubleClick(client: ControlClient, canvas: number, x: number, y: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    await client.request(ControlCommand.TestInput, ["click", x, y, 0, 0]);
    return;
  }
  const s = clientToScreen(canvas, x, y);
  setCursorPos(s.x, s.y);
  const pt = packCoords(x, y);
  sendMessage(canvas, WM_MOUSEMOVE, 0, pt);
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, pt);
  sendMessage(canvas, WM_LBUTTONUP, 0, pt);
  sendMessage(canvas, WM_LBUTTONDBLCLK, MK_LBUTTON, pt);
  sendMessage(canvas, WM_LBUTTONUP, 0, pt);
}

// orig edits in an Edit child of the canvas, ng in a text area on the frame
function findEditBox(canvas: number): number {
  let found = 0;
  enumChildWindows(canvas, (hwnd) => {
    if (getClassName(hwnd) === "Edit" && getControlText(hwnd).startsWith("This is a text")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  if (!found) {
    throw new Error("issue-6387: in-place edit box not found");
  }
  return found;
}

// create a free text at `pt` and close its in-place edit, leaving it selected
async function createFreeText(client: ControlClient, frame: number, canvas: number, x: number, y: number) {
  sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotFreeText"), packCoords(x, y));
  await waitForEdit(client, true, "creating a free text did not open the editor");
  const target = USE_NG ? frame : findEditBox(canvas);
  sendMessage(target, WM_KEYDOWN, VK_ESCAPE, 0);
  sendMessage(target, WM_KEYUP, VK_ESCAPE, 0);
  await waitForEdit(client, false, "Esc did not close the editor");
  await client.waitForRenderIdle();
  return await selectedRect(client);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6387");
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
    sendCommandSync(frame, cmdId("CmdToggleEditPDF"));

    const rc = getClientRect(canvas);
    const cx = Math.floor(rc.right / 2);
    const cy = Math.floor(rc.bottom / 2);
    const first = await createFreeText(client, frame, canvas, cx - 100, cy - 150);
    const second = await createFreeText(client, frame, canvas, cx - 100, cy + 50);
    if (Math.abs(first.y - second.y) < first.dy) {
      throw new Error(`issue-6387: annotations overlap: ${JSON.stringify(first)} ${JSON.stringify(second)}`);
    }

    // the second one is selected; double-click the first
    await doubleClick(client, canvas, first.x + 20, first.y + 10);
    const s = await waitForEdit(client, true, "double-click on another free text did not edit it");
    if (Math.abs(s.rect.y - first.y) > 3) {
      throw new Error(`issue-6387: edit box at y=${s.rect.y}, want the first annotation at y=${first.y}\n${s.raw}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }

  console.log("issue-6387: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
