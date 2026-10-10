// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6028
//
// Session restore loads tabs while the frame is hidden. ShowScrollBar then
// sets WS_VSCROLL without WM_NCCALCSIZE, so the style bit is on but no bar
// is laid out. After show, ShowWinScrollBar used to no-op because the bit
// already matched — the bar stayed missing until a resize. Switching to
// another restored tab had the same missing bar.
//
// Run: bun tests/issue-6028.ts [--no-build]   (or via tests/run-almost-all.ts)

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { IS_MAC } from "./host.ts";
import { ROOT, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import {
  findCanvas,
  killAndWait,
  launchControlled,
  sendCommand,
  vScrollbarColorCount,
  waitForExit,
} from "./win-automation.ts";
import {
  captureWindowPixels,
  getClientRect,
  getSystemMetrics,
  getWindowLong,
  getWindowRect,
  getWindowText,
  GWL_STYLE,
  postMessage,
  setProcessDpiAware,
  sleep,
  SM_CXVSCROLL,
  WM_CLOSE,
} from "./winapi.ts";

const WS_VSCROLL = 0x00200000;
const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

function vScrollLaidOut(canvas: number): boolean {
  const wr = getWindowRect(canvas);
  const cr = getClientRect(canvas);
  const styleOn = (getWindowLong(canvas, GWL_STYLE) & WS_VSCROLL) !== 0;
  return styleOn && wr.right - wr.left - cr.right >= 8;
}

// ng draws the windows-mode bar inside the canvas, not as WS_VSCROLL
async function ngVScrollbarColorCount(
  client: { request: (cmd: number, args?: unknown[]) => Promise<unknown[]> },
  frame: number,
): Promise<number> {
  const raw = String((await client.request(ControlCommand.TestLayout, ["get"]))[1] ?? "");
  const m = /item name=canvas visible=1 rect=(-?\d+),(-?\d+),(-?\d+),(-?\d+)/.exec(raw);
  if (!m) {
    return 0;
  }
  const scale = Number(/canvasScale=([0-9.]+)/.exec(raw)?.[1] ?? 1) || 1;
  const cap = captureWindowPixels(frame);
  if (!cap) {
    return 0;
  }
  const px = (d: number) => Math.round(d / scale);
  const right = px(+m[1]! + +m[3]!);
  const y0 = Math.max(0, px(+m[2]!));
  const y1 = Math.min(cap.h, px(+m[2]! + +m[4]!));
  const x = Math.max(0, Math.min(cap.w - 1, right - Math.floor(getSystemMetrics(SM_CXVSCROLL) / 2)));
  const colors = new Set<number>();
  for (let y = y0; y < y1; y++) {
    const i = (y * cap.w + x) * 4;
    colors.add((cap.data[i + 2]! << 16) | (cap.data[i + 1]! << 8) | cap.data[i]!);
  }
  return colors.size;
}

async function assertNgVScroll(
  client: { request: (cmd: number, args?: unknown[]) => Promise<unknown[]> },
  frame: number,
  label: string,
): Promise<void> {
  const ui = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
  const vis = /scrollVis=(\d)\//.exec(ui);
  const sb = /sbV=(-?\d+)\/(-?\d+)\//.exec(ui);
  const width = sb ? +sb[2]! : 0;
  if (vis?.[1] !== "1" || width < 8) {
    throw new Error(
      `issue-6028: ${label}: vertical scrollbar not laid out (scrollVis=${vis?.[1] ?? "?"} sbW=${width})`,
    );
  }
  const n = await ngVScrollbarColorCount(client, frame);
  if (n < 2) {
    throw new Error(`issue-6028: ${label}: vertical scrollbar not painted (${n} color(s) in the strip)`);
  }
}

function assertVScroll(canvas: number, label: string): void {
  if (!vScrollLaidOut(canvas)) {
    const wr = getWindowRect(canvas);
    const cr = getClientRect(canvas);
    const style = getWindowLong(canvas, GWL_STYLE) >>> 0;
    throw new Error(
      `issue-6028: ${label}: vertical scrollbar not laid out ` +
        `(win=${wr.right - wr.left}x${wr.bottom - wr.top} client=${cr.right}x${cr.bottom} style=0x${style.toString(16)})`,
    );
  }
  const n = vScrollbarColorCount(canvas);
  if (n < 2) {
    throw new Error(`issue-6028: ${label}: vertical scrollbar not painted (${n} color(s) in the strip)`);
  }
}

export async function testit(): Promise<void> {
  // A missing bar still has a scroll range. The check counts colors in the painted strip.
  if (IS_MAC) {
    console.log("SKIP issue-6028: the check reads scrollbar pixels with GetWindowDC");
    return;
  }
  setProcessDpiAware();
  const appData = tmpPath("issue-6028-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdfA = join(appData, "a.pdf");
  const pdfB = join(appData, "b.pdf");
  copyFileSync(SRC_PDF, pdfA);
  copyFileSync(SRC_PDF, pdfB);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = true",
      "ReuseInstance = false",
      "RememberOpenedFiles = true",
      "UseTabs = true",
      "NoHomeTab = true",
      "ShowStartPage = false",
      "ShowToc = false",
      "Scrollbars = windows",
      "DefaultDisplayMode = continuous",
      "DefaultZoom = 200%",
      "WindowState = 1",
      "WindowPos = 80 80 900 700",
      "",
    ].join("\n"),
  );

  const first = await launchControlled(["-appdata", appData, "-zoom", "200", pdfA, pdfB], {
    saveSettings: true,
    defaultWindowPos: true,
  });
  try {
    await first.client.waitForRenderIdle(15000);
    sendCommand(first.frame, cmdId("CmdZoom200"));
    await first.client.waitForRenderIdle(15000);
    const canvas1 = findCanvas(first.frame);
    if (!canvas1) {
      throw new Error("issue-6028: no canvas on first launch");
    }
    if (USE_NG) {
      await assertNgVScroll(first.client, first.frame, "first launch");
    } else {
      assertVScroll(canvas1, "first launch");
    }
    postMessage(first.frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(first.proc, 8000))) {
      throw new Error("issue-6028: first instance did not exit");
    }
  } finally {
    first.client.close();
    await killAndWait(first.proc);
  }

  const second = await launchControlled(["-appdata", appData], { saveSettings: true, defaultWindowPos: true });
  try {
    await second.client.waitForRenderIdle(15000);
    await sleep(200);
    const canvas = findCanvas(second.frame);
    if (!canvas) {
      throw new Error("issue-6028: no canvas after restore");
    }
    if (USE_NG) {
      await assertNgVScroll(second.client, second.frame, `restore active tab (${getWindowText(second.frame)})`);
    } else {
      assertVScroll(canvas, `restore active tab (${getWindowText(second.frame)})`);
    }

    const title1 = getWindowText(second.frame);
    sendCommand(second.frame, cmdId("CmdNextTab"));
    await second.client.waitForRenderIdle(15000);
    await sleep(200);
    const title2 = getWindowText(second.frame);
    if (title2 === title1) {
      throw new Error(`issue-6028: NextTab did not switch tabs (still ${title2})`);
    }
    if (USE_NG) {
      await assertNgVScroll(second.client, second.frame, `after NextTab (${title2})`);
    } else {
      assertVScroll(canvas, `after NextTab (${title2})`);
    }
    console.log(`  restore "${title1}" and NextTab "${title2}" both have a vertical scrollbar ✓`);

    postMessage(second.frame, WM_CLOSE, 0, 0);
    await waitForExit(second.proc, 8000);
  } finally {
    second.client.close();
    await killAndWait(second.proc);
    rmSync(appData, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
