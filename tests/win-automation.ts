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

import { IS_MAC } from "./host.ts";
import { macClick } from "./mac-control.ts";
import { cmdId, drainProcStderr, EXE, SLOW_BUILD_FACTOR, setFailureContext, USE_NG } from "./util.ts";
import {
  testWindowPos,
  injectTouch,
  POINTER_FLAG_DOWN,
  POINTER_FLAG_UP,
  POINTER_FLAG_UPDATE,
  waitForWindowIdle,
  enumWindows,
  getClassName,
  findChildWindow,
  findVisibleChildWindow,
  waitForTopWindow,
  packCoords,
  ngInputTarget,
  sleep,
  sendMessage,
  postMessage,
  sendText,
  captureWindowToPng,
  getSystemMetrics,
  getWindowOwner,
  getWindowPid,
  WM_CLOSE,
  getWindowRect,
  isWindowVisible,
  readWindowDCColumn,
  withWindowDC,
  SM_CXVSCROLL,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_KEYDOWN,
  WM_KEYUP,
  getRootWindow,
  WM_CONTEXTMENU,
  WM_COMMAND,
  MK_LBUTTON,
  VK_RETURN,
  VK_DOWN,
  VK_TAB,
  VK_ESCAPE,
  ensureModifierKeysUp,
  getClientRect,
  clientToScreen,
  getCursorPos,
  setCursorPos,
  sendCopyDataW,
  getPopupMenuHandle,
  readMenuTree,
  getWindowText,
  getFocusedHwnd,
  killAndWait as killAndWaitProcess,
  killProcessesNamed,
  type MenuItem,
} from "./winapi.ts";

export { ensureModifierKeysUp };
import { ControlClient, ControlCommand, uniquePipeName } from "./control.ts";

export { captureWindowToPng, killProcessesNamed };

type SharedControlledSession = {
  proc: Bun.Subprocess;
  pipe: string;
  frame: number;
};

let sharedSessionRequested = false;
let sharedSession: SharedControlledSession | null = null;

// keep drain promises alive: `void new Response().text()` can be GC'd, the
// pipe fills, ASan's next stderr write kills the process, tests see EPIPE
const gStderrDrains = new Set<Promise<string>>();
const gStderrByProc = new WeakMap<Bun.Subprocess, Promise<string>>();

function drainStderr(proc: Bun.Subprocess): Promise<string> {
  const p = drainProcStderr(proc.stderr);
  gStderrDrains.add(p);
  gStderrByProc.set(proc, p);
  void p.finally(() => gStderrDrains.delete(p));
  return p;
}

export async function takeStderr(proc: Bun.Subprocess): Promise<string> {
  const p = gStderrByProc.get(proc);
  return p ? (await p).trim() : "";
}

// the process the current test launched last; its stderr is attached to a
// test failure. stderr only closes when the process exits, so a still-running
// app yields nothing rather than hanging the report.
let gLastProc: Bun.Subprocess | null = null;
const STDERR_GRACE_MS = 1500;
const STDERR_TAIL_CHARS = 4000;

async function lastProcStderrTail(): Promise<string> {
  if (!gLastProc) {
    return "";
  }
  const timeout = new Promise<string>((resolve) => setTimeout(() => resolve(""), STDERR_GRACE_MS));
  const s = await Promise.race([takeStderr(gLastProc), timeout]);
  return s.length > STDERR_TAIL_CHARS ? "..." + s.slice(-STDERR_TAIL_CHARS) : s;
}

setFailureContext(lastProcStderrTail);

export function beginSharedControlledSession(): void {
  // macOS has no WM_COPYDATA open, so each test launches its own process.
  if (IS_MAC) {
    return;
  }
  if (sharedSession || sharedSessionRequested) {
    throw new Error("a shared controlled session is already active");
  }
  sharedSessionRequested = true;
}

export async function endSharedControlledSession(): Promise<void> {
  sharedSessionRequested = false;
  if (sharedSession) {
    const session = sharedSession;
    sharedSession = null;
    await killAndWaitProcess(session.proc);
  }
}

export async function killAndWait(proc: Bun.Subprocess): Promise<void> {
  if (sharedSession?.proc === proc) {
    return;
  }
  await killAndWaitProcess(proc);
}

