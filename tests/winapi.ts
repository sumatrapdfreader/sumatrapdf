// Reusable Windows API wrappers for tests, via Bun's FFI (bun:ffi).
//
// Why this exists: driving SumatraPDF for verification means finding its windows
// and posting real window messages / reading scrollbar state cross-process.
// Injected mouse-button input is dropped on the test machine, but PostMessage,
// MoveWindow, SetCursorPos and cross-process GetScrollInfo all work (see the
// project memory env-gui-automation). These helpers wrap the handful of user32
// calls those tests need so individual test files don't each re-declare the FFI.
//
// Handles (HWND) are represented as JS `number`s here. That's fine for window
// handles in practice; do not use these helpers for arbitrary 64-bit pointers.

import { dlopen, FFIType, JSCallback, ptr } from "bun:ffi";

const user32 = dlopen("user32.dll", {
  EnumWindows: { args: [FFIType.function, FFIType.i64], returns: FFIType.bool },
  EnumChildWindows: { args: [FFIType.ptr, FFIType.function, FFIType.i64], returns: FFIType.bool },
  GetClassNameW: { args: [FFIType.ptr, FFIType.ptr, FFIType.i32], returns: FFIType.i32 },
  GetWindowThreadProcessId: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  PostMessageW: { args: [FFIType.ptr, FFIType.u32, FFIType.i64, FFIType.i64], returns: FFIType.bool },
  SendMessageW: { args: [FFIType.ptr, FFIType.u32, FFIType.i64, FFIType.i64], returns: FFIType.i64 },
  MoveWindow: { args: [FFIType.ptr, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.bool], returns: FFIType.bool },
  ShowWindow: { args: [FFIType.ptr, FFIType.i32], returns: FFIType.bool },
  GetClientRect: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  GetScrollInfo: { args: [FFIType.ptr, FFIType.i32, FFIType.ptr], returns: FFIType.bool },
  SetCursorPos: { args: [FFIType.i32, FFIType.i32], returns: FFIType.bool },
  ClientToScreen: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
});

// window messages
export const WM_MOUSEMOVE = 0x0200;
export const WM_MBUTTONDOWN = 0x0207;
export const WM_COMMAND = 0x0111;
// virtual-key / mouse-button flags
export const MK_MBUTTON = 0x0010;
// ShowWindow commands
export const SW_RESTORE = 9;
// scrollbar bars + SCROLLINFO flags
export const SB_HORZ = 0;
export const SB_VERT = 1;
const SIF_ALL = 0x17;
// TreeView (SysTreeView32) messages / flags
export const TVM_GETNEXTITEM = 0x110a;
export const TVM_EXPAND = 0x1102;
export const TVGN_ROOT = 0x0;
export const TVGN_NEXT = 0x1;
export const TVGN_CARET = 0x9;
export const TVGN_NEXTVISIBLE = 0x6;
export const TVE_COLLAPSE = 0x1;
export const TVE_EXPAND = 0x2;

export interface Rect {
  left: number;
  top: number;
  right: number;
  bottom: number;
}

export function sleep(ms: number): Promise<void> {
  return Bun.sleep(ms);
}

// pack client x/y into an LPARAM the way WM_MOUSE* messages expect
export function packCoords(x: number, y: number): number {
  return ((y & 0xffff) << 16) | (x & 0xffff);
}

export function getClassName(hwnd: number): string {
  const buf = new Uint16Array(256);
  const n = user32.symbols.GetClassNameW(hwnd, ptr(buf), 256);
  let s = "";
  for (let i = 0; i < n; i++) {
    s += String.fromCharCode(buf[i]);
  }
  return s;
}

export function getWindowPid(hwnd: number): number {
  const out = new Uint32Array(1);
  user32.symbols.GetWindowThreadProcessId(hwnd, ptr(out));
  return out[0];
}

// visit returns false to stop enumeration early
export function enumWindows(visit: (hwnd: number) => boolean): void {
  const cb = new JSCallback((hwnd: number) => visit(hwnd), {
    args: [FFIType.ptr, FFIType.i64],
    returns: FFIType.bool,
  });
  try {
    user32.symbols.EnumWindows(cb, 0n);
  } finally {
    cb.close();
  }
}

export function enumChildWindows(parent: number, visit: (hwnd: number) => boolean): void {
  const cb = new JSCallback((hwnd: number) => visit(hwnd), {
    args: [FFIType.ptr, FFIType.i64],
    returns: FFIType.bool,
  });
  try {
    user32.symbols.EnumChildWindows(parent, cb, 0n);
  } finally {
    cb.close();
  }
}

