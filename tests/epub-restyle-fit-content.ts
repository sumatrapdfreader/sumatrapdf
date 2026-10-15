// Crash 2026-10-15-11-38-b11f: `!pageInfo` in PageSizeAfterRotation <-
// CalcZoomReal <- Relayout <- SyncWithEngineLayout after an EPUB restyle.
// Facing + Fit Content deep in a chaptered EPUB: a theme toggle collapses the
// page count, and Relayout asked for the zoom of the stale startPage.
//
// Run: bun tests/epub-restyle-fit-content.ts [--no-build]

import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { makeEpub } from "./epub-relayout-stale-page.ts";
import { cmdId, EXE, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";

const DEEP_CHAPTER = 34;

const SETTINGS = [
  "UiLanguage = en",
  "RestoreSession = false",
  "ShowStartPage = false",
  "CheckForUpdates = false",
  "DocumentColorsFollowTheme = smart",
  "Theme = Light",
  "DefaultDisplayMode = facing",
  "DefaultZoom = fit content",
].join("\n");

export async function testit(): Promise<void> {
  const dir = tmpPath("epub-restyle-fit-content-data");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "chapters.epub");
  writeFileSync(epub, makeEpub({ parasPerChapter: 12 }));
  const log = join(dir, "log.txt");
  const appdata = writeAppdata("epub-restyle-fit-content", SETTINGS);

  let pagesBefore = 0;
  let pagesAfter = 0;
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      // chapter by chapter, so the flat page count grows past the collapsed one
      for (let ch = 1; ch <= DEEP_CHAPTER; ch++) {
        await client.goToLocation(ch, 1);
      }
      await client.waitForRenderIdle(30000);
      const deep = await client.chapterInfo();
      pagesBefore = deep.pageCount;

      // the recount that follows a restyle publishes the full total again.
      // hold it so the read below is the collapsed count Relayout just used
      await client.setChapterLayoutHold(true);
      sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
      const after = await client.chapterInfo();
      pagesAfter = after.pageCount;
      await client.setChapterLayoutHold(false);
      await client.waitForRenderIdle(30000);
    },
    ["-appdata", appdata, "-log-to-file", log, "-window-pos", "1000x900@40x40", epub],
  );

  const logText = readFileSync(log, "utf8");
  if (!logText.includes("ApplyReflowThemeCss: chapters reset")) {
    throw new Error("theme toggle did not restyle the document");
  }
  // the crash needs the restyle to shrink the page count under the current page
  if (pagesAfter < 1 || pagesAfter >= pagesBefore) {
    throw new Error(`page count went from ${pagesBefore} to ${pagesAfter}, expected it to collapse`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
