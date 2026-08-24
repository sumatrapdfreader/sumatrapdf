// Deleting a selected annotation must not select a replacement. Deleting from
// the annotation editor must also drop it from the page, not only the list.
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

type AnnotState = {
  selected: number;
  count: number;
  selectedCount: number;
};

async function annotState(client: ControlClient, selectItem = 0): Promise<AnnotState> {
  const res = await client.request(ControlCommand.TestAnnotEditorLayout, [0, selectItem]);
  const exitCode = res[0] as number;
  const raw = String(res[1] ?? "");
  if (exitCode !== 0) {
    throw new Error(`annot-delete-redraw: TestAnnotEditorLayout failed: ${raw}`);
  }
  const m = / sel=(-?\d+) n=(\d+) selCount=(\d+)/.exec(raw);
  if (!m) {
    throw new Error(`annot-delete-redraw: could not parse: ${raw}`);
  }
  return { selected: +m[1]!, count: +m[2]!, selectedCount: +m[3]! };
}

async function waitForAnnotCount(client: ControlClient, want: number): Promise<AnnotState> {
  const deadline = Date.now() + 5_000;
  for (;;) {
    const state = await annotState(client);
    if (state.count === want) {
      return state;
    }
    if (Date.now() > deadline) {
      throw new Error(`annot-delete-redraw: list still has ${state.count} annotations (want ${want})`);
    }
    await sleep(50);
  }
}

function assertNoSelection(state: AnnotState, action: string): void {
  if (state.selected !== -1 || state.selectedCount !== 0) {
    throw new Error(
      `annot-delete-redraw: ${action} selected another annotation (sel=${state.selected} count=${state.selectedCount})`,
    );
  }
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

    // Keep annotations after both deletions to verify none is selected automatically.
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(80, 80));
    await sleep(200);
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(80, Math.min(220, cr.bottom - 80)));
    await sleep(200);
    sendMessage(frame, WM_COMMAND, cmdId("CmdCreateAnnotStamp"), packCoords(Math.min(320, cr.right - 80), 80));
    await sleep(400);
    await client.waitForRenderIdle();

    sendMessage(frame, WM_COMMAND, cmdId("CmdEditAnnotations"), 0);
    const annotWin = await waitAnnotWindow(proc.pid!, frame);
    const nBefore = (await annotState(client)).count;
    if (nBefore < 3) {
      throw new Error(`annot-delete-redraw: expected 3 stamps in the list, got ${nBefore}`);
    }

    // Exercise CmdDeleteAnnotation / DeleteAnnotationAndUpdateUI.
    sendMessage(frame, WM_COMMAND, cmdId("CmdDeleteAnnotation"), 0);
    const afterCommandDelete = await waitForAnnotCount(client, nBefore - 1);
    assertNoSelection(afterCommandDelete, "command deletion");

    // Explicitly select a remaining row, then exercise the editor's multi-row delete path.
    const beforeEditorDelete = await annotState(client, 2);
    if (beforeEditorDelete.selected < 0 || beforeEditorDelete.selectedCount !== 1) {
      throw new Error(`annot-delete-redraw: failed to select an annotation (${JSON.stringify(beforeEditorDelete)})`);
    }
    await client.waitForRenderIdle();
    const beforePng = join(dir, "before-delete.png");
    if (!captureWindowToPng(canvas, beforePng)) {
      throw new Error("annot-delete-redraw: capture before delete failed");
    }
    const redBefore = countRedPixels(beforePng);
    if (redBefore < 20) {
      throw new Error(`annot-delete-redraw: stamps not visible before delete (red=${redBefore})`);
    }

    postMessage(annotWin, WM_KEYDOWN, 0x2e /* VK_DELETE */, 0);
    const afterDelete = await waitForAnnotCount(client, nBefore - 2);
    assertNoSelection(afterDelete, "editor deletion");
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