// ng's frame is gpui's window. Tests import this binding, so the assignment
// is visible to them.
export let FRAME_CLASS = USE_NG ? "GpuiSystemMonitor" : "SUMATRA_PDF_FRAME";
export const CANVAS_CLASS = "SUMATRA_PDF_CANVAS";

// -window-pos for the right half of the screen (see testWindowPos)
export function windowPosArgs(): string[] {
  const p = testWindowPos();
  return ["-window-pos", `${p.dx}x${p.dy}@${p.x}x${p.y}`];
}

// Launch SumatraPDF.exe with -for-testing: a fresh instance that won't
// interfere with a running SumatraPDF, doesn't restore the previous session
// (only opens files passed on the cmd-line) and doesn't save settings. Use
// proc.pid with waitForFrame() to get the window.
//
// The window opens on the right half of the screen, which is a good deal
// faster to render and to capture than a full-screen one. Pass { defaultWindowPos: true } in a test that is
// about the window's own size or state - fullscreen, maximized, minimized,
// restoring a remembered position - so the app picks the position itself.
export function launchSumatra(args: string[], opts?: { defaultWindowPos?: boolean }): Bun.Subprocess {
  const posArgs = opts?.defaultWindowPos || args.includes("-window-pos") ? [] : windowPosArgs();
  const proc = Bun.spawn([EXE, "-for-testing", ...posArgs, ...args], {
    stdout: "ignore",
    // ASan writes its report to stderr. "ignore" closes that pipe and the
    // next write kills the process.
    stderr: IS_MAC ? "pipe" : "ignore",
  });
  if (IS_MAC) {
    drainStderr(proc);
  }
  return proc;
}