// find a top-level window of a given process and window class (0 if none)
export function findTopWindow(pid: number, className: string): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === className) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// find a descendant window of a given class (0 if none)
export function findChildWindow(parent: number, className: string): number {
  let found = 0;
  enumChildWindows(parent, (hwnd) => {
    if (getClassName(hwnd) === className) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

// poll for findTopWindow until it appears or timeout (returns 0 on timeout)
export async function waitForTopWindow(pid: number, className: string, timeoutMs = 12000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const h = findTopWindow(pid, className);
    if (h) {
      return h;
    }
    await sleep(200);
  }
  return 0;
}

export async function waitForChildWindow(parent: number, className: string, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const h = findChildWindow(parent, className);
    if (h) {
      return h;
    }
    await sleep(200);
  }
  return 0;
}

export function getClientRect(hwnd: number): Rect {
  const buf = new Int32Array(4);
  user32.symbols.GetClientRect(hwnd, ptr(buf));
  return { left: buf[0], top: buf[1], right: buf[2], bottom: buf[3] };
}

export function clientToScreen(hwnd: number, x: number, y: number): { x: number; y: number } {
  const buf = new Int32Array([x, y]);
  user32.symbols.ClientToScreen(hwnd, ptr(buf));
  return { x: buf[0], y: buf[1] };
}

// read the current scroll position (nPos) of a window's scrollbar
export function getScrollPos(hwnd: number, bar: number = SB_VERT): number {
  const buf = new Uint8Array(28);
  const dv = new DataView(buf.buffer);
  dv.setUint32(0, 28, true); // cbSize
  dv.setUint32(4, SIF_ALL, true); // fMask
  user32.symbols.GetScrollInfo(hwnd, bar, ptr(buf));
  return dv.getInt32(20, true); // nPos
}

export function postMessage(hwnd: number, msg: number, wParam: number, lParam: number): boolean {
  return user32.symbols.PostMessageW(hwnd, msg, BigInt(wParam), BigInt(lParam));
}

// SendMessage is synchronous: use it when you need the return value, or need the
// target window to finish handling the message before reading state. Returns the
// LRESULT as a bigint -- TreeView messages return HTREEITEM pointers that can
// exceed 2^53, so they must not be coerced to a JS number.
export function sendMessage(hwnd: number, msg: number, wParam: number | bigint, lParam: number | bigint): bigint {
  return user32.symbols.SendMessageW(hwnd, msg, BigInt(wParam), BigInt(lParam)) as bigint;
}

// --- TreeView (SysTreeView32) helpers; item handles are opaque bigints ---

export function treeGetNextItem(tree: number, flag: number, item: bigint = 0n): bigint {
  return sendMessage(tree, TVM_GETNEXTITEM, flag, item);
}

export function treeGetRoot(tree: number): bigint {
  return treeGetNextItem(tree, TVGN_ROOT, 0n);
}

export function treeGetSelection(tree: number): bigint {
  return treeGetNextItem(tree, TVGN_CARET, 0n);
}

export function treeExpand(tree: number, action: number, item: bigint): void {
  sendMessage(tree, TVM_EXPAND, action, item);
}

// number of currently-visible (i.e. expanded-into-view) rows in the tree
export function countVisibleTreeRows(tree: number): number {
  let n = 0;
  let it = treeGetRoot(tree);
  while (it !== 0n && n <= 100000) {
    n++;
    it = treeGetNextItem(tree, TVGN_NEXTVISIBLE, it);
  }
  return n;
}

// collapse every top-level node
export function collapseTreeRoots(tree: number): void {
  let it = treeGetRoot(tree);
  while (it !== 0n) {
    treeExpand(tree, TVE_COLLAPSE, it);
    it = treeGetNextItem(tree, TVGN_NEXT, it);
  }
}

export function moveWindow(hwnd: number, x: number, y: number, w: number, h: number, repaint = true): boolean {
  return user32.symbols.MoveWindow(hwnd, x, y, w, h, repaint);
}

export function showWindow(hwnd: number, cmd: number): boolean {
  return user32.symbols.ShowWindow(hwnd, cmd);
}

export function setCursorPos(x: number, y: number): boolean {
  return user32.symbols.SetCursorPos(x, y);
}
