// High-level GUI automation actions for driving SumatraPDF in ad-hoc tests,
// built on the raw winapi.ts wrappers. winapi.ts is the FFI / raw-message layer;
// this file is the "do a thing" layer (launch the app, find its windows, click a
// spot, type into the form-field editor, open a context menu, screenshot).
//
// Why message-posting instead of real input: on the test machine injected
// SendInput mouse/keyboard events are dropped, but posting window messages
// cross-process works (see the project memory env-gui-automation). Capturing via
// PrintWindow (captureWindowToPng) also works for occluded/background windows,
// which a screen grab does not.
//
// These are for *ad-hoc* tests (not checked in). Put reusable helpers here, not
// in the individual ad-hoc scripts.

import { EXE } from "./util.ts";
import {
  enumWindows,
  getClassName,
  findChildWindow,
  waitForTopWindow,
  packCoords,
  sleep,
  sendMessage,
  postMessage,
  sendText,
  captureWindowToPng,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_KEYDOWN,
  WM_CONTEXTMENU,
  WM_COMMAND,
  MK_LBUTTON,
  VK_RETURN,
  VK_TAB,
  VK_ESCAPE,
} from "./winapi.ts";

export { captureWindowToPng };

export const FRAME_CLASS = "SUMATRA_PDF_FRAME";
export const CANVAS_CLASS = "SUMATRA_PDF_CANVAS";

// Launch SumatraPDF.exe with -for-testing: a fresh instance that won't
// interfere with a running SumatraPDF, doesn't restore the previous session
// (only opens files passed on the cmd-line) and doesn't save settings. Use
// proc.pid with waitForFrame() to get the window.
export function launchSumatra(args: string[]): Bun.Subprocess {
  return Bun.spawn([EXE, "-for-testing", ...args], { stdout: "ignore", stderr: "ignore" });
}

// the main SUMATRA_PDF_FRAME window of a process (0 on timeout)
export function waitForFrame(pid: number, timeoutMs = 12000): Promise<number> {
  return waitForTopWindow(pid, FRAME_CLASS, timeoutMs);
}

// the document canvas (child of the frame)
export function findCanvas(frame: number): number {
  return findChildWindow(frame, CANVAS_CLASS);
}

export function findChildByClass(parent: number, className: string): number {
  return findChildWindow(parent, className);
}

// the floating in-place form-field editor: a standard "Edit" child of the canvas
// that appears while editing a text/choice field. 0 if none is active.
export function findFormEditor(canvas: number): number {
  return findChildWindow(canvas, "Edit");
}

// poll until the form editor overlay appears (after clicking a text field)
export async function waitForFormEditor(canvas: number, timeoutMs = 1500): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const h = findFormEditor(canvas);
    if (h) {
      return h;
    }
    await sleep(120);
  }
  return 0;
}

// Left-click at client (x,y) of hwnd. Sent synchronously, so the click is fully
// handled before returning, then we wait settleMs for any re-render.
export async function clickAt(hwnd: number, x: number, y: number, settleMs = 350): Promise<void> {
  const lp = packCoords(x, y);
  sendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  sendMessage(hwnd, WM_LBUTTONUP, 0, lp);
  await sleep(settleMs);
}

// Press a key (WM_KEYDOWN). Posted (not sent) so it flows through the app's
// PreTranslateMessage like real key input would (needed for canvas shortcuts /
// arrow keys; also fine for the form editor's Enter/Tab/Esc handling).
export async function pressKey(hwnd: number, vk: number, settleMs = 250): Promise<void> {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  await sleep(settleMs);
}

export const pressEnter = (hwnd: number) => pressKey(hwnd, VK_RETURN, 400);
export const pressTab = (hwnd: number) => pressKey(hwnd, VK_TAB, 300);
export const pressEscape = (hwnd: number) => pressKey(hwnd, VK_ESCAPE, 250);

// Type text into an input control (e.g. the form-field editor) via WM_SETTEXT,
// optionally committing with Enter. `control` is the edit hwnd itself.
export async function typeIntoInput(control: number, text: string, commit = true): Promise<void> {
  sendText(control, text);
  await sleep(150);
  if (commit) {
    await pressEnter(control);
  }
}

// Click a form text field at canvas-client (x,y) and type into the editor that
// pops up. Returns true if the editor appeared. Convenience for the common case.
export async function fillFormFieldAt(canvas: number, x: number, y: number, text: string): Promise<boolean> {
  await clickAt(canvas, x, y);
  const ed = await waitForFormEditor(canvas);
  if (!ed) {
    return false;
  }
  await typeIntoInput(ed, text);
  return true;
}

// Open hwnd's context menu by posting WM_CONTEXTMENU at screen (x,y). Posted
// (async) so we don't block on the modal menu; poll waitForContextMenu() after.
export function openContextMenu(hwnd: number, screenX: number, screenY: number): void {
  postMessage(hwnd, WM_CONTEXTMENU, hwnd, packCoords(screenX, screenY));
}

// Wait for a popup menu window (#32768) to appear; returns its hwnd or 0.
export async function waitForContextMenu(timeoutMs = 1500): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    let menu = 0;
    enumWindows((h) => {
      if (getClassName(h) === "#32768") {
        menu = h;
        return false;
      }
      return true;
    });
    if (menu) {
      return menu;
    }
    await sleep(80);
  }
  return 0;
}

// Send a WM_COMMAND (menu/command id) to a window (usually the frame).
export function sendCommand(hwnd: number, cmdId: number): void {
  postMessage(hwnd, WM_COMMAND, cmdId, 0);
}
