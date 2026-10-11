// Quitting while an EPUB's chapters are still laying out in the background
// crashed at exit: the DisplayModel of the closed window outlives its
// MainWindow when a render is in flight (DeleteControllerAsync hands it to a
// thread that waits for the render), and the "chapters laid out" task that
// arrives meanwhile called back into the deleted window.
//
// Driven the way the crash log showed it: a theme toggle resets the chapter
// table and restarts the layout of every chapter, and Quit lands while that
// is still running. The race is timing dependent, so the launch is repeated.
//
// Run: bun tests/epub-quit-during-layout.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { cmdId, EXE, runStandalone, tmpPath, writeAppdata } from "./util.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";
import { makeEpub } from "./epub-relayout-stale-page.ts";

const LAUNCHES = 6;
// how long after the theme toggle Quit is sent; varied so some launch hits
// the window between the chapter table reset and the end of the re-layout
const QUIT_DELAYS_MS = [0, 0, 20, 40, 80, 120];

const SETTINGS = [
  "UiLanguage = en",
  "RestoreSession = false",
  "ShowStartPage = false",
  "CheckForUpdates = false",
  "DocumentColorsFollowTheme = smart",
  "Theme = Light",
].join("\n");

export async function testit(): Promise<void> {
  const dir = tmpPath("epub-quit-during-layout-data");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "chapters.epub");
  writeFileSync(epub, makeEpub({ chapterCount: 36, parasPerChapter: 12 }));

  for (let i = 0; i < LAUNCHES; i++) {
    const appdata = writeAppdata("epub-quit-during-layout", SETTINGS);
    const log = join(dir, `log-${i}.txt`);
    const delayMs = QUIT_DELAYS_MS[i % QUIT_DELAYS_MS.length]!;
    // withControlledSumatra throws when the process crashes on the way out
    await withControlledSumatra(
      EXE,
      async (client, proc) => {
        const frame = await waitForFrame(proc.pid!);
        await client.waitForRenderIdle(30000);
        // a jump deep into the book starts renders and lays out chapters
        await client.goToLocation(30, 1);
        // resets the chapter table and re-lays out every chapter
        sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
        await Bun.sleep(delayMs);
      },
      ["-appdata", appdata, "-log-to-file", log, "-window-pos", "1000x900@40x40", "-view", "continuous", epub],
    );
    console.log(`epub-quit-during-layout: launch ${i + 1}/${LAUNCHES} (quit after ${delayMs} ms) ok`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
