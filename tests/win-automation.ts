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
  testWindowPos,
  waitForWindowIdle,
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
  getSystemMetrics,
  getWindowRect,
  readWindowDCColumn,
  SM_CXVSCROLL,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_KEYDOWN,
  WM_CONTEXTMENU,
  WM_COMMAND,
  MK_LBUTTON,
  VK_RETURN,
  VK_TAB,
  VK_ESCAPE,
  getClientRect,
  clientToScreen,
  getPopupMenuHandle,
  readMenuTree,
  getWindowText,
  getFocusedHwnd,
  killAndWait,
  killProcessesNamed,
  type MenuItem,
} from "./winapi.ts";
import { ControlClient, uniquePipeName } from "./control.ts";

export { captureWindowToPng, killAndWait, killProcessesNamed };

export const FRAME_CLASS = "SUMATRA_PDF_FRAME";
export const CANVAS_CLASS = "SUMATRA_PDF_CANVAS";

// -window-pos for the upper-right quarter of the screen (see testWindowPos)
export function windowPosArgs(): string[] {
  const p = testWindowPos();
  return ["-window-pos", `${p.dx}x${p.dy}@${p.x}x${p.y}`];
}

// Launch SumatraPDF.exe with -for-testing: a fresh instance that won't
// interfere with a running SumatraPDF, doesn't restore the previous session
// (only opens files passed on the cmd-line) and doesn't save settings. Use
// proc.pid with waitForFrame() to get the window.
//
// The window opens as a quarter of the screen, which is a good deal faster to
// render and to capture. Pass { defaultWindowPos: true } in a test that is
// about the window's own size or state - fullscreen, maximized, minimized,
// restoring a remembered position - so the app picks the position itself.
export function launchSumatra(args: string[], opts?: { defaultWindowPos?: boolean }): Bun.Subprocess {
  const posArgs = opts?.defaultWindowPos || args.includes("-window-pos") ? [] : windowPosArgs();
  return Bun.spawn([EXE, "-for-testing", ...posArgs, ...args], { stdout: "ignore", stderr: "ignore" });
}

// Launch with -dbg-control so the test can wait for render-idle (and other
// control commands) instead of sleeping. saveSettings: skip -for-testing when
// the test has to read back the settings file.
export async function launchControlled(
  args: string[],
  opts?: { defaultWindowPos?: boolean; saveSettings?: boolean },
): Promise<{ proc: Bun.Subprocess; client: ControlClient; frame: number }> {
  const pipe = uniquePipeName();
  const posArgs = opts?.defaultWindowPos || args.includes("-window-pos") ? [] : windowPosArgs();
  const testing = opts?.saveSettings ? [] : ["-for-testing"];
  const proc = Bun.spawn([EXE, ...testing, ...posArgs, "-dbg-control", pipe, ...args], {
    stdout: "ignore",
    stderr: "ignore",
  });
  try {
    const client = await ControlClient.connect(pipe);
    const frame = await waitForFrame(proc.pid!);
    if (!frame) {
      client.close();
      throw new Error("SumatraPDF main window did not appear");
    }
    return { proc, client, frame };
  } catch (e) {
    await killAndWait(proc);
    throw e;
  }
}

export function sendCommandSync(hwnd: number, id: number): void {
  sendMessage(hwnd, WM_COMMAND, id, 0);
}

export async function waitForTitle(frame: number, pred: (title: string) => boolean, timeoutMs = 8000): Promise<string> {
  const deadline = Date.now() + timeoutMs;
  let last = "";
  while (Date.now() < deadline) {
    last = getWindowText(frame);
    if (pred(last)) {
      return last;
    }
    await sleep(40);
  }
  throw new Error(`window title did not match in time (last: '${last}')`);
}

export async function waitForFocusClass(frame: number, className: string, timeoutMs = 5000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const hwnd = getFocusedHwnd(frame);
    if (hwnd && getClassName(hwnd) === className) {
      return hwnd;
    }
    await sleep(30);
  }
  throw new Error(`focus did not move to ${className}`);
}

