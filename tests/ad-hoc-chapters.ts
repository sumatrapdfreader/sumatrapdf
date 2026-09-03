// Ad-hoc verification for the chapter-aware Location work (see
// docs/location-chapters-plan.md). Opens the two big chaptered test files from
// OneDrive and checks: fast open (only chapter 1 laid out), chapter/page
// navigation across chapter edges, GoToLocation, and TOC navigation into a
// chapter that was not laid out yet.
//
// Not registered in run-almost-all/run-all: needs large files that live in
// OneDrive, not the repo.
//
// Run: bun tests/ad-hoc-chapters.ts [--no-build]

import { existsSync, readFileSync } from "node:fs";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, killProcessesNamed, launchControlled, sendCommandSync } from "./win-automation.ts";

const MOBI = String.raw`C:\Users\kjk\OneDrive\!sumatra\1000.mobi`;
const EPUB = String.raw`C:\Users\kjk\OneDrive\!sumatra\Dune - Frank Herbert.epub`;

// whole-section hard timeout: kill the running SumatraPDF and fail loudly
// rather than hang the session (see project memory gui-test-scripts-need-watchdog)
const SECTION_TIMEOUT_MS = 120_000;

async function withSectionWatchdog<T>(label: string, fn: () => Promise<T>): Promise<T> {
  let timedOut = false;
  const timer = setTimeout(() => {
    timedOut = true;
    console.error(`ad-hoc-chapters: ${label} exceeded ${SECTION_TIMEOUT_MS}ms, killing stray SumatraPDF`);
    void killProcessesNamed("SumatraPDF.exe");
  }, SECTION_TIMEOUT_MS);
  try {
    return await fn();
  } catch (e) {
    if (timedOut) {
      throw new Error(`${label}: timed out after ${SECTION_TIMEOUT_MS}ms`);
    }
    throw e;
  } finally {
    clearTimeout(timer);
  }
}

function countMatches(log: string, needle: string): number {
  return log.split("\n").filter((l) => l.includes(needle)).length;
}

function loadDocumentMs(log: string): number {
  const m = /LoadDocument: ([\d.]+) ms/.exec(log);
  if (!m) {
    throw new Error(`no 'LoadDocument: N ms' line in log:\n${log.slice(0, 2000)}`);
  }
  return Number(m[1]);
}

async function flatPageNo(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page"]);
  const exitCode = res[0] as number;
  const raw = String(res[1] ?? "");
  if (exitCode !== 0) {
    throw new Error(`TestFavoriteNav page failed: ${raw}`);
  }
  const m = /page=(-?\d+)/.exec(raw);
  if (!m) {
    throw new Error(`no page= in: ${raw}`);
  }
  return Number(m[1]);
}

// poll TestTocNavigate until it stops reporting NOTREADY (lazy TOC resolution
// can still be settling right after render-idle)
async function tocNavigate(client: ControlClient, destNo: number): Promise<{ exitCode: number; raw: string }> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestTocNavigate, [destNo]);
    const exitCode = res[0] as number;
    const raw = String(res[1] ?? "").trim();
    if (!raw.includes("NOTREADY")) {
      return { exitCode, raw };
    }
    if (Date.now() > deadline) {
      throw new Error(`tocNavigate: never ready: ${raw}`);
    }
    await new Promise((r) => setTimeout(r, 100));
  }
}

function assertEq(label: string, got: unknown, want: unknown): void {
  if (got !== want) {
    throw new Error(`${label}: got ${got}, want ${want}`);
  }
}

