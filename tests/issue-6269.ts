// #6269: Home page About popup. The Copy button's mouse-down called SetFocus
// on the WS_EX_NOACTIVATE popup, which deactivated the frame and closed the
// popup before mouse-up, so the click copied nothing.
//
// Run: bun tests/issue-6269.ts [--no-build]

import { dlopen, FFIType } from "bun:ffi";
import { IS_WIN } from "./host.ts";
import { runStandalone, USE_NG } from "./util.ts";
import {
  captureWindowPixels,
  clientToScreen,
  ensureModifierKeysUp,
  enumWindows,
  getClassName,
  getForegroundWindow,
  getWindowPid,
  getWindowRect,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  setCursorPos,
  setForegroundWindow,
  sleep,
  windowFromPoint,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

// Loaded only on Windows: this file is imported by the suite on every host.
const win32 = IS_WIN
  ? {
      user32: dlopen("user32.dll", {
        GetWindowThreadProcessId: { args: [FFIType.ptr, FFIType.ptr], returns: FFIType.u32 },
        AttachThreadInput: { args: [FFIType.u32, FFIType.u32, FFIType.bool], returns: FFIType.bool },
        BringWindowToTop: { args: [FFIType.ptr], returns: FFIType.bool },
      }).symbols,
      kernel32: dlopen("kernel32.dll", {
        GetCurrentThreadId: { args: [], returns: FFIType.u32 },
      }).symbols,
    }
  : null;

// SetForegroundWindow alone is ignored when another process is in front.
// The Copy bug only shows while the frame is the foreground window.
function forceForeground(hwnd: number): void {
  if (!win32) {
    throw new Error("issue-6269: Win32 only");
  }
  if (getForegroundWindow() === hwnd) {
    return;
  }
  const fg = getForegroundWindow();
  const fgThread = win32.user32.GetWindowThreadProcessId(fg, null);
  const me = win32.kernel32.GetCurrentThreadId();
  win32.user32.AttachThreadInput(me, fgThread, true);
  setForegroundWindow(hwnd);
  win32.user32.BringWindowToTop(hwnd);
  win32.user32.AttachThreadInput(me, fgThread, false);
  if (getForegroundWindow() !== hwnd) {
    throw new Error("issue-6269: could not bring SumatraPDF to the foreground");
  }
}

function clipboardText(): string {
  const res = Bun.spawnSync(["powershell.exe", "-NoProfile", "-Command", "Get-Clipboard -Raw"], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (res.exitCode !== 0) {
    throw new Error(`issue-6269: Get-Clipboard failed: ${res.stderr.toString()}`);
  }
  return res.stdout.toString().replace(/\r?\n$/, "");
}

function setClipboard(value: string): void {
  const res = Bun.spawnSync(["powershell.exe", "-NoProfile", "-Command", `Set-Clipboard -Value '${value}'`], {
    stdout: "pipe",
    stderr: "pipe",
  });
  if (res.exitCode !== 0) {
    throw new Error(`issue-6269: Set-Clipboard failed: ${res.stderr.toString()}`);
  }
}

// the five brand stripes at the left of the header. Other saturated pixels
// in that band (a focus ring, an icon) must not pull the point off the title.
function findLogo(canvas: number): { x: number; y: number } | null {
  const pix = captureWindowPixels(canvas, "client");
  if (!pix) {
    return null;
  }
  let minX = pix.w;
  const yMax = Math.min(pix.h, 80);
  const hits: { x: number; y: number }[] = [];
  for (let y = 0; y < yMax; y++) {
    for (let x = 0; x < pix.w; x++) {
      const i = (y * pix.w + x) * 4;
      const b = pix.data[i]!;
      const g = pix.data[i + 1]!;
      const r = pix.data[i + 2]!;
      const spread = Math.max(r, g, b) - Math.min(r, g, b);
      if (spread < 40 || r + g + b > 680) {
        continue;
      }
      hits.push({ x, y });
      if (x < minX) {
        minX = x;
      }
    }
  }
  let n = 0;
  let maxX = 0;
  let minY = pix.h;
  let maxY = 0;
  for (const p of hits) {
    if (p.x > minX + 40) {
      continue;
    }
    n++;
    if (p.x > maxX) {
      maxX = p.x;
    }
    if (p.y < minY) {
      minY = p.y;
    }
    if (p.y > maxY) {
      maxY = p.y;
    }
  }
  if (n < 20) {
    return null;
  }
  return { x: Math.round((minX + maxX) / 2), y: Math.round((minY + maxY) / 2) };
}

function findHover(pid: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === "SUMATRA_ABOUT_HOVER" && isWindowVisible(hwnd)) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function postMove(hwnd: number, x: number, y: number): void {
  const screen = clientToScreen(hwnd, x, y);
  setCursorPos(screen.x, screen.y);
  sendMessage(hwnd, WM_MOUSEMOVE, 0, packCoords(x, y));
}

// one enter, then keep the real cursor on the title until the hover timer fires.
// a move on every poll restarts that timer, so the popup never opens.
async function showByHover(canvas: number, logo: { x: number; y: number }, pid: number): Promise<number> {
  const screen = clientToScreen(canvas, logo.x, logo.y);
  postMove(canvas, logo.x, logo.y);
  const deadline = Date.now() + 2000;
  while (Date.now() < deadline) {
    setCursorPos(screen.x, screen.y);
    const hover = findHover(pid);
    if (hover) {
      return hover;
    }
    await sleep(40);
  }
  throw new Error("issue-6269: About popup did not appear on hover");
}

function screenAt(hwnd: number, x: number, y: number): { x: number; y: number } {
  const origin = clientToScreen(hwnd, 0, 0);
  return { x: origin.x + x, y: origin.y + y };
}

// step the pointer from the title onto Copy. A jump plus an immediate move
// closes the popup; a real move does not.
async function walkTo(hover: number, from: { x: number; y: number }, to: { x: number; y: number }): Promise<void> {
  const steps = 8;
  for (let i = 1; i <= steps; i++) {
    const x = Math.round(from.x + ((to.x - from.x) * i) / steps);
    const y = Math.round(from.y + ((to.y - from.y) * i) / steps);
    setCursorPos(x, y);
    await sleep(15);
    const hwnd = windowFromPoint(x, y);
    if (!hwnd) {
      throw new Error("issue-6269: no window under the pointer on the way to Copy");
    }
    const origin = clientToScreen(hwnd, 0, 0);
    sendMessage(hwnd, WM_MOUSEMOVE, 0, packCoords(x - origin.x, y - origin.y));
    if (!isWindowVisible(hover)) {
      throw new Error("issue-6269: About popup closed while moving onto Copy");
    }
  }
}

function darkSpan(
  pix: { w: number; h: number; data: Uint8Array },
  y: number,
): { n: number; minX: number; maxX: number } {
  let n = 0;
  let minX = pix.w;
  let maxX = 0;
  for (let x = 0; x < pix.w; x++) {
    const i = (y * pix.w + x) * 4;
    const sum = pix.data[i]! + pix.data[i + 1]! + pix.data[i + 2]!;
    if (sum > 400) {
      continue;
    }
    n++;
    if (x < minX) {
      minX = x;
    }
    if (x > maxX) {
      maxX = x;
    }
  }
  return { n, minX, maxX };
}

function isBlankCapture(pix: { w: number; h: number; data: Uint8Array }): boolean {
  const y = Math.floor(pix.h / 2);
  for (let x = 0; x < pix.w; x += 16) {
    const i = (y * pix.w + x) * 4;
    if (pix.data[i]! + pix.data[i + 1]! + pix.data[i + 2]! > 0) {
      return false;
    }
  }
  return true;
}

// "Copy program and machine info" is the first text row in the footer, under
// the rule. A fixed offset from the bottom lands on the software link.
// PW_CLIENTONLY PrintWindow sometimes returns an all-black bitmap for this
// no-activate popup; the full-window capture still has the glyphs.
async function findCopyPoint(hover: number): Promise<{ x: number; y: number }> {
  const win = getWindowRect(hover);
  const origin = clientToScreen(hover, 0, 0);
  const offX = origin.x - win.left;
  const offY = origin.y - win.top;
  for (let attempt = 0; attempt < 8; attempt++) {
    const pix = captureWindowPixels(hover, "window");
    if (!pix || isBlankCapture(pix)) {
      await sleep(40);
      continue;
    }
    let bandY0 = -1;
    let bandY1 = -1;
    let minX = pix.w;
    let maxX = 0;
    for (let y = Math.floor(pix.h * 0.65); y < pix.h; y++) {
      const span = darkSpan(pix, y);
      const text = span.n >= 30 && span.maxX - span.minX >= 80 && span.minX > 16;
      if (!text) {
        if (bandY0 >= 0) {
          break;
        }
        continue;
      }
      if (bandY0 < 0) {
        bandY0 = y;
      }
      bandY1 = y;
      if (span.minX < minX) {
        minX = span.minX;
      }
      if (span.maxX > maxX) {
        maxX = span.maxX;
      }
    }
    if (bandY0 >= 0) {
      return {
        x: Math.round((minX + maxX) / 2) - offX,
        y: Math.round((bandY0 + bandY1) / 2) - offY,
      };
    }
    break;
  }
  throw new Error("issue-6269: could not find the Copy link");
}

// mouse-down, a gap, then mouse-up. The popup used to close inside the down,
// so the up never reached the button.
async function clickCopy(hover: number, from: { x: number; y: number }): Promise<void> {
  const { x, y } = await findCopyPoint(hover);
  await walkTo(hover, from, screenAt(hover, x, y));
  const lp = packCoords(x, y);
  sendMessage(hover, WM_LBUTTONDOWN, MK_LBUTTON, lp);
  if (!isWindowVisible(hover)) {
    throw new Error("issue-6269: About popup closed on Copy mouse-down");
  }
  await sleep(120);
  if (!isWindowVisible(hover)) {
    throw new Error("issue-6269: About popup closed between Copy mouse-down and mouse-up");
  }
  sendMessage(hover, WM_LBUTTONUP, 0, lp);
  await sleep(100);
  const text = clipboardText();
  if (!text.startsWith("SumatraPDF")) {
    throw new Error(`issue-6269: Copy did not put the report on the clipboard: ${JSON.stringify(text.slice(0, 80))}`);
  }
}

export async function testit(): Promise<void> {
  // ng draws About as a gpui hover card. There is no WS_EX_NOACTIVATE popup
  // whose SetFocus closes it on mouse-down.
  if (!IS_WIN || USE_NG) {
    console.log("SKIP issue-6269: About hover is a Win32 no-activate popup");
    return;
  }
  setClipboard("issue-6269-sentinel");
  const { proc, frame } = await launchControlled([]);
  try {
    const canvas = findCanvas(frame);
    let logo: { x: number; y: number } | null = null;
    const logoDeadline = Date.now() + 3000;
    while (Date.now() < logoDeadline && !logo) {
      logo = findLogo(canvas);
      if (!logo) {
        await sleep(50);
      }
    }
    if (!logo) {
      throw new Error("issue-6269: could not find the SumatraPDF title on the home page");
    }
    const logoScreen = clientToScreen(canvas, logo.x, logo.y);

    forceForeground(frame);
    const hover = await showByHover(canvas, logo, proc.pid!);
    await clickCopy(hover, logoScreen);

    // clicking the title puts the frame in front. Copy used to fail on every
    // later try, not only on the first hover.
    setClipboard("issue-6269-sentinel");
    postMove(canvas, logo.x, logo.y);
    const lp = packCoords(logo.x, logo.y);
    sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, lp);
    sendMessage(canvas, WM_LBUTTONUP, 0, lp);
    forceForeground(frame);
    const again = isWindowVisible(hover) ? hover : await showByHover(canvas, logo, proc.pid!);
    await clickCopy(again, logoScreen);
  } finally {
    await ensureModifierKeysUp();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
