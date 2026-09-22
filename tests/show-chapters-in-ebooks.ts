// ShowChaptersInEbooks is off by default: a chaptered EPUB shows a flat page
// in the toolbar, and the remaining chapters are counted in the background
// until every chapter is laid out. On, the toolbar shows the chapter box.
// Navigation stays chapter-based either way.
//
// Run: bun tests/show-chapters-in-ebooks.ts [--no-build]

import { join } from "node:path";
import { ControlClient, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone, SLOW_BUILD_FACTOR, writeAppdata } from "./util.ts";
import { sleep } from "./winapi.ts";

async function waitLaidOut(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 30000 * SLOW_BUILD_FACTOR;
  let last = await client.chapterInfo();
  while (Date.now() < deadline) {
    if (!last.hasChapters || last.chapterCount < 2) {
      throw new Error(
        `show-chapters: expected a chaptered epub, got hasChapters=${last.hasChapters} chapterCount=${last.chapterCount}`,
      );
    }
    if (last.laidOut >= last.chapterCount) {
      if (last.pageCount < last.chapterCount) {
        throw new Error(`show-chapters: pageCount ${last.pageCount} < ${last.chapterCount} chapters`);
      }
      return;
    }
    await sleep(50);
    last = await client.chapterInfo();
  }
  throw new Error(
    `show-chapters: layout did not finish, laidOut=${last.laidOut}/${last.chapterCount} pageCount=${last.pageCount}`,
  );
}

async function openEpub(showChapters: boolean): Promise<void> {
  const appdata = writeAppdata(
    showChapters ? "show-chapters-on" : "show-chapters-off",
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      `ShowChaptersInEbooks = ${showChapters}`,
    ].join("\n"),
  );
  const epub = join(ROOT, "tests", "issue-6095.epub");
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle(30000);
      const info = await client.chapterInfo();
      if (info.chapterUi !== showChapters) {
        throw new Error(`show-chapters: chapterUi=${info.chapterUi}, want ${showChapters}`);
      }
      if (!info.hasChapters) {
        throw new Error("show-chapters: document has no chapters");
      }
      const got = await client.goToLocation(info.chapter, info.page);
      if (got.chapter !== info.chapter || got.page !== info.page) {
        throw new Error(`show-chapters: goToLocation landed on ${got.chapter}:${got.page}`);
      }
      await waitLaidOut(client);
    },
    ["-appdata", appdata, "-window-pos", "1000x900@40x40", epub],
  );
}

export async function testit(): Promise<void> {
  await openEpub(false);
  await openEpub(true);
}

if (import.meta.main) {
  await runStandalone(testit);
}