// Launch with -dbg-control so the test can wait for render-idle (and other
// control commands) instead of sleeping. saveSettings: skip -for-testing when
// the test has to read back the settings file. env: extra environment variables.
export async function launchControlled(
  args: string[],
  opts?: { defaultWindowPos?: boolean; saveSettings?: boolean; env?: Record<string, string> },
): Promise<{ proc: Bun.Subprocess; client: ControlClient; frame: number }> {
  // many tests post keys and clicks directly; a held modifier would chord them
  await ensureModifierKeysUp();
  if (sharedSession) {
    if (opts?.env) {
      throw new Error("a shared controlled session can't take env; register the test after the shared group");
    }
    const path = args[args.length - 1];
    if (!path || path.startsWith("-")) {
      throw new Error("shared controlled session needs a document path as its last argument");
    }
    // Cancel a gesture left by the prior test, or the next click only resets it.
    sendMessage(sharedSession.frame, WM_KEYDOWN, VK_ESCAPE, 0);
    sendMessage(sharedSession.frame, WM_COMMAND, cmdId("CmdDiscardChanges"), 0);
    sendMessage(sharedSession.frame, WM_COMMAND, cmdId("CmdToggleEditPDF"), 0);
    const command = `[Open("${path}", 0, 1, 0)]`;
    if (!sendCopyDataW(sharedSession.frame, 0x44646557, command)) {
      throw new Error("could not open the next document in the shared SumatraPDF process");
    }
    const client = await ControlClient.connect(sharedSession.pipe);
    await client.waitForRenderIdle();
    return { proc: sharedSession.proc, client, frame: sharedSession.frame };
  }
  const pipe = uniquePipeName();
  const posArgs = opts?.defaultWindowPos || args.includes("-window-pos") ? [] : windowPosArgs();
  const testing = opts?.saveSettings ? [] : ["-for-testing"];
  // drain stderr: ASan writes reports there, and with stderr:"ignore" those
  // writes hit a closed pipe and kill the process (EPIPE in the test client)
  const proc = Bun.spawn([EXE, ...testing, ...posArgs, "-dbg-control", pipe, ...args], {
    stdout: "ignore",
    stderr: "pipe",
    env: opts?.env ? { ...process.env, ...opts.env } : undefined,
  });
  drainStderr(proc);
  gLastProc = proc;
  try {
    const client = await ControlClient.connect(pipe);
    // macOS has no HWND. The control socket is the ready signal; 1 stands in
    // for the frame so callers that only check it is non-zero keep going.
    const frame = IS_MAC ? 1 : await waitForFrame(proc.pid!);
    if (!frame) {
      client.close();
      throw new Error("SumatraPDF main window did not appear");
    }
    if (sharedSessionRequested) {
      sharedSession = { proc, pipe, frame };
    }
    return { proc, client, frame };
  } catch (e) {
    await killAndWaitProcess(proc);
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
  if (IS_MAC) {
    return frame;
  }
  // ng draws the page in the frame
  if (USE_NG) {
    return frame;
  }
  return findChildWindow(frame, CANVAS_CLASS);
}

export function findChildByClass(parent: number, className: string): number {
  const child = findChildWindow(parent, className);
  // ng draws the toolbar in the frame; there is no SUMATRA_VIRT_TOOLBAR child
  if (!child && USE_NG && className === "SUMATRA_VIRT_TOOLBAR") {
    return parent;
  }
  return child;
}

// the floating in-place form-field editor: a visible "Edit" child of the canvas
// that appears while editing a text/choice field. 0 if none is active.
// Must skip hidden children: the home-page search box is an Edit under the
// canvas even after a document is open.
export function findFormEditor(canvas: number): number {
  return findVisibleChildWindow(canvas, "Edit");
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
//
// The cursor is moved to the click first: SetCapture (from a drag start) injects
// WM_MOUSEMOVE at the real cursor, and if that is far from (x,y) the app treats
// the click as a drag (ClickEdgeToTurnPage and similar then no-op).
export async function clickAt(hwnd: number, x: number, y: number, settleMs = 350, extraMk = 0): Promise<void> {
  if (IS_MAC) {
    macClick(x, y, extraMk);
    await sleep(settleMs);
    return;
  }
  // the app also reads the live key state: a Ctrl stuck down mid-run (RDP)
  // turns a plain click into Ctrl+click
  await ensureModifierKeysUp();
  const screen = clientToScreen(hwnd, x, y);
  setCursorPos(screen.x, screen.y);
  // ng: one click, as on macOS, so no real move lands between down and up
  const target = ngInputTarget(hwnd);
  if (target) {
    macClick(x, y, extraMk, target);
  } else {
    const lp = packCoords(x, y);
    sendMessage(hwnd, WM_LBUTTONDOWN, MK_LBUTTON | extraMk, lp);
    sendMessage(hwnd, WM_LBUTTONUP, extraMk, lp);
  }

  // someone moving the real mouse (e.g. over RDP) mid-click turns it into a drag.
  // macOS has no GetCursorPos; the click coordinates went out as TestInput.
  if (!IS_MAC) {
    const at = getCursorPos();
    if (at.x !== screen.x || at.y !== screen.y) {
      console.log(
        `⚠ clickAt: real mouse moved during the click (to ${at.x},${at.y}, click at ${screen.x},${screen.y})`,
      );
    }
  }
  await sleep(settleMs);
}

type ScreenRect = { left: number; top: number; right: number; bottom: number };

function rectHasPoint(r: ScreenRect, x: number, y: number): boolean {
  return x >= r.left && x < r.right && y >= r.top && y < r.bottom;
}

// Moves the real cursor off `avoid` (default: where test windows open), so real
// mouse moves can't hover the app, e.g. open a toolbar drop-down. SetCursorPos
// clamps to the screen, so try each side and keep the one that landed outside.
export function parkCursorAway(avoid?: ScreenRect[]): boolean {
  if (!avoid) {
    const p = testWindowPos();
    avoid = [{ left: p.x, top: p.y, right: p.x + p.dx, bottom: p.y + p.dy }];
  }
  const fr = avoid[0]!;
  const candidates = [
    { x: fr.left - 40, y: fr.top + 100 },
    { x: fr.right + 40, y: fr.top + 100 },
    { x: fr.left + 100, y: fr.bottom + 40 },
    { x: fr.left + 100, y: fr.top - 40 },
  ];
  for (const p of candidates) {
    if (!setCursorPos(p.x, p.y)) {
      continue;
    }
    const c = getCursorPos();
    if (!avoid.some((r) => rectHasPoint(r, c.x, c.y))) {
      return true;
    }
  }
  return false;
}

// Press a key (WM_KEYDOWN). Posted (not sent) so it flows through the app's
// PreTranslateMessage like real key input would (needed for canvas shortcuts /
// arrow keys; also fine for the form editor's Enter/Tab/Esc handling).
// a held Ctrl/Shift/Alt on the machine would turn this into a chord
export async function pressKey(hwnd: number, vk: number, settleMs = 250): Promise<void> {
  await ensureModifierKeysUp();
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

// A one-finger vertical swipe from (x, y0) to (x, y1), in screen pixels: a
// touch pan, which Windows delivers as WM_GESTURE GID_PAN.
export async function touchSwipe(x: number, y0: number, y1: number, steps = 20): Promise<boolean> {
  if (!injectTouch(x, y0, POINTER_FLAG_DOWN)) {
    return false;
  }
  for (let i = 1; i <= steps; i++) {
    await sleep(16);
    injectTouch(x, Math.round(y0 + ((y1 - y0) * i) / steps), POINTER_FLAG_UPDATE);
  }
  return injectTouch(x, y1, POINTER_FLAG_UP);
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
  withWindowDC(hwnd, () => {
    for (let x = 2; x < w - 2; x += 6) {
      px.push(...readWindowDCColumn(hwnd, x, 0, dy));
    }
  });
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

// The Command Palette window and where its keys go. orig: the focused Edit.
// ng: its gpui window takes the keys itself; set the query with paletteSetText.
export function findPalette(frame: number): { palette: number; edit: number } {
  if (USE_NG && !IS_MAC) {
    const palette = findNgPalette(frame);
    return { palette, edit: palette };
  }
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return { palette: 0, edit: 0 };
  }
  if (USE_NG) {
    return { palette: findNgPalette(frame), edit };
  }
  const palette = getRootWindow(edit);
  return { palette: palette === frame ? 0 : palette, edit };
}

// The palette once it takes input, polled for up to timeoutMs. ng asks the
// control channel, which answers once its window is made.
export async function waitForPalette(
  client: ControlClient,
  frame: number,
  timeoutMs = 8000,
): Promise<{ palette: number; edit: number }> {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const handles = findPalette(frame);
    let ready = handles.palette !== 0 && handles.edit !== 0;
    if (ready && USE_NG && !IS_MAC) {
      ready = (await client.request(ControlCommand.TestCommandPalette, []))[0] === 0;
    }
    if (ready) {
      return handles;
    }
    if (Date.now() > deadline) {
      return { palette: 0, edit: 0 };
    }
    await sleep(50);
  }
}

// Replaces the palette's query. orig: WM_SETTEXT on its Edit; ng (and mac)
// through the control channel.
export async function paletteSetText(
  client: ControlClient,
  handles: { palette: number; edit: number },
  text: string,
): Promise<void> {
  if (!USE_NG) {
    sendText(handles.edit, text);
    return;
  }
  const res = await client.request(ControlCommand.TestInput, ["text", text]);
  if (res[0] !== 0) {
    throw new Error(`paletteSetText: ${String(res[1] ?? "")}`);
  }
}

export type PaletteState = {
  open: boolean;
  sel: number;
  items: number;
  querySel: [number, number];
  queryLen: number;
  cmd: number;
  rtl: number;
  thumb: number;
  page: number;
  rendered: number;
  annots: number;
  annotPage: number;
  annotsDone: number;
  settingHelp: number;
  selValue: string;
  selText: string;
  raw: string;
};

const closedPalette: PaletteState = {
  open: false,
  sel: -1,
  items: 0,
  querySel: [0, 0],
  queryLen: 0,
  cmd: 0,
  rtl: 0,
  thumb: 0,
  page: 0,
  rendered: 0,
  annots: 0,
  annotPage: 0,
  annotsDone: 0,
  settingHelp: 0,
  selValue: "",
  selText: "",
  raw: "",
};

// Parsed TestCommandPalette state; open is false (other fields zero) when closed.
export async function paletteState(client: ControlClient): Promise<PaletteState> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  const out = String(res[1] ?? "").trim();
  if (res[0] === 2) {
    return { ...closedPalette, raw: out };
  }
  const qs = /querySel=(-?\d+),(-?\d+)/.exec(out);
  if (res[0] !== 0 || !qs) {
    throw new Error(`paletteState: TestCommandPalette failed: ${out}`);
  }
  const num = (name: string): number => Number(new RegExp(`\\b${name}=(-?\\d+)`).exec(out)?.[1] ?? 0);
  return {
    open: true,
    sel: num("sel"),
    items: num("items"),
    querySel: [+qs[1]!, +qs[2]!],
    queryLen: num("queryLen"),
    cmd: num("cmd"),
    rtl: num("rtl"),
    thumb: num("thumb"),
    page: num("page"),
    rendered: num("rendered"),
    annots: num("annots"),
    annotPage: num("annotPage"),
    annotsDone: num("annotsDone"),
    settingHelp: num("settingHelp"),
    selValue: /selValue=(.*?) selText=/.exec(out)?.[1] ?? "",
    selText: /selText=(.*)$/.exec(out)?.[1] ?? "",
    raw: out,
  };
}

// Opens the palette, types query and runs the item whose command is cmdName.
export async function paletteRun(client: ControlClient, frame: number, query: string, cmdName: string): Promise<void> {
  const tag = `paletteRun(${cmdName})`;
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const handles = await waitForPalette(client, frame, 8_000 * SLOW_BUILD_FACTOR);
  const focusDeadline = Date.now() + 8_000 * SLOW_BUILD_FACTOR;
  while (USE_NG && Date.now() < focusDeadline) {
    const raw = (await paletteState(client)).raw;
    if (!raw.includes("editFocus=") || raw.includes("editFocus=1")) {
      break;
    }
    await sleep(40);
  }
  // not paletteSetText: ng's "text" input goes to a lingering free-text editor first
  if (USE_NG) {
    await ngType(client, query);
  } else {
    await paletteSetText(client, handles, query);
  }

  const deadline = Date.now() + 3_000 * SLOW_BUILD_FACTOR;
  let st: PaletteState;
  for (;;) {
    st = await paletteState(client);
    if (st.open && st.queryLen === query.length) {
      break;
    }
    if (Date.now() > deadline) {
      throw new Error(`${tag}: palette did not filter\n${st.raw}`);
    }
    await sleep(40);
  }

  const want = cmdId(cmdName);
  for (let i = 0; i < st.items; i++) {
    st = await paletteState(client);
    if (st.open && st.cmd === want) {
      await paletteKey(client, handles, VK_RETURN);
      return;
    }
    await paletteKey(client, handles, VK_DOWN);
  }
  throw new Error(`${tag}: command was not in the filtered palette`);
}

async function paletteKey(client: ControlClient, handles: { edit: number }, vk: number): Promise<void> {
  if (USE_NG) {
    await ngKey(client, vk);
    return;
  }
  postMessage(handles.edit, WM_KEYDOWN, vk, 0);
  await sleep(80);
}

// ng's find box is a gpui input with no Edit. Drives it through TestFindUiState:
// action is "set", "caret", "enter" or "state". Returns the state text.
export async function ngFind(client: ControlClient, action: string, arg = ""): Promise<string> {
  const res = await client.request(ControlCommand.TestFindUiState, arg ? [action, arg] : [action]);
  if (res[0] !== 0) {
    throw new Error(`ngFind: ${action} failed: ${String(res[1] ?? "")}`);
  }
  return String(res[1] ?? "");
}

// ng: CmdFindFirst is posted, so poll until the find box shows
export async function waitFindOpen(client: ControlClient, timeoutMs = 4000): Promise<void> {
  const deadline = Date.now() + timeoutMs;
  let last = "";
  for (;;) {
    last = String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "");
    if (/compact=1/.test(last) || /floating=1/.test(last)) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`waitFindOpen: find bar did not open (${last})`);
    }
    await sleep(40);
  }
}

