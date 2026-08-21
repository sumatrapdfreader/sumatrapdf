// Deleting an annotation from the Edit Annotations list must drop it from the
// page, not only from the list. Selection change only ScheduleRepaint, which
// blits the cached page bitmap (still showing the deleted annot).
import { mkdirSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import {
  captureWindowToPng,
  enumWindows,
  getClientRect,
  getWindowPid,
  getWindowText,
  packCoords,
  postMessage,
  sendMessage,
  sleep,
  VK_ESCAPE,
  WM_COMMAND,
  WM_KEYDOWN,
} from "./winapi";
import { findCanvas, killAndWait, launchControlled } from "./win-automation";

function findAnnotWindow(pid: number, frame: number): number {
  let found = 0;
  enumWindows((hwnd) => {
    if (hwnd === frame || getWindowPid(hwnd) !== pid) {
      return true;
    }
    if (getWindowText(hwnd).startsWith("Annotations")) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

async function waitAnnotWindow(pid: number, frame: number): Promise<number> {
  const deadline = Date.now() + 10_000;
  for (;;) {
    const hwnd = findAnnotWindow(pid, frame);
    if (hwnd) {
      return hwnd;
    }
    if (Date.now() > deadline) {
      throw new Error("annot-delete-redraw: Annotations window did not open");
    }
    await sleep(50);
  }
}

async function annotCount(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, 0]);
  const exitCode = res[0] as number;
  const raw = String(res[1] ?? "");
  if (exitCode !== 0) {
    throw new Error(`annot-delete-redraw: TestAnnotEditorLayout failed: ${raw}`);
  }
  const m = / n=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`annot-delete-redraw: could not parse: ${raw}`);
  }
  return +m[1]!;
}

function countRedPixels(png: string): number {
  const p = png.split("\\").join("\\\\");
  const ps = `Add-Type -AssemblyName System.Drawing; $b=[System.Drawing.Bitmap]::FromFile('${p}'); $n=0; for($y=0;$y -lt $b.Height;$y+=2){for($x=0;$x -lt $b.Width;$x+=2){$c=$b.GetPixel($x,$y); if($c.R -gt 180 -and $c.G -lt 80 -and $c.B -lt 80){$n++}}}; $b.Dispose(); Write-Output $n`;
  const r = Bun.spawnSync(["powershell", "-NoProfile", "-Command", ps]);
  return parseInt(r.stdout.toString().trim(), 10) || 0;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("annot-delete-redraw");
  mkdirSync(dir, { recursive: true });
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-view", "single page", "-zoom", "fit page", pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    const cr = getClientRect(canvas);

    // two stamps so delete selects the remaining one (the path that used to
    // skip MainWindowRerender). One stamp would hit the "nothing selected"
    // branch, which already re-rendered.
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(80, 80));
    await sleep(200);
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(80, Math.min(220, cr.bottom - 80)));
    await sleep(400);
    await client.waitForRenderIdle();

    sendMessage(frame, WM_COMMAND, cmdId("CmdEditAnnotations"), 0);
    const annotWin = await waitAnnotWindow(proc.pid!, frame);
    const nBefore = await annotCount(client);
    if (nBefore < 2) {
      throw new Error(`annot-delete-redraw: expected 2 stamps in the list, got ${nBefore}`);
    }
    const beforePng = join(dir, "before-delete.png");
    if (!captureWindowToPng(canvas, beforePng)) {
      throw new Error("annot-delete-redraw: capture before delete failed");
    }
    const redBefore = countRedPixels(beforePng);
    if (redBefore < 20) {
      throw new Error(`annot-delete-redraw: stamps not visible before delete (red=${redBefore})`);
    }

    postMessage(annotWin, WM_KEYDOWN, 0x2e /* VK_DELETE */, 0);
    const deadline = Date.now() + 5_000;
    for (;;) {
      const n = await annotCount(client);
      if (n === nBefore - 1) {
        break;
      }
      if (Date.now() > deadline) {
        throw new Error(`annot-delete-redraw: list still has ${n} after Delete (want ${nBefore - 1})`);
      }
      await sleep(50);
    }
    await client.waitForRenderIdle();
    const afterPng = join(dir, "after-delete.png");
    if (!captureWindowToPng(canvas, afterPng)) {
      throw new Error("annot-delete-redraw: capture after delete failed");
    }
    const redAfter = countRedPixels(afterPng);
    // one of two DRAFT stamps gone; allow some leftover from the remaining one
    if (redAfter >= redBefore * 0.8) {
      throw new Error(
        `annot-delete-redraw: page still shows deleted stamp (red before=${redBefore} after=${redAfter})`,
      );
    }
    if (redAfter < 5) {
      throw new Error(`annot-delete-redraw: remaining stamp vanished too (red=${redAfter})`);
    }

    postMessage(annotWin, WM_KEYDOWN, VK_ESCAPE, 0);
  } finally {
    client.close();
    await killAndWait(proc);
  }
  console.log("annot-delete-redraw: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
