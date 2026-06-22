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
  GetWindowTextW: { args: [FFIType.ptr, FFIType.ptr, FFIType.i32], returns: FFIType.i32 },
  GetWindowRect: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  SetForegroundWindow: { args: [FFIType.ptr], returns: FFIType.bool },
  GetWindowDC: { args: [FFIType.ptr], returns: FFIType.u64 },
  ReleaseDC: { args: [FFIType.ptr, FFIType.u64], returns: FFIType.i32 },
  PrintWindow: { args: [FFIType.ptr, FFIType.u64, FFIType.u32], returns: FFIType.bool },
});

// GDI + GDI+ for capturing a window to a PNG (see captureWindowToPng). Capturing
// via PrintWindow works even when the window is occluded or not foreground,
// which screen-grabbing does not.
//
// HDC/HBITMAP/HGDIOBJ handles are u64 (bigint) here, NOT ptr: Bun's ptr return
// type sign-extends a 32-bit GDI handle whose high bit is set into a bogus
// 64-bit value, so the handle round-trips wrong and the calls fail intermittently.
const gdi32 = dlopen("gdi32.dll", {
  CreateCompatibleDC: { args: [FFIType.u64], returns: FFIType.u64 },
  CreateCompatibleBitmap: { args: [FFIType.u64, FFIType.i32, FFIType.i32], returns: FFIType.u64 },
  SelectObject: { args: [FFIType.u64, FFIType.u64], returns: FFIType.u64 },
  DeleteObject: { args: [FFIType.u64], returns: FFIType.bool },
  DeleteDC: { args: [FFIType.u64], returns: FFIType.bool },
});

const gdiplus = dlopen("gdiplus.dll", {
  GdiplusStartup: { args: [FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  GdipCreateBitmapFromHBITMAP: { args: [FFIType.u64, FFIType.u64, FFIType.ptr], returns: FFIType.u32 },
  GdipSaveImageToFile: { args: [FFIType.u64, FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  GdipDisposeImage: { args: [FFIType.u64], returns: FFIType.u32 },
});

// window messages
export const WM_SETTEXT = 0x000c;
export const WM_KEYDOWN = 0x0100;
export const WM_KEYUP = 0x0101;
export const WM_CHAR = 0x0102;
export const WM_MOUSEMOVE = 0x0200;
export const WM_LBUTTONDOWN = 0x0201;
export const WM_LBUTTONUP = 0x0202;
export const WM_RBUTTONDOWN = 0x0204;
export const WM_RBUTTONUP = 0x0205;
export const WM_MBUTTONDOWN = 0x0207;
export const WM_CONTEXTMENU = 0x007b;
export const WM_COMMAND = 0x0111;
// virtual-key / mouse-button flags
export const MK_LBUTTON = 0x0001;
export const MK_MBUTTON = 0x0010;
// virtual key codes
export const VK_TAB = 0x09;
export const VK_RETURN = 0x0d;
export const VK_ESCAPE = 0x1b;
export const VK_LEFT = 0x25;
export const VK_UP = 0x26;
export const VK_RIGHT = 0x27;
export const VK_DOWN = 0x28;
// PrintWindow flags
export const PW_RENDERFULLCONTENT = 0x00000002;
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

// a null-terminated UTF-16 (wide) string buffer, for LPCWSTR args
export function wideZ(s: string): Uint16Array {
  const buf = new Uint16Array(s.length + 1);
  for (let i = 0; i < s.length; i++) {
    buf[i] = s.charCodeAt(i);
  }
  buf[s.length] = 0;
  return buf;
}

export function getWindowText(hwnd: number): string {
  const buf = new Uint16Array(512);
  const n = user32.symbols.GetWindowTextW(hwnd, ptr(buf), 512);
  let s = "";
  for (let i = 0; i < n; i++) {
    s += String.fromCharCode(buf[i]);
  }
  return s;
}

// window rectangle in screen coordinates (vs getClientRect's client-relative one)
export function getWindowRect(hwnd: number): Rect {
  const buf = new Int32Array(4);
  user32.symbols.GetWindowRect(hwnd, ptr(buf));
  return { left: buf[0], top: buf[1], right: buf[2], bottom: buf[3] };
}

export function setForegroundWindow(hwnd: number): boolean {
  return user32.symbols.SetForegroundWindow(hwnd);
}

// set a window's text via WM_SETTEXT (works on edit controls cross-process,
// unlike SendInput typing). Synchronous, so the wide buffer stays alive.
export function sendText(hwnd: number, text: string): void {
  const buf = wideZ(text);
  sendMessage(hwnd, WM_SETTEXT, 0, ptr(buf));
}

// PNG encoder CLSID {557CF406-1A04-11D3-9A73-0000F81EF32E}
const PNG_ENCODER_CLSID = new Uint8Array([
  0x06, 0xf4, 0x7c, 0x55, 0x04, 0x1a, 0xd3, 0x11, 0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e,
]);

let gdiplusStarted = false;
function ensureGdiplus(): void {
  if (gdiplusStarted) {
    return;
  }
  const input = new Uint8Array(24); // GdiplusStartupInput, rest zeroed
  new DataView(input.buffer).setUint32(0, 1, true); // GdiplusVersion = 1
  const token = new BigUint64Array(1);
  const st = gdiplus.symbols.GdiplusStartup(ptr(token), ptr(input), 0);
  if (st === 0) {
    gdiplusStarted = true; // only mark done if it actually initialized
  }
}

// Capture a window (by its full bounds) to a PNG file via PrintWindow. Works
// for occluded / non-foreground / background windows -- the main reason to use
// this over a screen grab. Returns true on success.
export function captureWindowToPng(hwnd: number, outPath: string): boolean {
  ensureGdiplus();
  const r = getWindowRect(hwnd);
  const w = r.right - r.left;
  const h = r.bottom - r.top;
  if (w <= 0 || h <= 0) {
    return false;
  }
  const winDC = user32.symbols.GetWindowDC(hwnd);
  const memDC = gdi32.symbols.CreateCompatibleDC(winDC);
  const bmp = gdi32.symbols.CreateCompatibleBitmap(winDC, w, h);
  const oldObj = gdi32.symbols.SelectObject(memDC, bmp);
  user32.symbols.PrintWindow(hwnd, memDC, PW_RENDERFULLCONTENT);
  gdi32.symbols.SelectObject(memDC, oldObj);

  const gpBmp = new BigUint64Array(1);
  gdiplus.symbols.GdipCreateBitmapFromHBITMAP(bmp, 0n, ptr(gpBmp));
  const status = gdiplus.symbols.GdipSaveImageToFile(gpBmp[0], ptr(wideZ(outPath)), ptr(PNG_ENCODER_CLSID), 0);
  gdiplus.symbols.GdipDisposeImage(gpBmp[0]);

  gdi32.symbols.DeleteObject(bmp);
  gdi32.symbols.DeleteDC(memDC);
  user32.symbols.ReleaseDC(hwnd, winDC);
  return status === 0;
}