async function testMobi(): Promise<void> {
  if (!existsSync(MOBI)) {
    console.log(`ad-hoc-chapters: skip MOBI section, missing ${MOBI}`);
    return;
  }

  await withSectionWatchdog("mobi", async () => {
    const log = tmpPath("ad-hoc-chapters-mobi.log");
    const { proc, client, frame } = await launchControlled(["-log-to-file", log, MOBI]);
    try {
      await client.waitForRenderIdle();

      const logText = readFileSync(log, "utf8");
      const openMs = loadDocumentMs(logText);
      console.log(`mobi: LoadDocument ${openMs} ms`);
      if (openMs >= 3000) {
        throw new Error(`mobi: LoadDocument took ${openMs} ms, want < 3000 ms (debug build)`);
      }
      const layoutsAtOpen = countMatches(logText, "LayOutChapter:");
      assertEq("mobi: LayOutChapter calls at open", layoutsAtOpen, 1);

      const info0 = await client.chapterInfo();
      if (!info0.hasChapters) {
        throw new Error("mobi: expected HasChapters");
      }
      assertEq("mobi: ChapterCount", info0.chapterCount, 892);

      // CmdGoToNextPage x15: flat pageNo must climb by exactly 1 each time,
      // including across a chapter boundary (a chapter that gets laid out
      // and stops being a 1-page placeholder must not skip/duplicate a pageNo)
      let prevPage = await flatPageNo(client);
      for (let i = 0; i < 15; i++) {
        sendCommandSync(frame, cmdId("CmdGoToNextPage"));
        await client.waitForRenderIdle();
        const page = await flatPageNo(client);
        assertEq(`mobi: flat pageNo after next #${i + 1}`, page, prevPage + 1);
        prevPage = page;
      }

      // GoToLocation(50, 1): lands exactly there and lays out chapter 50 on demand
      const got50 = await client.goToLocation(50, 1);
      assertEq("mobi: goToLocation(50,1) chapter", got50.chapter, 50);
      assertEq("mobi: goToLocation(50,1) page", got50.page, 1);
      const logAfter50 = readFileSync(log, "utf8");
      if (!/LayOutChapter: chapter 50 ->/.test(logAfter50)) {
        throw new Error("mobi: expected 'LayOutChapter: chapter 50' in log after goToLocation(50,1)");
      }

      // CmdGoToPrevPage from {50,1} crosses back into chapter 49's last page
      sendCommandSync(frame, cmdId("CmdGoToPrevPage"));
      await client.waitForRenderIdle();
      const info49 = await client.chapterInfo();
      assertEq("mobi: prev-page-across-edge chapter", info49.chapter, 49);
      assertEq("mobi: prev-page-across-edge page == ChapterPageCount(49)", info49.page, info49.chapterPageCount);

      // CmdGoToLastPage lands on the last chapter
      sendCommandSync(frame, cmdId("CmdGoToLastPage"));
      await client.waitForRenderIdle();
      const infoLast = await client.chapterInfo();
      assertEq("mobi: last page chapter", infoLast.chapter, 892);

      // TOC navigation deep into the book resolves through a chapter that
      // was not laid out yet
      const { exitCode, raw } = await tocNavigate(client, 500);
      console.log(`mobi: TestTocNavigate(500) -> ${raw}`);
      if (exitCode !== 0) {
        throw new Error(`mobi: TestTocNavigate(500) failed: ${raw}`);
      }

      // Real sidebar TOC click (B6): before the fix, SnapshotDestForDeferredNav
      // dropped the destination for any TOC item whose target chapter hadn't
      // laid out yet, so clicking a bookmark deep in the book did nothing.
      // TestTocSidebarNav drives the exact deferred-nav path a click uses
      // (GoToTocItem -> NewGoToTocLinkData -> SnapshotDestForDeferredNav ->
      // uitask) rather than calling HandleLink directly, so it doesn't mask a
      // regression by pre-resolving the destination.
      await client.goToLocation(1, 1);
      await client.waitForRenderIdle();
      const beforeSidebar = await client.chapterInfo();
      await client.tocSidebarNav(500);
      let sidebarInfo = beforeSidebar;
      const sidebarDeadline = Date.now() + 10_000;
      while (Date.now() < sidebarDeadline && sidebarInfo.chapter === beforeSidebar.chapter) {
        await new Promise((r) => setTimeout(r, 100));
        sidebarInfo = await client.chapterInfo();
      }
      console.log(`mobi: sidebar TOC click landed chapter=${sidebarInfo.chapter}`);
      if (sidebarInfo.chapter === beforeSidebar.chapter) {
        throw new Error(`mobi: sidebar TOC click did not navigate (stayed at chapter ${beforeSidebar.chapter})`);
      }

      // Favorite in a chapter that isn't laid out yet (B8): add it, jump away,
      // then jump back via the favorite. The favorite must store an engine
      // bookmark (not just a flat pageNo, which would shift as chapters lay
      // out) and land back on the same chapter.
      const got30 = await client.goToLocation(30, 1);
      assertEq("mobi: goToLocation(30,1) chapter", got30.chapter, 30);
      const favPage = await flatPageNo(client);
      const addRes = await client.request(ControlCommand.TestFavoriteNav, ["add", favPage]);
      assertEq("mobi: favorite add exit code", addRes[0], 0);
      sendCommandSync(frame, cmdId("CmdGoToFirstPage"));
      await client.waitForRenderIdle();
      const afterFirst = await client.chapterInfo();
      assertEq("mobi: CmdGoToFirstPage chapter", afterFirst.chapter, 1);
      const gotoFavRes = await client.request(ControlCommand.TestFavoriteNav, ["goto-fav", favPage]);
      assertEq("mobi: favorite goto exit code", gotoFavRes[0], 0);
      await client.waitForRenderIdle();
      const afterFav = await client.chapterInfo();
      assertEq("mobi: favorite jump chapter", afterFav.chapter, 30);

      // In-page links resolve lazily (CreatePageLink -> GetNamedDestLazy):
      // must not crash / fail to load page 1 while laying out a link target.
      const linksRes = await client.request(ControlCommand.TestPageLinks, [MOBI, 1]);
      const linksDump = String(linksRes[1] ?? "");
      if (linksDump.includes("engine-create-failed") || linksDump.includes("page-load-failed")) {
        throw new Error(`mobi: TestPageLinks(1) failed:\n${linksDump}`);
      }

      // Selection survives a chapter layout (C9): PagesRenumbered used to
      // unconditionally wipe the selection on every chapter (re)layout.
      const sel = await client.selectionSurvivesRenumber(60);
      console.log(
        `mobi: selection survives chapter layout -> survived=${sel.survived} pageNo=${sel.pageNo} ` +
          `textSurvived=${sel.textSurvived}`,
      );
      if (!sel.survived) {
        throw new Error("mobi: selection did not survive a chapter layout");
      }
      if (!sel.textSurvived) {
        throw new Error("mobi: text selection did not survive a chapter layout");
      }

      await client.quit();
    } catch (e) {
      await killAndWait(proc);
      throw e;
    }
    const exitCode = await proc.exited;
    assertEq("mobi: process exit code", exitCode, 0);
    console.log("ad-hoc-chapters: mobi section OK");
  });
}

