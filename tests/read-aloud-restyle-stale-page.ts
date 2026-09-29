// Reading aloud deep in a chaptered EPUB, then toggling the theme: the restyle
// collapses the chapter table, so the page numbers in the read aloud highlight
// map are past the new page count. The highlight timer's auto-scroll converted
// them with DisplayModel::CvtToScreen, which reported !pageInfo
// (crash 2026-09-29-06-19-0526). The map also has to follow the renumbering:
// the page it reports for the spoken word must stay in the chapter being read.
//
// Run: bun tests/read-aloud-restyle-stale-page.ts [--no-build]

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, DEBUG_REPORT_EXIT_CODE, withControlledSumatra } from "./control.ts";
import { makeEpub } from "./epub-relayout-stale-page.ts";
import { cmdId, EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, writeAppdata } from "./util.ts";
import { sleep } from "./winapi.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";

const DEEP_CHAPTER = 34;
const THEME_TOGGLES = 4;

const SAMPLES_PER_TOGGLE = 6;

type TtsState = { voices: number; speaking: number; page: number; chapter: number };

async function ttsState(client: ControlClient): Promise<TtsState> {
  const res = await client.request(ControlCommand.TestReadAloudPlaybackBar, []);
  const out = String(res[1] ?? "");
  const voices = /voices=(\d+)/.exec(out);
  const speaking = /speaking=(\d+)/.exec(out);
  const progress = /progress page=(-?\d+) loc=(-?\d+):(-?\d+)/.exec(out);
  if (!voices || !speaking || !progress) {
    throw new Error(`read-aloud-restyle-stale-page: could not parse: ${out.trim()}`);
  }
  return { voices: +voices[1]!, speaking: +speaking[1]!, page: +progress[1]!, chapter: +progress[2]! };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("read-aloud-restyle-stale-page-data");
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "chapters.epub");
  writeFileSync(epub, makeEpub());

  // with the default DocumentColorsFollowTheme only the first toggle restyles
  const appdata = writeAppdata(
    "read-aloud-restyle-stale-page",
    [
      "UiLanguage = en",
      "RestoreSession = false",
      "ShowStartPage = false",
      "CheckForUpdates = false",
      "DocumentColorsFollowTheme = smart",
      "Theme = Light",
    ].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      if ((await ttsState(client)).voices === 0) {
        console.log("SKIP read-aloud-restyle-stale-page: no TTS voices on this machine");
        return;
      }

      // chapter by chapter, so the flat page count grows past the collapsed one
      for (let ch = 1; ch <= DEEP_CHAPTER; ch++) {
        await client.goToLocation(ch, 1);
      }
      await client.waitForRenderIdle(30000);

      sendCommandSync(frame, cmdId("CmdReadAloudFromTopPage"));
      const deadline = Date.now() + 12_000 * SLOW_BUILD_FACTOR;
      // speaking, with a spoken position that maps to a page
      for (;;) {
        const st = await ttsState(client);
        if (st.speaking === 1 && st.page > 0) {
          break;
        }
        if (Date.now() > deadline) {
          throw new Error("read-aloud-restyle-stale-page: read aloud never started speaking");
        }
        await sleep(80);
      }

      const before = await ttsState(client);
      if (before.chapter < DEEP_CHAPTER) {
        throw new Error(`read-aloud-restyle-stale-page: reading chapter ${before.chapter}, expected ${DEEP_CHAPTER}+`);
      }

      // sample while earlier chapters are still collapsed to placeholders
      let resolved = 0;
      for (let i = 0; i < THEME_TOGGLES; i++) {
        sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
        for (let n = 0; n < SAMPLES_PER_TOGGLE; n++) {
          const st = await ttsState(client);
          const inChapter = st.chapter >= before.chapter && st.chapter <= before.chapter + 1;
          if (st.page > 0 && !inChapter) {
            throw new Error(
              `read-aloud-restyle-stale-page: after restyle ${i + 1} read aloud is on page ${st.page} ` +
                `(chapter ${st.chapter}), was reading chapter ${before.chapter}`,
            );
          }
          if (st.page > 0) {
            resolved++;
          }
          await sleep(80);
        }
      }
      if (resolved === 0) {
        throw new Error("read-aloud-restyle-stale-page: no spoken page after any restyle, nothing was checked");
      }

      // a debug report ends the process, and quitting a dead one skips the exit code check
      if (proc.exitCode !== null) {
        const what =
          proc.exitCode === DEBUG_REPORT_EXIT_CODE ? "debug report (ReportIf) fired" : `exit code ${proc.exitCode}`;
        throw new Error(`read-aloud-restyle-stale-page: ${what}`);
      }

      sendCommandSync(frame, cmdId("CmdStopReadAloud"));
    },
    [
      "-appdata",
      appdata,
      "-log-to-file",
      join(dir, "log.txt"),
      "-window-pos",
      "1000x900@40x40",
      "-view",
      "continuous",
      epub,
    ],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
