// Smart overlay scrollbar must not paint over the Edit Annotations window
// when the cursor moves in the main window (HWND_TOP used to raise it).
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { cmdId, ROOT, runStandalone, waitForAnnotWindow, writeAppdata } from "./util.ts";
import {
  clientToScreen,
  enumWindows,
  getClassName,
  getWindowPid,
  getWindowRect,
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
  const dir = writeAppdata(
    "overlay-scrollbar-annot-zorder",
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
    const annot = await waitForAnnotWindow(pid, frame);

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