export type ToolWinInfo = { hwnd: number; owner: number; title: string; visible: boolean; mainEnabled: boolean };

// ng: one tool window from the TestToolWindow list, or null when it is not up
export async function ngToolWindow(client: ControlClient, name: string): Promise<ToolWinInfo | null> {
  const raw = String((await client.request(ControlCommand.TestToolWindow, ["list"]))[1] ?? "");
  for (const line of raw.split("\n")) {
    if (!line.startsWith(`${name} made=1 `)) {
      continue;
    }
    const num = (key: string) => Number(new RegExp(`${key}=(-?\\d+)`).exec(line)?.[1] ?? 0);
    return {
      hwnd: num("hwnd"),
      owner: num("owner"),
      title: /title='(.*)'/.exec(line)?.[1] ?? "",
      visible: num("visible") === 1,
      mainEnabled: num("mainEnabled") === 1,
    };
  }
  return null;
}

export type ToolWinOpts = { title?: string; before?: Set<number>; exclude?: number; timeoutMs?: number };

// Waits for a dialog / tool window. ng: the named tool window of the control
// channel. Orig: a new visible default-class window, optionally with a title.
// Returns its hwnd, or 0 on timeout.
export async function waitToolWindow(
  client: ControlClient,
  pid: number,
  name: string,
  opts: ToolWinOpts = {},
): Promise<number> {
  const deadline = Date.now() + (opts.timeoutMs ?? 5000) * SLOW_BUILD_FACTOR;
  for (;;) {
    let found = 0;
    if (USE_NG) {
      const tw = await ngToolWindow(client, name);
      found = tw && tw.visible && (!opts.title || tw.title === opts.title) ? tw.hwnd : 0;
    } else {
      enumWindows((hwnd) => {
        if (
          hwnd === opts.exclude ||
          opts.before?.has(hwnd) ||
          getWindowPid(hwnd) !== pid ||
          getClassName(hwnd) !== "SumatraWgDefaultWinClass" ||
          !isWindowVisible(hwnd) ||
          (opts.title && getWindowText(hwnd) !== opts.title)
        ) {
          return true;
        }
        found = hwnd;
        return false;
      });
    }
    if (found) {
      return found;
    }
    if (Date.now() > deadline) {
      return 0;
    }
    await sleep(40);
  }
}

