// The main-window layout probe exposes geometry through -dbg-control and
// counts completed relayouts through gAfterLayout. A small direct-DC sample
// also verifies that a fullscreen transition painted its final geometry.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, killAndWait, sendCommandSync, waitForTitle } from "./win-automation.ts";
import { captureWindowDCRegionPixels, sendCopyDataW, setProcessDpiAware, sleep } from "./winapi.ts";
import type { LayoutInfo, LayoutItem } from "./control.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
const EPUB = join(import.meta.dir, "issue-5846.epub");
const kCopyDataDdeW = 0x44646557;

function requireItem(layout: LayoutInfo, name: string): LayoutItem {
  const item = layout.items[name];
  if (!item) {
    throw new Error(`layout-callback: missing '${name}' in:\n${layout.raw}`);
  }
  return item;
}

// Fullscreen leaves the left edge as bare document background. Sampling that
// strip catches normal-window caption/toolbar pixels copied into it without
// depending on page rendering or writing a screenshot.
function sampleFullscreenTop(frame: number): Uint8Array {
  const pixels = captureWindowDCRegionPixels(frame, 0, 0, 100, 70);
  if (!pixels) {
    throw new Error("layout-callback: failed to read fullscreen window pixels");
  }
  return pixels;
}

function countChangedPixels(a: Uint8Array, b: Uint8Array): number {
  if (a.length !== b.length) {
    return Math.max(a.length, b.length) / 4;
  }
  let changed = 0;
  for (let i = 0; i < a.length; i += 4) {
    if (a[i] !== b[i] || a[i + 1] !== b[i + 1] || a[i + 2] !== b[i + 2]) {
      changed++;
    }
  }
  return changed;
}

async function sampleStableFullscreenTop(frame: number): Promise<Uint8Array> {
  const deadline = Date.now() + 3000;
  let previous = sampleFullscreenTop(frame);
  for (;;) {
    await sleep(120);
    const current = sampleFullscreenTop(frame);
    if (countChangedPixels(previous, current) === 0 || Date.now() >= deadline) {
      return current;
    }
    previous = current;
  }
}

function nearWhiteFraction(pixels: Uint8Array): number {
  let white = 0;
  for (let i = 0; i < pixels.length; i += 4) {
    if (pixels[i] >= 250 && pixels[i + 1] >= 250 && pixels[i + 2] >= 250) {
      white++;
    }
  }
  return white / (pixels.length / 4);
}

async function waitForWhiteRegion(frame: number, x: number, y: number, w: number, h: number): Promise<number> {
  const deadline = Date.now() + 3000;
  let fraction = 0;
  do {
    const pixels = captureWindowDCRegionPixels(frame, x, y, w, h);
    if (!pixels) {
      throw new Error("layout-callback: failed to read webview pixels");
    }
    fraction = nearWhiteFraction(pixels);
    if (fraction >= 0.98) {
      // The WebView2 composition surface can arrive just after the Win32
      // layout. Require the white result to survive that handoff; otherwise a
      // transiently blank host would hide stale chrome that appears next.
      await sleep(250);
      const confirmed = captureWindowDCRegionPixels(frame, x, y, w, h);
      if (!confirmed) {
        throw new Error("layout-callback: failed to confirm webview pixels");
      }
      fraction = nearWhiteFraction(confirmed);
      if (fraction >= 0.98) {
        return fraction;
      }
    }
    await sleep(50);
  } while (Date.now() < deadline);
  return fraction;
}

