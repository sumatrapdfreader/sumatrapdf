// With ShowToc = true, the Bookmarks sidebar must show outline items after
// open without a window resize. TreeView items inserted while the sidebar was
// hidden or 0-sized used to stay blank until the user resized the frame.
//
// Run: bun tests/toc-show-on-open.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, runStandalone, tmpPath } from "./util";
import { launchControlled, killAndWait } from "./win-automation";
import { countVisibleTreeRows, findChildWindow, sleep } from "./winapi";

function makePdf(): string {
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R /Outlines 5 0 R /PageMode /UseOutlines >>`,
    `<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
    `<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>`,
    `<< /Type /Outlines /Count 2 /First 6 0 R /Last 7 0 R >>`,
    `<< /Title (FirstBookmark) /Parent 5 0 R /Next 7 0 R /Dest [3 0 R /XYZ null null null] >>`,
    `<< /Title (SecondBookmark) /Parent 5 0 R /Prev 6 0 R /Dest [4 0 R /XYZ null null null] >>`,
  ];
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("toc-show-on-open.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const appdata = tmpPath("toc-show-on-open-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "ShowToc = true\nShowFavorites = false\nRestoreSession = false\nCheckForUpdates = false\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();

    const deadline = Date.now() + 4000;
    let tree = 0;
    let rows = 0;
    while (Date.now() < deadline) {
      tree = findChildWindow(frame, "SysTreeView32");
      if (tree) {
        rows = countVisibleTreeRows(tree);
        if (rows >= 2) {
          break;
        }
      }
      await sleep(40);
    }
    if (!tree) {
      throw new Error("toc-show-on-open: bookmarks tree did not appear");
    }
    if (rows < 2) {
      throw new Error(`toc-show-on-open: bookmarks sidebar empty (${rows} rows)`);
    }
    console.log(`toc-show-on-open: ${rows} bookmark rows`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