// ng: the tool window is closed over the control channel. Orig: WM_CLOSE.
export async function closeToolWindow(client: ControlClient, name: string, hwnd: number): Promise<void> {
  if (USE_NG) {
    await client.request(ControlCommand.TestToolWindow, ["close", name]);
    return;
  }
  postMessage(hwnd, WM_CLOSE, 0, 0);
}

// ng's palette is the visible GPUI window the frame owns. Posted keys and
// pixel captures work on it.
function findNgPalette(frame: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowOwner(hwnd) !== frame || !isWindowVisible(hwnd) || getClassName(hwnd) !== FRAME_CLASS) {
      return true;
    }
    const r = getWindowRect(hwnd);
    if (r.right - r.left < 100) {
      return true;
    }
    found = hwnd;
    return false;
  });
  return found;
}

// A posted, unmodified key press that doesn't wait for the app.
export function pressVKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, 0);
  postMessage(hwnd, WM_KEYUP, vk, 0);
}

export async function ngInput(client: ControlClient, args: (string | number)[]): Promise<void> {
  const res = await client.request(ControlCommand.TestInput, args);
  const raw = String(res[1] ?? "");
  if (res[0] !== 0 || !raw.startsWith("OK")) {
    throw new Error(`TestInput ${args.join(" ")} failed: ${raw}`);
  }
}