// Wait for the app to exit by itself, which is also when it has finished
// writing its settings file. Exact, unlike sleeping "long enough" after
// CmdExit / WM_CLOSE. Returns false on timeout so the caller can report it
export async function waitForExit(proc: Bun.Subprocess, timeoutMs = 10000): Promise<boolean> {
  let exited = false;
  void proc.exited.then(() => {
    exited = true;
  });
  const deadline = Date.now() + timeoutMs;
  while (!exited && Date.now() < deadline) {
    await sleep(25);
  }
  return exited;
}

// Wait for the document to be on screen: the frame exists, the canvas has
// painted and the picture has stopped changing
export async function waitForDocument(pid: number, timeoutMs = 12000): Promise<number> {
  const frame = await waitForFrame(pid, timeoutMs);
  if (!frame) {
    return 0;
  }
  const canvas = findCanvas(frame);
  await waitForWindowIdle(canvas || frame);
  return frame;
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

// A dense pixel sample of the top `dy` rows of a window, straight from its
// window DC (so it covers the non-client caption row as well as the tab bar and
// toolbar children). Use it to assert that chrome repainted: capture once in a
// known-good state, again after the action, and compare — see tests/issue-5866.ts.
// Call setProcessDpiAware() first on scaled displays.
export function topChromePixels(hwnd: number, dy: number): number[] {
  const r = getWindowRect(hwnd);
  const w = r.right - r.left;
  const px: number[] = [];
  for (let x = 2; x < w - 2; x += 6) {
    px.push(...readWindowDCColumn(hwnd, x, 0, dy));
  }
  return px;
}

// Count positions where both samples have a real COLORREF and they differ.
// GetPixel's CLR_INVALID (0xffffffff) is skipped: concurrent paint often makes
// a few samples fail even when the chrome is fine (issue #5866 false positive).
export function countDifferingPixels(a: number[], b: number[]): number {
  const CLR_INVALID = 0xffffffff;
  let n = 0;
  const len = Math.min(a.length, b.length);
  for (let i = 0; i < len; i++) {
    if (a[i] === CLR_INVALID || b[i] === CLR_INVALID) {
      continue;
    }
    if (a[i] !== b[i]) {
      n++;
    }
  }
  return n + Math.abs(a.length - b.length);
}

// How many distinct colors are painted down the middle of the canvas's native
// vertical scrollbar. The scrollbar lives in the canvas's non-client area, so
// this reads the window DC (what is on screen) rather than PrintWindow output.
//
// A drawn scrollbar has at least a track color and a thumb color, so >= 2. An
// unpainted one is a flat strip of a single color (issue #5850), which is how a
// missing WM_NCPAINT shows up. Call setProcessDpiAware() first on scaled displays.
export function vScrollbarColorCount(canvas: number): number {
  const r = getWindowRect(canvas);
  const w = r.right - r.left;
  const h = r.bottom - r.top;
  const sbW = getSystemMetrics(SM_CXVSCROLL);
  const colors = readWindowDCColumn(canvas, w - Math.floor(sbW / 2), 0, h);
  return new Set(colors).size;
}

// Open hwnd's context menu at the center of its client area and read the whole
// menu tree (submenus included), then dismiss it. Returns [] if none appeared.
export async function readContextMenuTree(hwnd: number): Promise<MenuItem[]> {
  const cr = getClientRect(hwnd);
  const s = clientToScreen(hwnd, Math.floor(cr.right / 2), Math.floor(cr.bottom / 2));
  openContextMenu(hwnd, s.x, s.y);
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

// Depth-first search for a submenu by its label (exact, & already stripped).
export function findSubMenu(items: MenuItem[], label: string): MenuItem | null {
  for (const it of items) {
    if (it.items) {
      if (it.text === label) {
        return it;
      }
      const found = findSubMenu(it.items, label);
      if (found) {
        return found;
      }
    }
  }
  return null;
}
