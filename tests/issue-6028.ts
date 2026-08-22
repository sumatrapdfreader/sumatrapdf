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
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  findCanvas,
  killAndWait,
  launchControlled,
  sendCommand,
  vScrollbarColorCount,
  waitForExit,
} from "./win-automation.ts";
import {
  getClientRect,
  getWindowLong,
  getWindowRect,
  getWindowText,
  GWL_STYLE,
  postMessage,
  setProcessDpiAware,
  sleep,
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
    assertVScroll(canvas1, "first launch");
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
    assertVScroll(canvas, `restore active tab (${getWindowText(second.frame)})`);

    const title1 = getWindowText(second.frame);
    sendCommand(second.frame, cmdId("CmdNextTab"));
    await second.client.waitForRenderIdle(15000);
    await sleep(200);
    const title2 = getWindowText(second.frame);
    if (title2 === title1) {
      throw new Error(`issue-6028: NextTab did not switch tabs (still ${title2})`);
    }
    assertVScroll(canvas, `after NextTab (${title2})`);
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
