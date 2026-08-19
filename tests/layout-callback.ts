// The main-window layout probe exposes geometry through -dbg-control and
// counts completed relayouts through gAfterLayout. A small direct-DC sample
// also verifies that a fullscreen transition painted its final geometry.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, killAndWait, sendCommandSync } from "./win-automation.ts";
import { captureWindowDCRegionPixels, sleep } from "./winapi.ts";
import type { LayoutInfo, LayoutItem } from "./control.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

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

export async function testit(): Promise<void> {
  const appdata = tmpPath("layout-callback-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "ShowToolbar = true\nShowMenubar = false\nRestoreSession = false\nCheckForUpdates = false\n",
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
    sendCommandSync(frame, cmdId("CmdToggleToolbar"));
    deadline = Date.now() + 3000;
    let after = await client.layout();
    while ((after.count === 0 || requireItem(after, "toolbar").visible) && Date.now() < deadline) {
      await sleep(20);
      after = await client.layout();
    }

    const canvasAfter = requireItem(after, "canvas");
    const toolbarAfter = requireItem(after, "toolbar");
    if (after.count !== 1) {
      throw new Error(`layout-callback: toggling one toolbar caused ${after.count} completed relayouts:\n${after.raw}`);
    }
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
}

if (import.meta.main) {
  await runStandalone(testit);
}
