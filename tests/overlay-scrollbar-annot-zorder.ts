// Smart overlay scrollbar must not paint over the Edit Annotations window
// when the cursor moves in the main window (HWND_TOP used to raise it).
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowRect,
  getWindowText,
  isWindowAbove,
  setCursorPos,
  setForegroundWindow,
  setProcessDpiAware,
  setWindowPos,
  sleep,
  windowFromPoint,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const OVERLAY_CLASS = "SUMATRA_OVERLAY_SCROLLBAR";
const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

function findAnnotWindow(pid: number, frame: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (hwnd !== frame && getWindowPid(hwnd) === pid && getWindowText(hwnd).startsWith("Annotations")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function findOverlayScrollbars(pid: number): number[] {
  const found: number[] = [];
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === OVERLAY_CLASS) {
      found.push(hwnd);
    }
    return true;
  });
  return found;
}

export async function testit(): Promise<void> {
  setProcessDpiAware();
  const dir = tmpPath("overlay-scrollbar-annot-zorder");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nScrollbars = smart\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, PDF]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    sendCommandSync(frame, cmdId("CmdEditAnnotations"));
    const layout = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
    if ((layout[0] as number) !== 0) {
      throw new Error(`overlay-scrollbar-annot-zorder: editor did not open: ${String(layout[1] ?? "")}`);
    }
    const pid = proc.pid!;
    const deadline = Date.now() + 10_000;
    let annot = 0;
    while (Date.now() < deadline) {
      annot = findAnnotWindow(pid, frame);
      if (annot) {
        break;
      }
      await sleep(40);
    }
    if (!annot) {
      throw new Error("overlay-scrollbar-annot-zorder: Annotations window did not open");
    }

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("overlay-scrollbar-annot-zorder: canvas not found");
    }
    const cr = getWindowRect(canvas);
    // Cover the canvas's right edge, where the overlay scrollbar lives.
    setWindowPos(annot, cr.right - 180, cr.top + 40, 360, 400);

    setForegroundWindow(frame);
    const mid = clientToScreen(canvas, Math.floor((cr.right - cr.left) / 2), Math.floor((cr.bottom - cr.top) / 2));
    setCursorPos(mid.x, mid.y);
    sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    await sleep(150);

    const bars = findOverlayScrollbars(pid);
    if (bars.length === 0) {
      throw new Error("overlay-scrollbar-annot-zorder: overlay scrollbar HWND not found");
    }
    for (const sb of bars) {
      if (!isWindowAbove(annot, sb)) {
        throw new Error("overlay-scrollbar-annot-zorder: overlay scrollbar is above the Annotations window");
      }
    }

    const probeX = cr.right - 8;
    const probeY = cr.top + 80;
    const hit = windowFromPoint(probeX, probeY);
    if (hit && getClassName(hit) === OVERLAY_CLASS) {
      throw new Error("overlay-scrollbar-annot-zorder: overlay scrollbar is hit-testing above Annotations");
    }
    console.log("overlay-scrollbar-annot-zorder: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
