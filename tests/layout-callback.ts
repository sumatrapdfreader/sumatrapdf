// The main-window layout probe exposes geometry through -dbg-control and
// counts completed relayouts through gAfterLayout. This checks both parts
// without taking a screenshot or reading pixels from the desktop.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, killAndWait, sendCommandSync } from "./win-automation.ts";
import { sleep } from "./winapi.ts";
import type { LayoutInfo, LayoutItem } from "./control.ts";

const PDF = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

function requireItem(layout: LayoutInfo, name: string): LayoutItem {
  const item = layout.items[name];
  if (!item) {
    throw new Error(`layout-callback: missing '${name}' in:\n${layout.raw}`);
  }
  return item;
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
    await client.waitForRenderIdle();
    const before = await client.layout("start");
    const frameBefore = requireItem(before, "frame");
    const canvasBefore = requireItem(before, "canvas");
    const toolbarBefore = requireItem(before, "toolbar");
    if (!before.watching || before.count !== 0) {
      throw new Error(`layout-callback: probe did not start cleanly: ${before.raw}`);
    }
    if (!canvasBefore.visible || !toolbarBefore.visible || frameBefore.rect.dx <= 0 || frameBefore.rect.dy <= 0) {
      throw new Error(`layout-callback: initial HWND geometry is invalid:\n${before.raw}`);
    }
    if (before.nodes.length < 10 || !before.nodes.some((node) => node.path === "chrome" && node.kind === "vbox")) {
      throw new Error(`layout-callback: recursive layout tree is missing:\n${before.raw}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleToolbar"));
    const deadline = Date.now() + 3000;
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