// ng has no window to post keys to: they go through TestInput.
export function ngKey(client: ControlClient, vk: number): Promise<void> {
  return ngInput(client, ["key", vk, 0]);
}

export async function ngType(client: ControlClient, text: string): Promise<void> {
  for (const ch of text) {
    await ngInput(client, ["char", ch.codePointAt(0)!]);
  }
}

type Pt = { x: number; y: number };

// Pixels in the placement-preview blue within 8px of the start-end box.
export function countPreviewBlue(shot: { w: number; h: number; data: Uint8Array } | null, start: Pt, end: Pt): number {
  if (!shot) {
    return 0;
  }
  const left = Math.max(0, Math.min(start.x, end.x) - 8);
  const right = Math.min(shot.w - 1, Math.max(start.x, end.x) + 8);
  const top = Math.max(0, Math.min(start.y, end.y) - 8);
  const bottom = Math.min(shot.h - 1, Math.max(start.y, end.y) + 8);
  let count = 0;
  for (let y = top; y <= bottom; y++) {
    for (let x = left; x <= right; x++) {
      const off = (y * shot.w + x) * 4;
      const b = shot.data[off]!;
      const g = shot.data[off + 1]!;
      const r = shot.data[off + 2]!;
      if (b > 150 && b > g + 50 && g > r + 30 && r < 80) {
        count++;
      }
    }
  }
  return count;
}

// A pixel of the filter-match underlay: the accent (yellow) tinted over the
// row, so it is lighter than the accent but still clearly short of blue.
export function isFilterMatchTint(r: number, g: number, b: number): boolean {
  return r > 225 && g > 200 && b < 215 && r - b > 35;
}
