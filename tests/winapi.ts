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

import { dlopen, FFIType, JSCallback, ptr, toArrayBuffer } from "bun:ffi";

const user32 = dlopen("user32.dll", {
  EnumWindows: { args: [FFIType.function, FFIType.i64], returns: FFIType.bool },
  EnumChildWindows: { args: [FFIType.ptr, FFIType.function, FFIType.i64], returns: FFIType.bool },
  GetClassNameW: { args: [FFIType.ptr, FFIType.ptr, FFIType.i32], returns: FFIType.i32 },
  GetWindowThreadProcessId: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  PostMessageW: { args: [FFIType.ptr, FFIType.u32, FFIType.i64, FFIType.i64], returns: FFIType.bool },
  SendMessageW: { args: [FFIType.ptr, FFIType.u32, FFIType.i64, FFIType.i64], returns: FFIType.i64 },
  MoveWindow: {
    args: [FFIType.ptr, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.bool],
    returns: FFIType.bool,
  },
  SetWindowPos: {
    args: [FFIType.ptr, FFIType.ptr, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.i32, FFIType.u32],
    returns: FFIType.bool,
  },
  GetWindowLongW: { args: [FFIType.ptr, FFIType.i32], returns: FFIType.i32 },
  SetWindowLongW: { args: [FFIType.ptr, FFIType.i32, FFIType.i32], returns: FFIType.i32 },
  ShowWindow: { args: [FFIType.ptr, FFIType.i32], returns: FFIType.bool },
  IsZoomed: { args: [FFIType.ptr], returns: FFIType.bool },
  GetClientRect: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  GetScrollInfo: { args: [FFIType.ptr, FFIType.i32, FFIType.ptr], returns: FFIType.bool },
  SetCursorPos: { args: [FFIType.i32, FFIType.i32], returns: FFIType.bool },
  ClientToScreen: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  GetWindowTextW: { args: [FFIType.ptr, FFIType.ptr, FFIType.i32], returns: FFIType.i32 },
  GetWindowRect: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.bool },
  IsWindowVisible: { args: [FFIType.ptr], returns: FFIType.bool },
  SetForegroundWindow: { args: [FFIType.ptr], returns: FFIType.bool },
  GetForegroundWindow: { args: [], returns: FFIType.u64 },
  GetWindowDC: { args: [FFIType.ptr], returns: FFIType.u64 },
  ReleaseDC: { args: [FFIType.ptr, FFIType.u64], returns: FFIType.i32 },
  PrintWindow: { args: [FFIType.ptr, FFIType.u64, FFIType.u32], returns: FFIType.bool },
  GetSystemMetrics: { args: [FFIType.i32], returns: FFIType.i32 },
  SystemParametersInfoW: { args: [FFIType.u32, FFIType.u32, FFIType.ptr, FFIType.u32], returns: FFIType.bool },
  SetProcessDpiAwarenessContext: { args: [FFIType.i64], returns: FFIType.bool },
  GetGUIThreadInfo: { args: [FFIType.u32, FFIType.ptr], returns: FFIType.bool },
  GetCursorInfo: { args: [FFIType.ptr], returns: FFIType.bool },
  GetIconInfo: { args: [FFIType.u64, FFIType.ptr], returns: FFIType.bool },
  DrawIconEx: {
    args: [
      FFIType.u64,
      FFIType.i32,
      FFIType.i32,
      FFIType.u64,
      FFIType.i32,
      FFIType.i32,
      FFIType.u32,
      FFIType.u64,
      FFIType.u32,
    ],
    returns: FFIType.bool,
  },
  FillRect: { args: [FFIType.u64, FFIType.ptr, FFIType.u64], returns: FFIType.i32 },
  GetMenuItemCount: { args: [FFIType.u64], returns: FFIType.i32 },
  GetSubMenu: { args: [FFIType.u64, FFIType.i32], returns: FFIType.u64 },
  GetMenuStringW: { args: [FFIType.u64, FFIType.u32, FFIType.ptr, FFIType.i32, FFIType.u32], returns: FFIType.i32 },
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
  BitBlt: {
    args: [
      FFIType.u64,
      FFIType.i32,
      FFIType.i32,
      FFIType.i32,
      FFIType.i32,
      FFIType.u64,
      FFIType.i32,
      FFIType.i32,
      FFIType.u32,
    ],
    returns: FFIType.bool,
  },
  StretchBlt: {
    args: [
      FFIType.u64,
      FFIType.i32,
      FFIType.i32,
      FFIType.i32,
      FFIType.i32,
      FFIType.u64,
      FFIType.i32,
      FFIType.i32,
      FFIType.i32,
      FFIType.i32,
      FFIType.u32,
    ],
    returns: FFIType.bool,
  },
  SetStretchBltMode: { args: [FFIType.u64, FFIType.i32], returns: FFIType.i32 },
  GetPixel: { args: [FFIType.u64, FFIType.i32, FFIType.i32], returns: FFIType.u32 },
  CreateSolidBrush: { args: [FFIType.u32], returns: FFIType.u64 },
  GetObjectW: { args: [FFIType.u64, FFIType.i32, FFIType.ptr], returns: FFIType.i32 },
  CreateDIBSection: {
    args: [FFIType.u64, FFIType.ptr, FFIType.u32, FFIType.ptr, FFIType.u64, FFIType.u32],
    returns: FFIType.u64,
  },
});

const shell32 = dlopen("shell32.dll", {
  SHAppBarMessage: { args: [FFIType.u32, FFIType.ptr], returns: FFIType.u64 },
});