async function testEpub(): Promise<void> {
  if (!existsSync(EPUB)) {
    console.log(`ad-hoc-chapters: skip EPUB section, missing ${EPUB}`);
    return;
  }

  await withSectionWatchdog("epub", async () => {
    const log = tmpPath("ad-hoc-chapters-epub.log");
    const { proc, client, frame } = await launchControlled(["-log-to-file", log, EPUB]);
    try {
      await client.waitForRenderIdle();

      const info0 = await client.chapterInfo();
      if (!info0.hasChapters) {
        throw new Error("epub: expected HasChapters");
      }
      assertEq("epub: ChapterCount", info0.chapterCount, 62);

      const got30 = await client.goToLocation(30, 1);
      assertEq("epub: goToLocation(30,1) chapter", got30.chapter, 30);
      assertEq("epub: goToLocation(30,1) page", got30.page, 1);

      sendCommandSync(frame, cmdId("CmdGoToPrevPage"));
      await client.waitForRenderIdle();
      const info29 = await client.chapterInfo();
      assertEq("epub: prev-page-across-edge chapter", info29.chapter, 29);
      assertEq("epub: prev-page-across-edge page == ChapterPageCount(29)", info29.page, info29.chapterPageCount);

      // Real sidebar TOC click (same deferred-nav path as the mobi section
      // above): SnapshotDestForDeferredNav must carry the resolved loc onto
      // the snapshot, otherwise the click into an unlaid chapter drops.
      await client.goToLocation(1, 1);
      await client.waitForRenderIdle();
      const beforeSidebar = await client.chapterInfo();
      await client.tocSidebarNav(40);
      let sidebarInfo = beforeSidebar;
      const sidebarDeadline = Date.now() + 10_000;
      while (Date.now() < sidebarDeadline && sidebarInfo.chapter === beforeSidebar.chapter) {
        await new Promise((r) => setTimeout(r, 100));
        sidebarInfo = await client.chapterInfo();
      }
      console.log(`epub: sidebar TOC click landed chapter=${sidebarInfo.chapter}`);
      if (sidebarInfo.chapter === beforeSidebar.chapter) {
        throw new Error(`epub: sidebar TOC click did not navigate (stayed at chapter ${beforeSidebar.chapter})`);
      }

      await client.quit();
    } catch (e) {
      await killAndWait(proc);
      throw e;
    }
    const exitCode = await proc.exited;
    assertEq("epub: process exit code", exitCode, 0);
    console.log("ad-hoc-chapters: epub section OK");
  });
}

export async function testit(): Promise<void> {
  await killProcessesNamed("SumatraPDF.exe");
  try {
    await testMobi();
    await testEpub();
  } finally {
    await killProcessesNamed("SumatraPDF.exe");
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