export async function testit(): Promise<void> {
  setProcessDpiAware();
  const appdata = tmpPath("layout-callback-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    [
      "ShowToolbar = true",
      "ShowMenubar = false",
      // the sidebar stays open in fullscreen (only presentation hides it), so
      // the canvas-covers-the-frame check needs it off - and a PDF without an
      // outline can still get a generated one (#5724), zlib.3.pdf included
      "ShowToc = false",
      "UseTabs = true",
      "RestoreSession = false",
      "CheckForUpdates = false",
      "HtmlUI [",
      "\tUseFixedPageUI = false",
      "]",
      "",
    ].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, PDF]);
  try {
    await client.setNotificationsEnabled(false);
    await client.waitForRenderIdle();
    const normal = await client.layout("start");
    const normalFrame = requireItem(normal, "frame");
    const normalCanvas = requireItem(normal, "canvas");
    const normalToolbar = requireItem(normal, "toolbar");
    const normalTabs = requireItem(normal, "tabs");
    if (!normal.watching || normal.count !== 0) {
      throw new Error(`layout-callback: probe did not start cleanly: ${normal.raw}`);
    }
    if (
      !normalCanvas.visible ||
      !normalToolbar.visible ||
      !normalTabs.visible ||
      normalFrame.rect.dx <= 0 ||
      normalFrame.rect.dy <= 0
    ) {
      throw new Error(`layout-callback: initial HWND geometry is invalid:\n${normal.raw}`);
    }
    if (normal.nodes.length < 10 || !normal.nodes.some((node) => node.path === "chrome" && node.kind === "vbox")) {
      throw new Error(`layout-callback: recursive layout tree is missing:\n${normal.raw}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    let fullscreen = await client.layout();
    let deadline = Date.now() + 3000;
    while (
      (fullscreen.count === 0 ||
        requireItem(fullscreen, "tabs").visible ||
        requireItem(fullscreen, "canvas").rect.y !== 0) &&
      Date.now() < deadline
    ) {
      await sleep(20);
      fullscreen = await client.layout();
    }
    const fullFrame = requireItem(fullscreen, "frame");
    const fullCanvas = requireItem(fullscreen, "canvas");
    const fullTabs = requireItem(fullscreen, "tabs");
    if (fullscreen.count !== 1 || fullTabs.visible) {
      throw new Error(`layout-callback: fullscreen chrome layout is wrong:\n${fullscreen.raw}`);
    }
    if (
      fullCanvas.rect.x !== 0 ||
      fullCanvas.rect.y !== 0 ||
      fullCanvas.rect.dx !== fullFrame.rect.dx ||
      fullCanvas.rect.dy !== fullFrame.rect.dy
    ) {
      throw new Error(`layout-callback: fullscreen canvas does not cover the frame:\n${fullscreen.raw}`);
    }

    const immediatePixels = sampleFullscreenTop(frame);
    const settledPixels = await sampleStableFullscreenTop(frame);
    const stalePixels = countChangedPixels(immediatePixels, settledPixels);
    if (stalePixels > 20) {
      throw new Error(`layout-callback: ${stalePixels} stale normal-window pixels remained after entering fullscreen`);
    }

    sendCommandSync(frame, cmdId("CmdToggleFullscreen"));
    deadline = Date.now() + 3000;
    let restored = await client.layout();
    while (!requireItem(restored, "tabs").visible && Date.now() < deadline) {
      await sleep(20);
      restored = await client.layout();
    }
    if (!requireItem(restored, "tabs").visible) {
      throw new Error(`layout-callback: tabs did not return after leaving fullscreen:\n${restored.raw}`);
    }
    await client.waitForRenderIdle();

    const before = await client.layout("reset");
    const canvasBefore = requireItem(before, "canvas");
    // the toolbar cycles pinned -> overlay -> hidden, one relayout per step,
    // so it takes two commands to get it off the canvas
    let after = before;
    for (const wantHidden of [false, true]) {
      await client.layout("reset");
      sendCommandSync(frame, cmdId("CmdToggleToolbar"));
      deadline = Date.now() + 3000;
      const done = (l: LayoutInfo) => l.count === 1 && (!wantHidden || !requireItem(l, "toolbar").visible);
      after = await client.layout();
      while (!done(after) && Date.now() < deadline) {
        await sleep(20);
        after = await client.layout();
      }
      if (after.count !== 1) {
        throw new Error(
          `layout-callback: toggling one toolbar caused ${after.count} completed relayouts:\n${after.raw}`,
        );
      }
    }

    const canvasAfter = requireItem(after, "canvas");
    const toolbarAfter = requireItem(after, "toolbar");
    if (toolbarAfter.visible) {
      throw new Error(`layout-callback: toolbar stayed visible:\n${after.raw}`);
    }
    if (canvasAfter.rect.dy <= canvasBefore.rect.dy) {
      throw new Error(
        `layout-callback: canvas did not grow after hiding toolbar ` +
          `(${canvasBefore.rect.dy} -> ${canvasAfter.rect.dy})`,
      );
    }

    const stopped = await client.layout("stop");
    if (stopped.watching) {
      throw new Error("layout-callback: probe did not stop");
    }
    console.log(
      `  layout probe exposed ${after.nodes.length} nodes; toolbar toggle completed in ${after.count} relayout ✓`,
    );
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const html = join(appdata, "webview-layout.html");
  writeFileSync(
    html,
    "<html style='background:transparent'><head><title>WebView layout</title></head>" +
      "<body style='background:transparent;margin:120px'>content</body></html>",
  );
  const web = await launchControlled(["-appdata", appdata, html]);
  try {
    await web.client.setNotificationsEnabled(false);
    const normal = await web.client.layout();
    const normalCanvas = requireItem(normal, "canvas");
    let white = await waitForWhiteRegion(
      web.frame,
      normalCanvas.rect.x + 20,
      normalCanvas.rect.y + 20,
      Math.min(160, normalCanvas.rect.dx - 30),
      60,
    );
    if (white < 0.98) {
      throw new Error(`layout-callback: initial HTML page was only ${(white * 100).toFixed(1)}% white`);
    }

    sendCommandSync(web.frame, cmdId("CmdToggleFullscreen"));
    let fullscreen = await web.client.layout();
    let deadline = Date.now() + 3000;
    while (
      (requireItem(fullscreen, "tabs").visible || requireItem(fullscreen, "canvas").rect.y !== 0) &&
      Date.now() < deadline
    ) {
      await sleep(20);
      fullscreen = await web.client.layout();
    }
    const fullCanvas = requireItem(fullscreen, "canvas");
    white = await waitForWhiteRegion(
      web.frame,
      fullCanvas.rect.x + 20,
      fullCanvas.rect.y,
      Math.min(160, fullCanvas.rect.dx - 30),
      60,
    );
    if (white < 0.98) {
      throw new Error(`layout-callback: fullscreen HTML top retained chrome; only ${(white * 100).toFixed(1)}% white`);
    }
    console.log("  webview fullscreen discarded the old window chrome ✓");
  } finally {
    web.client.close();
    await killAndWait(web.proc);
  }

  const tabs = await launchControlled(["-appdata", appdata, html]);
  try {
    await tabs.client.setNotificationsEnabled(false);
    sendCopyDataW(tabs.frame, kCopyDataDdeW, `[Open("${EPUB}")]`);
    await waitForTitle(tabs.frame, (title) => title.includes("issue-5846.epub"));
    await tabs.client.waitForRenderIdle();
    for (let attempt = 0; attempt < 3; attempt++) {
      sendCommandSync(tabs.frame, cmdId("CmdPrevTab"));
      await waitForTitle(tabs.frame, (title) => title.includes("webview-layout.html"));
      const restored = await tabs.client.layout();
      const restoredCanvas = requireItem(restored, "canvas");
      const white = await waitForWhiteRegion(
        tabs.frame,
        restoredCanvas.rect.x + 20,
        restoredCanvas.rect.y + 20,
        Math.min(160, restoredCanvas.rect.dx - 30),
        60,
      );
      if (white < 0.98) {
        throw new Error(`layout-callback: restored HTML background was only ${(white * 100).toFixed(1)}% white`);
      }
      if (attempt < 2) {
        sendCommandSync(tabs.frame, cmdId("CmdNextTab"));
        await waitForTitle(tabs.frame, (title) => title.includes("issue-5846.epub"));
      }
    }
    console.log("  webview tab restore kept an opaque white page ✓");
  } finally {
    tabs.client.close();
    await killAndWait(tabs.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
