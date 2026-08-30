// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6062
//
// Closing the last document with NoHomeTab leaves an empty window. The page
// box still showed the last page number and the canvas scrollbar stayed up.
//
// Run: bun tests/issue-6062.ts [--no-build]

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { findCanvas, killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";
import {
  enumChildWindows,
  enumWindows,
  findChildWindow,
  getClassName,
  getControlText,
  getWindowLong,
  getWindowPid,
  GWL_STYLE,
  isWindowVisible,
  sleep,
} from "./winapi.ts";

const WS_VSCROLL = 0x00200000;
const SRC_PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

function toolbarEdits(frame: number): { hwnd: number; text: string }[] {
  const tb = findChildWindow(frame, "SUMATRA_VIRT_TOOLBAR");
  if (!tb) {
    return [];
  }
  const out: { hwnd: number; text: string }[] = [];
  enumChildWindows(tb, (hwnd) => {
    if (getClassName(hwnd) === "Edit") {
      out.push({ hwnd, text: getControlText(hwnd).trim() });
    }
    return true;
  });
  return out;
}

function visibleOverlayScrollbars(pid: number): number {
  let n = 0;
  enumWindows((hwnd) => {
    if (getWindowPid(hwnd) === pid && getClassName(hwnd) === "SUMATRA_OVERLAY_SCROLLBAR" && isWindowVisible(hwnd)) {
      n++;
    }
    return true;
  });
  return n;
}

async function runCloseLastFile(scrollbars: string): Promise<void> {
  const appData = tmpPath(`issue-6062-appdata-${scrollbars}`);
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  const pdf = join(appData, "doc.pdf");
  copyFileSync(SRC_PDF, pdf);
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = false",
      "RememberOpenedFiles = false",
      "UseTabs = true",
      "NoHomeTab = true",
      "ShowStartPage = false",
      "ShowToc = false",
      `Scrollbars = ${scrollbars}`,
      "DefaultDisplayMode = continuous",
      "DefaultZoom = 200%",
      "",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appData, "-zoom", "200", pdf], {
    saveSettings: true,
  });
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdGoToLastPage"));
    await client.waitForRenderIdle();
    await sleep(150);

    const before = toolbarEdits(frame);
    const pageBefore = before.find((e) => /^\d+$/.test(e.text));
    if (!pageBefore) {
      throw new Error(
        `issue-6062 (${scrollbars}): no page number in the toolbar before close (${JSON.stringify(before)})`,
      );
    }

    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error(`issue-6062 (${scrollbars}): no canvas`);
    }
    if (scrollbars === "windows") {
      const hadVScroll = (getWindowLong(canvas, GWL_STYLE) & WS_VSCROLL) !== 0;
      if (!hadVScroll) {
        throw new Error(`issue-6062 (${scrollbars}): expected a vertical scrollbar before close`);
      }
    }

    sendCommandSync(frame, cmdId("CmdClose"));
    await sleep(250);

    const after = toolbarEdits(frame);
    const leftoverPage = after.find((e) => /^\d+$/.test(e.text));
    if (leftoverPage) {
      throw new Error(
        `issue-6062 (${scrollbars}): page box still shows '${leftoverPage.text}' after closing the last file`,
      );
    }

    const style = getWindowLong(canvas, GWL_STYLE);
    if ((style & WS_VSCROLL) !== 0) {
      throw new Error(
        `issue-6062 (${scrollbars}): canvas still has WS_VSCROLL after closing the last file (style=0x${(style >>> 0).toString(16)})`,
      );
    }
    const nOverlay = visibleOverlayScrollbars(proc.pid!);
    if (nOverlay > 0) {
      throw new Error(
        `issue-6062 (${scrollbars}): overlay scrollbar still visible after closing the last file (n=${nOverlay})`,
      );
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  await runCloseLastFile("windows");
  await runCloseLastFile("smart");
}

if (import.meta.main) {
  await runStandalone(testit);
}