const gdiplus = dlopen("gdiplus.dll", {
  GdiplusStartup: { args: [FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  GdipCreateBitmapFromHBITMAP: { args: [FFIType.u64, FFIType.u64, FFIType.ptr], returns: FFIType.u32 },
  GdipSaveImageToFile: { args: [FFIType.u64, FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
  GdipDisposeImage: { args: [FFIType.u64], returns: FFIType.u32 },
});

const kernel32 = dlopen("kernel32.dll", {
  CreateProcessW: {
    args: [
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.bool,
      FFIType.u32,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
    ],
    returns: FFIType.bool,
  },
  CreateToolhelp32Snapshot: { args: [FFIType.u32, FFIType.u32], returns: FFIType.u64 },
  Process32FirstW: { args: [FFIType.u64, FFIType.ptr], returns: FFIType.bool },
  Process32NextW: { args: [FFIType.u64, FFIType.ptr], returns: FFIType.bool },
  OpenProcess: { args: [FFIType.u32, FFIType.bool, FFIType.u32], returns: FFIType.u64 },
  TerminateProcess: { args: [FFIType.u64, FFIType.u32], returns: FFIType.bool },
  WaitForSingleObject: { args: [FFIType.u64, FFIType.u32], returns: FFIType.u32 },
  CloseHandle: { args: [FFIType.u64], returns: FFIType.bool },
  GetLastError: { args: [], returns: FFIType.u32 },
});

// Authenticode helpers (mirror src/base/Crypto_win.cpp GetExecutableSignerTemp / IsPEFileSigned).
// crypt32 for embedded PKCS#7 signer name; wintrust for signature validity.
const crypt32 = dlopen("crypt32.dll", {
  CryptQueryObject: {
    args: [
      FFIType.u32,
      FFIType.ptr,
      FFIType.u32,
      FFIType.u32,
      FFIType.u32,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
      FFIType.ptr,
    ],
    returns: FFIType.bool,
  },
  CryptMsgGetParam: {
    args: [FFIType.u64, FFIType.u32, FFIType.u32, FFIType.ptr, FFIType.ptr],
    returns: FFIType.bool,
  },
  CryptMsgClose: { args: [FFIType.u64], returns: FFIType.bool },
  CertCloseStore: { args: [FFIType.u64, FFIType.u32], returns: FFIType.bool },
  CertFindCertificateInStore: {
    args: [FFIType.u64, FFIType.u32, FFIType.u32, FFIType.u32, FFIType.ptr, FFIType.ptr],
    returns: FFIType.ptr,
  },
  CertGetNameStringA: {
    args: [FFIType.ptr, FFIType.u32, FFIType.u32, FFIType.ptr, FFIType.ptr, FFIType.u32],
    returns: FFIType.u32,
  },
  CertFreeCertificateContext: { args: [FFIType.ptr], returns: FFIType.bool },
});

const wintrust = dlopen("wintrust.dll", {
  WinVerifyTrust: { args: [FFIType.ptr, FFIType.ptr, FFIType.ptr], returns: FFIType.i32 },
});

const CERT_QUERY_OBJECT_FILE = 1;
const CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED = 0x400;
const CERT_QUERY_FORMAT_FLAG_BINARY = 2;
const CMSG_SIGNER_INFO_PARAM = 6;
const X509_ASN_ENCODING = 0x1;
const PKCS_7_ASN_ENCODING = 0x10000;
const CERT_FIND_SUBJECT_CERT = 11 << 16; // CERT_COMPARE_SUBJECT_CERT << CERT_COMPARE_SHIFT
const CERT_NAME_SIMPLE_DISPLAY_TYPE = 4;

// WINTRUST_ACTION_GENERIC_VERIFY_V2 = {00AAC56B-CD44-11d0-8CC2-00C04FC295EE}
const WINTRUST_ACTION_GENERIC_VERIFY_V2 = new Uint8Array([
  0x6b, 0xc5, 0xaa, 0x00, 0x44, 0xcd, 0xd0, 0x11, 0x8c, 0xc2, 0x00, 0xc0, 0x4f, 0xc2, 0x95, 0xee,
]);
const WTD_UI_NONE = 2;
const WTD_REVOKE_NONE = 0;
const WTD_CHOICE_FILE = 1;
const WTD_STATEACTION_IGNORE = 0;
const WTD_SAFER_FLAG = 0x100;
const ERROR_SUCCESS = 0;

// CreateProcess creation flags
const DETACHED_PROCESS = 0x00000008;
const CREATE_NEW_PROCESS_GROUP = 0x00000200;

// window messages
export const WM_CLOSE = 0x0010;
export const WM_DISPLAYCHANGE = 0x007e;
export const WM_SETTEXT = 0x000c;
export const WM_GETTEXT = 0x000d;
export const WM_GETTEXTLENGTH = 0x000e;
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
export const WM_COPYDATA = 0x004a;
// virtual-key / mouse-button flags
export const MK_LBUTTON = 0x0001;
export const MK_RBUTTON = 0x0002;
export const MK_SHIFT = 0x0004;
export const MK_CONTROL = 0x0008;
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
export const PW_CLIENTONLY = 0x00000001;
export const PW_RENDERFULLCONTENT = 0x00000002;
export const SRCCOPY = 0x00cc0020;
export const COLORONCOLOR = 3; // StretchBlt mode: no smoothing
// ShowWindow commands
export const SW_RESTORE = 9;
export const SW_MAXIMIZE = 3;
// GetSystemMetrics indices
export const SM_CXVSCROLL = 2;
export const SM_CYHSCROLL = 3;
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

// a window position the way MoveWindow and SumatraPDF's -window-pos want it
export interface WindowPos {
  x: number;
  y: number;
  dx: number;
  dy: number;
}

const SPI_GETWORKAREA = 0x0030;
const SM_CXSCREEN = 0;
const SM_CYSCREEN = 1;

// the desktop minus the taskbar (and any other appbar)
export function getWorkArea(): Rect {
  const buf = new Int32Array(4);
  if (!user32.symbols.SystemParametersInfoW(SPI_GETWORKAREA, 0, ptr(buf), 0)) {
    // no work area to be had: fall back to the whole primary screen
    return { left: 0, top: 0, right: getSystemMetrics(SM_CXSCREEN), bottom: getSystemMetrics(SM_CYSCREEN) };
  }
  return { left: buf[0]!, top: buf[1]!, right: buf[2]!, bottom: buf[3]! };
}

// A cheap fingerprint of what a window is showing: every 16th pixel, mixed
// together. Two captures with the same fingerprint are the same picture for
// our purposes, and sampling keeps it to ~30k reads instead of ~500k
export function windowFingerprint(hwnd: number): number {
  const cap = captureWindowPixels(hwnd);
  if (!cap) {
    return 0;
  }
  let h = 2166136261;
  let distinct = 0;
  let first = -1;
  for (let i = 0; i < cap.data.length; i += 64) {
    const v = cap.data[i]! | (cap.data[i + 1]! << 8) | (cap.data[i + 2]! << 16);
    if (first < 0) {
      first = v;
    } else if (v !== first && distinct === 0) {
      distinct = 1;
    }
    h = Math.imul(h ^ v, 16777619) >>> 0;
  }
  // 0 means "nothing to see": an empty capture, or one flat color (a window
  // that hasn't painted its document yet)
  return distinct ? h || 1 : 0;
}

// Wait until a window has painted something and stopped changing - which is
// what "the document finished loading and rendering" looks like from outside.
// Returns false if it never settled, so the caller can carry on rather than
// hang. Replaces sleeping for the worst case
export async function waitForWindowIdle(hwnd: number, timeoutMs = 5000, settleMs = 120): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  let prev = -1;
  let stableSince = 0;
  while (Date.now() < deadline) {
    const fp = windowFingerprint(hwnd);
    if (fp !== 0 && fp === prev) {
      if (stableSince === 0) {
        stableSince = Date.now();
      } else if (Date.now() - stableSince >= settleMs) {
        return true;
      }
    } else {
      stableSince = 0;
    }
    prev = fp;
    await sleep(40);
  }
  return false;
}

// The upper-right quarter of the work area. Tests put SumatraPDF there: a
// window of that size renders and, more to the point, captures a quarter of
// the pixels a default-sized one does, and every captureWindowPixels() walk
// over the result costs a quarter as much. Upper-right keeps it clear of the
// taskbar's usual place and of anything at the top-left of the desktop
export function testWindowPos(): WindowPos {
  const wa = getWorkArea();
  const dx = Math.floor((wa.right - wa.left) / 2);
  const dy = Math.floor((wa.bottom - wa.top) / 2);
  return { x: wa.left + dx, y: wa.top, dx, dy };
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

// Which window has keyboard focus in the thread that owns hwnd. GetFocus() only
// answers for the calling thread, so a test in another process must go through
// GUITHREADINFO { DWORD cbSize, flags; HWND active, focus, capture, menuOwner,
// moveSize, caret; RECT rcCaret; } = 72 bytes on x64, focus at offset 16.
export function getFocusedHwnd(hwndInSameThread: number): number {
  const tid = user32.symbols.GetWindowThreadProcessId(hwndInSameThread, null);
  const buf = new ArrayBuffer(72);
  const dv = new DataView(buf);
  dv.setUint32(0, 72, true);
  if (!user32.symbols.GetGUIThreadInfo(tid, ptr(buf))) {
    return 0;
  }
  return Number(dv.getBigUint64(16, true));
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

// full SCROLLINFO for a window scrollbar (SIF_ALL)
export function getScrollInfo(
  hwnd: number,
  bar: number = SB_VERT,
): { min: number; max: number; page: number; pos: number; trackPos: number } {
  const buf = new Uint8Array(28);
  const dv = new DataView(buf.buffer);
  dv.setUint32(0, 28, true); // cbSize
  dv.setUint32(4, SIF_ALL, true); // fMask
  user32.symbols.GetScrollInfo(hwnd, bar, ptr(buf));
  return {
    min: dv.getInt32(8, true), // nMin
    max: dv.getInt32(12, true), // nMax
    page: dv.getUint32(16, true), // nPage
    pos: dv.getInt32(20, true), // nPos
    trackPos: dv.getInt32(24, true), // nTrackPos
  };
}

// read the current scroll position (nPos) of a window's scrollbar
export function getScrollPos(hwnd: number, bar: number = SB_VERT): number {
  return getScrollInfo(hwnd, bar).pos;
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

// Send a null-terminated UTF-16 WM_COPYDATA payload. COPYDATASTRUCT is 24
// bytes on x64: ULONG_PTR dwData, DWORD cbData + padding, PVOID lpData.
export function sendCopyDataW(hwnd: number, dataId: number, text: string): bigint {
  const payload = wideZ(text);
  const cds = new Uint8Array(24);
  const view = new DataView(cds.buffer);
  view.setBigUint64(0, BigInt(dataId), true);
  view.setUint32(8, payload.byteLength, true);
  view.setBigUint64(16, BigInt(ptr(payload)), true);
  return sendMessage(hwnd, WM_COPYDATA, 0, BigInt(ptr(cds)));
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

// SWP_* flags (subset)
export const SWP_NOZORDER = 0x0004;
export const SWP_NOACTIVATE = 0x0010;
export const SWP_FRAMECHANGED = 0x0020;
export const GWL_STYLE = -16;
export const WS_MAXIMIZE = 0x01000000;

export function getWindowLong(hwnd: number, index: number): number {
  return user32.symbols.GetWindowLongW(hwnd, index);
}

export function setWindowLong(hwnd: number, index: number, value: number): number {
  return user32.symbols.SetWindowLongW(hwnd, index, value);
}

export function setWindowPos(
  hwnd: number,
  x: number,
  y: number,
  w: number,
  h: number,
  flags = SWP_NOZORDER | SWP_NOACTIVATE,
): boolean {
  return user32.symbols.SetWindowPos(hwnd, 0, x, y, w, h, flags);
}

export function showWindow(hwnd: number, cmd: number): boolean {
  return user32.symbols.ShowWindow(hwnd, cmd);
}

export function isZoomed(hwnd: number): boolean {
  return user32.symbols.IsZoomed(hwnd);
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

// Text of a control (Edit, Static, ...) in another process. GetWindowTextW only
// returns the caption of foreign windows, so child controls come back empty --
// WM_GETTEXT is marshalled across processes by user32 and does work.
export function getControlText(hwnd: number, maxChars = 1 << 20): string {
  const n = Number(sendMessage(hwnd, WM_GETTEXTLENGTH, 0, 0));
  if (n <= 0) {
    return "";
  }
  const cap = Math.min(n + 1, maxChars);
  const buf = new Uint16Array(cap);
  const got = Number(sendMessage(hwnd, WM_GETTEXT, cap, BigInt(ptr(buf))));
  let s = "";
  for (let i = 0; i < got; i++) {
    s += String.fromCharCode(buf[i]);
  }
  return s;
}

// Full window text (large buffer). For child controls of another process use
// getControlText instead.
export function getWindowTextFull(hwnd: number, maxChars = 16384): string {
  const buf = new Uint16Array(maxChars);
  const n = user32.symbols.GetWindowTextW(hwnd, ptr(buf), maxChars);
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

// A hidden window keeps its last rect, so getWindowRect can't tell you whether a
// control is actually on screen (e.g. layout code that collapses a control hides
// it rather than resizing it to nothing). Ask the OS instead.
export function isWindowVisible(hwnd: number): boolean {
  return user32.symbols.IsWindowVisible(hwnd);
}

export function setForegroundWindow(hwnd: number): boolean {
  return user32.symbols.SetForegroundWindow(hwnd);
}

// the window the user is currently working with (across processes). Use it to
// assert that something didn't steal activation
export function getForegroundWindow(): number {
  return Number(user32.symbols.GetForegroundWindow());
}

export function getSystemMetrics(index: number): number {
  return user32.symbols.GetSystemMetrics(index);
}

// Opt this process into per-monitor-v2 DPI awareness. Bun is DPI-virtualized by
// default, which makes window rects and window-DC pixel coordinates disagree
// when reading a DPI-aware window like SumatraPDF's on a scaled display (see
// project memory dpi-aware-probe-trick). No-op at 100% scaling.
//
// This is process-wide and cannot be undone, so only call it from tests that
// read pixels or physical geometry, not from every test.
export function setProcessDpiAware(): boolean {
  // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
  return user32.symbols.SetProcessDpiAwarenessContext(-4n as unknown as number);
}

// Taskbar auto-hide, via SHAppBarMessage. Takes effect immediately (no Explorer
// restart). Worth toggling in a test because auto-hide makes the monitor work
// area equal the whole screen, so a MAXIMIZED window's client rect matches a
// full-screen one -- which is a distinct code path in SumatraPDF's
// ExitFullScreen (issue #5866). Always restore the previous value in a finally.
const ABM_GETSTATE = 0x4;
const ABM_SETSTATE = 0xa;
const ABS_AUTOHIDE = 0x1;
const ABS_ALWAYSONTOP = 0x2;

function appBarData(lParam: number): Uint8Array {
  const buf = new Uint8Array(48); // x64 APPBARDATA
  const dv = new DataView(buf.buffer);
  dv.setUint32(0, 48, true); // cbSize
  dv.setBigInt64(40, BigInt(lParam), true); // lParam
  return buf;
}

function taskbarState(): number {
  return Number(shell32.symbols.SHAppBarMessage(ABM_GETSTATE, ptr(appBarData(0))));
}

export function getTaskbarAutoHide(): boolean {
  return (taskbarState() & ABS_AUTOHIDE) !== 0;
}

export function setTaskbarAutoHide(on: boolean): void {
  // preserve always-on-top; only flip the auto-hide bit
  const onTop = taskbarState() & ABS_ALWAYSONTOP;
  const next = on ? ABS_AUTOHIDE | onTop : onTop;
  shell32.symbols.SHAppBarMessage(ABM_SETSTATE, ptr(appBarData(next)));
}

// GetPixel returns this (DWORD)-1 when it cannot read the pixel. Callers that
// compare samples must treat it as "no data", not as a color (issue #5866 test
// was failing on these alone while the UI looked correct).
export const CLR_INVALID = 0xffffffff;

// Read a vertical run of pixels straight out of a window's DC, as 0x00bbggrr
// COLORREFs. Coordinates are window-relative, so 0,0 is the top-left of the
// NON-client area -- which is what makes this usable for checking that native
// scrollbars actually painted (PrintWindow leaves the NC area blank).
// Retries briefly on CLR_INVALID: concurrent DWM/paint can make GetPixel fail
// for a frame without meaning the chrome is wrong.
export function readWindowDCColumn(hwnd: number, x: number, y: number, count: number): number[] {
  const dc = user32.symbols.GetWindowDC(hwnd);
  const out: number[] = [];
  for (let i = 0; i < count; i++) {
    let c = gdi32.symbols.GetPixel(dc, x, y + i) >>> 0;
    for (let attempt = 0; c === CLR_INVALID && attempt < 4; attempt++) {
      c = gdi32.symbols.GetPixel(dc, x, y + i) >>> 0;
    }
    out.push(c);
  }
  user32.symbols.ReleaseDC(hwnd, dc);
  return out;
}

// Like readWindowDCColumn but a horizontal run of pixels. Handy for checking
// that centered text (a "Couldn't render page N" message, a notification, ...)
// actually painted: scan the rows around the middle and count non-background
// pixels.
export function readWindowDCRow(hwnd: number, x: number, y: number, count: number): number[] {
  const dc = user32.symbols.GetWindowDC(hwnd);
  const out: number[] = [];
  for (let i = 0; i < count; i++) {
    let c = gdi32.symbols.GetPixel(dc, x + i, y) >>> 0;
    for (let attempt = 0; c === CLR_INVALID && attempt < 4; attempt++) {
      c = gdi32.symbols.GetPixel(dc, x + i, y) >>> 0;
    }
    out.push(c);
  }
  user32.symbols.ReleaseDC(hwnd, dc);
  return out;
}

// Grab a window's client-area pixels via PrintWindow into a 32bpp DIB and hand
// them back as BGRA bytes (4 per pixel, top-down). Unlike GetPixel on a window
// DC this works for occluded / background windows, and unlike captureWindowToPng
// it needs no PNG decoder to assert on what was painted.
export function captureWindowPixels(hwnd: number): { w: number; h: number; data: Uint8Array } | null {
  const rc = getClientRect(hwnd);
  const w = rc.right - rc.left;
  const h = rc.bottom - rc.top;
  if (w <= 0 || h <= 0) {
    return null;
  }
  const screenDC = user32.symbols.GetWindowDC(0);
  const memDC = gdi32.symbols.CreateCompatibleDC(screenDC);
  // BITMAPINFOHEADER, negative height = top-down rows
  const bmi = new ArrayBuffer(40);
  const dv = new DataView(bmi);
  dv.setUint32(0, 40, true); // biSize
  dv.setInt32(4, w, true); // biWidth
  dv.setInt32(8, -h, true); // biHeight
  dv.setUint16(12, 1, true); // biPlanes
  dv.setUint16(14, 32, true); // biBitCount
  dv.setUint32(16, 0, true); // biCompression = BI_RGB
  const bitsPtr = new BigUint64Array(1);
  const bmp = gdi32.symbols.CreateDIBSection(memDC, ptr(bmi), 0 /*DIB_RGB_COLORS*/, ptr(bitsPtr), 0n, 0);
  if (!bmp || !bitsPtr[0]) {
    gdi32.symbols.DeleteDC(memDC);
    user32.symbols.ReleaseDC(0, screenDC);
    return null;
  }
  const oldObj = gdi32.symbols.SelectObject(memDC, bmp);
  user32.symbols.PrintWindow(hwnd, memDC, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
  gdi32.symbols.SelectObject(memDC, oldObj);

  // bun wants the pointer as a number, not the bigint CreateDIBSection wrote
  const nBytes = w * h * 4;
  const bits = Number(bitsPtr[0]) as unknown as Parameters<typeof toArrayBuffer>[0];
  const data = new Uint8Array(toArrayBuffer(bits, 0, nBytes)).slice();

  gdi32.symbols.DeleteObject(bmp);
  gdi32.symbols.DeleteDC(memDC);
  user32.symbols.ReleaseDC(0, screenDC);
  return { w, h, data };
}

// Toolbar (ToolbarWindow32) helpers. A button is addressed by its command id.
export const TB_GETSTATE = 0x0412;
export const TB_COMMANDTOINDEX = 0x0419;
export const TBSTATE_HIDDEN = 0x08;

// button state bits, or -1 when the toolbar has no button for that command
export function tbGetState(toolbar: number, cmdId: number): number {
  return Number(sendMessage(toolbar, TB_GETSTATE, BigInt(cmdId), 0n));
}

export function tbIsButtonVisible(toolbar: number, cmdId: number): boolean {
  const state = tbGetState(toolbar, cmdId);
  return state >= 0 && (state & TBSTATE_HIDDEN) === 0;
}

// position of a button in the toolbar, -1 when there is no button for that
// command. Messages that write through a pointer (TB_GETRECT) can't be used
// across processes, but this one only returns a number.
export function tbGetButtonIndex(toolbar: number, cmdId: number): number {
  return Number(sendMessage(toolbar, TB_COMMANDTOINDEX, BigInt(cmdId), 0n));
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

// Capture a window by BitBlt-ing from its window DC instead of PrintWindow.
// Use this to verify NON-CLIENT drawing (native scrollbars, custom NC frames):
// PrintWindow re-renders the window's content and leaves the NC area blank,
// while the window DC holds what is actually on screen for that window. Under
// DWM this works for background windows too, but NOT for minimized ones.
export function captureWindowDCToPng(hwnd: number, outPath: string): boolean {
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
  gdi32.symbols.BitBlt(memDC, 0, 0, w, h, winDC, 0, 0, SRCCOPY);
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

// Like captureWindowDCToPng but only a sub-rect (window-relative, so 0,0 is the
// top-left of the NON-client area), optionally magnified `zoom` times with
// nearest-neighbour so thin things like a scrollbar are legible in the PNG.
export function captureWindowDCRegionToPng(
  hwnd: number,
  x: number,
  y: number,
  w: number,
  h: number,
  outPath: string,
  zoom = 1,
): boolean {
  ensureGdiplus();
  if (w <= 0 || h <= 0) {
    return false;
  }
  const winDC = user32.symbols.GetWindowDC(hwnd);
  const memDC = gdi32.symbols.CreateCompatibleDC(winDC);
  const bmp = gdi32.symbols.CreateCompatibleBitmap(winDC, w * zoom, h * zoom);
  const oldObj = gdi32.symbols.SelectObject(memDC, bmp);
  gdi32.symbols.SetStretchBltMode(memDC, COLORONCOLOR);
  gdi32.symbols.StretchBlt(memDC, 0, 0, w * zoom, h * zoom, winDC, x, y, w, h, SRCCOPY);
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

// The cursor shape currently displayed on the desktop, as an HCURSOR (0 when
// the cursor is hidden). It is a per-desktop global, so this sees the shape the
// app under test asked for with SetCursor() even from another process.
export function getCurrentCursor(): bigint {
  // CURSORINFO { DWORD cbSize; DWORD flags; HCURSOR hCursor; POINT ptScreenPos; }
  const ci = new BigUint64Array(4);
  new Uint32Array(ci.buffer)[0] = 8 + 8 + 8; // cbSize (with padding after flags)
  if (!user32.symbols.GetCursorInfo(ptr(ci))) {
    return 0n;
  }
  return ci[1];
}

// Size in pixels of a cursor's image, from its color (or mask) bitmap.
export function getCursorSize(hcursor: bigint): { dx: number; dy: number } {
  // ICONINFO { BOOL fIcon; DWORD xHotspot; DWORD yHotspot; HBITMAP hbmMask; HBITMAP hbmColor; }
  const ii = new BigUint64Array(4);
  if (!user32.symbols.GetIconInfo(hcursor, ptr(ii))) {
    return { dx: 0, dy: 0 };
  }
  const hbmMask = ii[2];
  const hbmColor = ii[3];
  // BITMAP { LONG bmType; LONG bmWidth; LONG bmHeight; ... }
  const bm = new Int32Array(12);
  const hbm = hbmColor !== 0n ? hbmColor : hbmMask;
  gdi32.symbols.GetObjectW(hbm, 4 * 12, ptr(bm));
  const dx = bm[1];
  // a monochrome cursor's mask holds the AND and XOR halves stacked
  const dy = hbmColor !== 0n ? bm[2] : bm[2] / 2;
  if (hbmMask !== 0n) {
    gdi32.symbols.DeleteObject(hbmMask);
  }
  if (hbmColor !== 0n) {
    gdi32.symbols.DeleteObject(hbmColor);
  }
  return { dx, dy };
}

// Draw a cursor onto white and black squares side by side and save it as a PNG,
// magnified `zoom` times. Two backgrounds because a cursor with a soft alpha
// edge (e.g. the laser pointer dot) looks different over light and dark pages.
export function captureCursorToPng(hcursor: bigint, outPath: string, zoom = 4): boolean {
  ensureGdiplus();
  const { dx, dy } = getCursorSize(hcursor);
  if (dx <= 0 || dy <= 0) {
    return false;
  }
  const w = dx * 2;
  const h = dy;
  const screenDC = user32.symbols.GetWindowDC(0);
  const memDC = gdi32.symbols.CreateCompatibleDC(screenDC);
  const bmp = gdi32.symbols.CreateCompatibleBitmap(screenDC, w, h);
  const oldObj = gdi32.symbols.SelectObject(memDC, bmp);

  const fill = (x0: number, x1: number, color: number) => {
    const rc = new Int32Array([x0, 0, x1, h]);
    const brush = gdi32.symbols.CreateSolidBrush(color);
    user32.symbols.FillRect(memDC, ptr(rc), brush);
    gdi32.symbols.DeleteObject(brush);
  };
  fill(0, dx, 0x00ffffff); // white
  fill(dx, w, 0x00000000); // black

  const DI_NORMAL = 3;
  user32.symbols.DrawIconEx(memDC, 0, 0, hcursor, 0, 0, 0, 0n, DI_NORMAL);
  user32.symbols.DrawIconEx(memDC, dx, 0, hcursor, 0, 0, 0, 0n, DI_NORMAL);
  gdi32.symbols.SelectObject(memDC, oldObj);

  const zoomDC = gdi32.symbols.CreateCompatibleDC(screenDC);
  const zoomBmp = gdi32.symbols.CreateCompatibleBitmap(screenDC, w * zoom, h * zoom);
  const oldZoom = gdi32.symbols.SelectObject(zoomDC, zoomBmp);
  gdi32.symbols.SetStretchBltMode(zoomDC, COLORONCOLOR);
  gdi32.symbols.SelectObject(memDC, bmp);
  gdi32.symbols.StretchBlt(zoomDC, 0, 0, w * zoom, h * zoom, memDC, 0, 0, w, h, SRCCOPY);
  gdi32.symbols.SelectObject(memDC, oldObj);
  gdi32.symbols.SelectObject(zoomDC, oldZoom);

  const gpBmp = new BigUint64Array(1);
  gdiplus.symbols.GdipCreateBitmapFromHBITMAP(zoomBmp, 0n, ptr(gpBmp));
  const status = gdiplus.symbols.GdipSaveImageToFile(gpBmp[0], ptr(wideZ(outPath)), ptr(PNG_ENCODER_CLSID), 0);
  gdiplus.symbols.GdipDisposeImage(gpBmp[0]);

  gdi32.symbols.DeleteObject(zoomBmp);
  gdi32.symbols.DeleteDC(zoomDC);
  gdi32.symbols.DeleteObject(bmp);
  gdi32.symbols.DeleteDC(memDC);
  user32.symbols.ReleaseDC(0, screenDC);
  return status === 0;
}

// Simple display name of the Authenticode signer (e.g. "Krzysztof Kowalczyk",
// "Microsoft Windows"), or null if the file is unsigned / unreadable.
// Mirrors GetExecutableSignerTemp in src/base/Crypto_win.cpp.
export function getExecutableSigner(filePath: string): string | null {
  const pathW = wideZ(filePath);
  const hStore = new BigUint64Array(1);
  const hMsg = new BigUint64Array(1);
  const ok = crypt32.symbols.CryptQueryObject(
    CERT_QUERY_OBJECT_FILE,
    ptr(pathW),
    CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
    CERT_QUERY_FORMAT_FLAG_BINARY,
    0,
    null,
    null,
    null,
    ptr(hStore),
    ptr(hMsg),
    null,
  );
  if (!ok || hStore[0] === 0n || hMsg[0] === 0n) {
    return null;
  }

  try {
    const sizeBuf = new Uint32Array(1);
    if (!crypt32.symbols.CryptMsgGetParam(hMsg[0], CMSG_SIGNER_INFO_PARAM, 0, null, ptr(sizeBuf)) || sizeBuf[0] === 0) {
      return null;
    }
    const signerInfo = new Uint8Array(sizeBuf[0]);
    if (!crypt32.symbols.CryptMsgGetParam(hMsg[0], CMSG_SIGNER_INFO_PARAM, 0, ptr(signerInfo), ptr(sizeBuf))) {
      return null;
    }

    // x64 CMSG_SIGNER_INFO: Issuer CRYPTOAPI_BLOB at +8, SerialNumber at +24.
    // CERT_INFO for CERT_FIND_SUBJECT_CERT only needs SerialNumber (+8) and Issuer (+48).
    const certInfo = new Uint8Array(64);
    certInfo.set(signerInfo.subarray(24, 40), 8); // SerialNumber
    certInfo.set(signerInfo.subarray(8, 24), 48); // Issuer

    const certCtx = crypt32.symbols.CertFindCertificateInStore(
      hStore[0],
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
      0,
      CERT_FIND_SUBJECT_CERT,
      ptr(certInfo),
      null,
    );
    if (!certCtx) {
      return null;
    }
    try {
      const nameBuf = new Uint8Array(512);
      const n = crypt32.symbols.CertGetNameStringA(
        certCtx,
        CERT_NAME_SIMPLE_DISPLAY_TYPE,
        0,
        null,
        ptr(nameBuf),
        nameBuf.length,
      );
      if (n <= 1) {
        return null;
      }
      // n includes the trailing NUL
      return new TextDecoder().decode(nameBuf.subarray(0, n - 1));
    } finally {
      crypt32.symbols.CertFreeCertificateContext(certCtx);
    }
  } finally {
    crypt32.symbols.CryptMsgClose(hMsg[0]);
    crypt32.symbols.CertCloseStore(hStore[0], 0);
  }
}

// True if WinVerifyTrust accepts the embedded Authenticode signature.
// Mirrors IsPEFileSigned in src/base/Crypto_win.cpp.
export function isPeFileSigned(filePath: string): boolean {
  const pathW = wideZ(filePath);

  // WINTRUST_FILE_INFO (x64): cbStruct@0, pcwszFilePath@8, hFile@16, pgKnownSubject@24 → 32 bytes
  const fileInfo = new Uint8Array(32);
  const fi = new DataView(fileInfo.buffer);
  fi.setUint32(0, 32, true); // cbStruct
  fi.setBigUint64(8, BigInt(ptr(pathW)), true); // pcwszFilePath

  // WINTRUST_DATA (x64, with pSignatureSettings) → 88 bytes
  // cbStruct@0, pPolicy@8, pSIP@16, dwUIChoice@24, fdwRevocation@28,
  // dwUnionChoice@32, pFile@40, dwStateAction@48, hWVTStateData@56,
  // pwszURLReference@64, dwProvFlags@72, dwUIContext@76, pSignatureSettings@80
  const trustData = new Uint8Array(88);
  const td = new DataView(trustData.buffer);
  td.setUint32(0, 88, true); // cbStruct
  td.setUint32(24, WTD_UI_NONE, true); // dwUIChoice
  td.setUint32(28, WTD_REVOKE_NONE, true); // fdwRevocationChecks
  td.setUint32(32, WTD_CHOICE_FILE, true); // dwUnionChoice
  td.setBigUint64(40, BigInt(ptr(fileInfo)), true); // pFile
  td.setUint32(48, WTD_STATEACTION_IGNORE, true); // dwStateAction
  td.setUint32(72, WTD_SAFER_FLAG, true); // dwProvFlags

  const status = wintrust.symbols.WinVerifyTrust(null, ptr(WINTRUST_ACTION_GENERIC_VERIFY_V2), ptr(trustData));
  return status === ERROR_SUCCESS;
}

// Launch a process fully detached from this one (via CreateProcessW) and return
// its pid, WITHOUT waiting for it to exit. Unlike Bun.spawn, the child is not
// placed in Bun's job object, so it keeps running after this script exits.
// Use for launching a long-lived GUI app from a short-lived launcher script.
export function launchDetached(exePath: string, args: string[] = []): number {
  const quoted = `"${exePath}"` + (args.length ? " " + args.map((a) => `"${a}"`).join(" ") : "");
  const appW = wideZ(exePath);
  const cmdW = wideZ(quoted); // CreateProcessW may modify this buffer in place

  const si = new Uint8Array(104); // STARTUPINFOW (x64)
  new DataView(si.buffer).setUint32(0, 104, true); // cb = sizeof(STARTUPINFOW)
  const pi = new Uint8Array(24); // PROCESS_INFORMATION (x64)

  const ok = kernel32.symbols.CreateProcessW(
    ptr(appW),
    ptr(cmdW),
    null,
    null,
    false,
    DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
    null,
    null,
    ptr(si),
    ptr(pi),
  );
  if (!ok) {
    const err = kernel32.symbols.GetLastError();
    throw new Error(`CreateProcessW('${exePath}') failed, GetLastError=${err}`);
  }

  const dv = new DataView(pi.buffer);
  const hProcess = dv.getBigUint64(0, true);
  const hThread = dv.getBigUint64(8, true);
  const pid = dv.getUint32(16, true);
  kernel32.symbols.CloseHandle(hProcess); // we don't wait on the child
  kernel32.symbols.CloseHandle(hThread);
  return pid;
}

const TH32CS_SNAPPROCESS = 0x2;
const INVALID_HANDLE_VALUE = 0xffffffffffffffffn;
const PROCESS_TERMINATE = 0x0001;
const SYNCHRONIZE = 0x00100000;
const WAIT_OBJECT_0 = 0;
// x64 PROCESSENTRY32W: DWORD fields, then ULONG_PTR, then WCHAR szExeFile[MAX_PATH]
const PROCESSENTRY32W_SIZE = 568;
const PROCESSENTRY32W_PID = 8;
const PROCESSENTRY32W_EXE = 44;

function readWideZ(buf: Uint8Array, byteOff: number): string {
  const u16 = new Uint16Array(buf.buffer, buf.byteOffset + byteOff, (buf.byteLength - byteOff) >> 1);
  let s = "";
  for (let i = 0; i < u16.length && u16[i]; i++) {
    s += String.fromCharCode(u16[i]!);
  }
  return s;
}

// PIDs whose image name matches (case-insensitive), e.g. "SumatraPDF.exe"
export function listPidsByExeName(exeName: string): number[] {
  const snap = kernel32.symbols.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (!snap || snap === INVALID_HANDLE_VALUE) {
    return [];
  }
  try {
    const buf = new Uint8Array(PROCESSENTRY32W_SIZE);
    const dv = new DataView(buf.buffer);
    dv.setUint32(0, PROCESSENTRY32W_SIZE, true);
    if (!kernel32.symbols.Process32FirstW(snap, ptr(buf))) {
      return [];
    }
    const want = exeName.toLowerCase();
    const pids: number[] = [];
    for (;;) {
      if (readWideZ(buf, PROCESSENTRY32W_EXE).toLowerCase() === want) {
        pids.push(dv.getUint32(PROCESSENTRY32W_PID, true));
      }
      if (!kernel32.symbols.Process32NextW(snap, ptr(buf))) {
        break;
      }
    }
    return pids;
  } finally {
    kernel32.symbols.CloseHandle(snap);
  }
}

// Terminate a pid and block until it is gone. Already-exited is success.
export function killPid(pid: number, timeoutMs = 5000): boolean {
  if (!pid) {
    return true;
  }
  const h = kernel32.symbols.OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, false, pid);
  if (!h) {
    return true;
  }
  try {
    kernel32.symbols.TerminateProcess(h, 1);
    return kernel32.symbols.WaitForSingleObject(h, timeoutMs) === WAIT_OBJECT_0;
  } finally {
    kernel32.symbols.CloseHandle(h);
  }
}

export type KillableProc = {
  pid?: number | null;
  kill?: (signal?: number | string) => unknown;
  exitCode?: number | null;
};

// Kill a spawned process (Bun.Subprocess or node ChildProcess) and wait until
// the OS says it has exited. Replaces proc.kill()+sleep and taskkill /F.
export async function killAndWait(proc: KillableProc | undefined | null, timeoutMs = 5000): Promise<boolean> {
  if (!proc) {
    return true;
  }
  if (proc.exitCode !== null && proc.exitCode !== undefined) {
    return true;
  }
  const pid = proc.pid ?? 0;
  try {
    proc.kill?.();
  } catch {
    /* already gone */
  }
  return killPid(pid, timeoutMs);
}

// taskkill /F /IM <exe> replacement: terminate every matching process and wait
export async function killProcessesNamed(exeName: string, timeoutMs = 5000): Promise<number> {
  let n = 0;
  for (const pid of listPidsByExeName(exeName)) {
    if (pid === process.pid) {
      continue;
    }
    if (killPid(pid, timeoutMs)) {
      n++;
    }
  }
  return n;
}

// --- menus

// MN_GETHMENU: the HMENU behind a popup-menu window (class #32768, which is
// what waitForContextMenu returns). 0n if the window isn't a menu (any more).
export function getPopupMenuHandle(hwndPopup: number): bigint {
  return BigInt(sendMessage(hwndPopup, 0x01e1, 0, 0));
}

export function getMenuItemCount(hmenu: bigint): number {
  return user32.symbols.GetMenuItemCount(hmenu);
}

export function getSubMenu(hmenu: bigint, pos: number): bigint {
  return BigInt(user32.symbols.GetSubMenu(hmenu, pos));
}

// text of the item at `pos`, with the & accelerator markers stripped; the
// shortcut after the tab is dropped, so this is just the label
export function getMenuItemText(hmenu: bigint, pos: number): string {
  const buf = new Uint16Array(512);
  const n = user32.symbols.GetMenuStringW(hmenu, pos, ptr(buf), 512, MF_BYPOSITION);
  if (n <= 0) {
    return "";
  }
  const s = Buffer.from(buf.buffer, 0, n * 2).toString("utf16le");
  return s.split("\t")[0].replace(/&/g, "");
}

export const MF_BYPOSITION = 0x400;

export type MenuItem = { text: string; items?: MenuItem[] };

// the whole menu tree, submenus included. Separators (empty labels) are skipped
export function readMenuTree(hmenu: bigint): MenuItem[] {
  const out: MenuItem[] = [];
  const n = getMenuItemCount(hmenu);
  for (let i = 0; i < n; i++) {
    const text = getMenuItemText(hmenu, i);
    const sub = getSubMenu(hmenu, i);
    if (sub) {
      out.push({ text, items: readMenuTree(sub) });
    } else if (text) {
      out.push({ text });
    }
  }
  return out;
}
