// #6203: with the sidebar on the right and both Bookmarks and Favorites shown,
// the panes kept the x they were created at when the frame was resized, so
// they sat left of their slot, under the canvas, and ran short of the right
// edge. The shrink half of the bug is covered by Splitter_ShrinkTest.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { LayoutInfo } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Type /Outlines /Count 1 /First 5 0 R /Last 5 0 R >>",
    "<< /Title (Chapter) /Parent 4 0 R /Dest [3 0 R /XYZ null null null] >>",
  ]);
}

function rect(layout: LayoutInfo, name: string) {
  const it = layout.items[name];
  if (!it || !it.visible) {
    throw new Error(`issue-6203: '${name}' not visible in:\n${layout.raw}`);
  }
  return it.rect;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6203");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "a.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\n" +
      "ShowToc = true\nShowFavorites = true\nSidebarOnRight = true\nSidebarDx = 250\n",
  );

  const { proc, client } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    const deadline = Date.now() + 8000;
    let msg = "";
    while (Date.now() < deadline) {
      const layout = await client.layout();
      const canvas = rect(layout, "canvas");
      const toc = rect(layout, "toc");
      const fav = rect(layout, "favorites");
      const canvasRight = canvas.x + canvas.dx;
      msg = `canvas right=${canvasRight} toc x=${toc.x} fav x=${fav.x}`;
      if (toc.x >= canvasRight && fav.x >= canvasRight && toc.x === fav.x) {
        return;
      }
      await sleep(100);
    }
    throw new Error(`issue-6203: right sidebar overlaps canvas: ${msg}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
