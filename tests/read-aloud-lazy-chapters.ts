// Read aloud from the start of a chaptered EPUB whose later chapters are still
// placeholders (one page each until background layout publishes them). The
// text to speak is collected page by page to the end of the book, and each
// placeholder grows when its text is extracted; collecting up to the page
// count taken before that growth cut off the end of the book.
//
// Run: bun tests/read-aloud-lazy-chapters.ts [--no-build]

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { makeEpub } from "./epub-relayout-stale-page.ts";
import { cmdId, EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath, writeAppdata } from "./util.ts";
import { sleep } from "./winapi.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";

type MapState = {
  voices: number;
  speaking: number;
  endChapter: number;
  pages: number;
  span: number;
  raw: string;
};

async function mapState(client: ControlClient): Promise<MapState> {
  const res = await client.request(ControlCommand.TestReadAloudPlaybackBar, []);
  const out = String(res[1] ?? "");
  const voices = /voices=(\d+)/.exec(out);
  const speaking = /speaking=(\d+)/.exec(out);
  const map = /map start=(-?\d+):(-?\d+) end=(-?\d+):(-?\d+) pages=(-?\d+) span=(-?\d+)/.exec(out);
  if (!voices || !speaking || !map) {
    throw new Error(`read-aloud-lazy-chapters: could not parse: ${out.trim()}`);
  }
  return {
    voices: +voices[1]!,
    speaking: +speaking[1]!,
    endChapter: +map[3]!,
    pages: +map[5]!,
    span: +map[6]!,
    raw: map[0]!,
  };
}

export async function testit(): Promise<void> {
  const dir = tmpPath("read-aloud-lazy-chapters-data");
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "chapters.epub");
  writeFileSync(epub, makeEpub());

  const appdata = writeAppdata(
    "read-aloud-lazy-chapters",
    ["UiLanguage = en", "RestoreSession = false", "ShowStartPage = false", "CheckForUpdates = false"].join("\n"),
  );

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);

      if ((await mapState(client)).voices === 0) {
        console.log("SKIP read-aloud-lazy-chapters: no TTS voices on this machine");
        return;
      }

      // start before background layout publishes the later chapters
      const info = await client.chapterInfo();
      sendCommandSync(frame, cmdId("CmdReadAloudFromTopPage"));
      if (info.laidOut >= info.chapterCount) {
        throw new Error(
          `read-aloud-lazy-chapters: all ${info.chapterCount} chapters were laid out before read aloud started`,
        );
      }

      // compare once every chapter is laid out, so the span is in final page numbers
      const deadline = Date.now() + 30_000 * SLOW_BUILD_FACTOR;
      for (;;) {
        const ci = await client.chapterInfo();
        if (ci.laidOut >= ci.chapterCount) {
          break;
        }
        if (Date.now() > deadline) {
          throw new Error(`read-aloud-lazy-chapters: only ${ci.laidOut} of ${ci.chapterCount} chapters laid out`);
        }
        await sleep(100);
      }
      await client.waitForRenderIdle(30000);

      const st = await mapState(client);
      sendCommandSync(frame, cmdId("CmdStopReadAloud"));
      if (st.endChapter !== info.chapterCount || st.pages !== st.span) {
        throw new Error(
          `read-aloud-lazy-chapters: expected text through chapter ${info.chapterCount} with every page, got ${st.raw}`,
        );
      }
    },
    ["-appdata", appdata, "-window-pos", "1000x900@40x40", "-view", "continuous", epub],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
