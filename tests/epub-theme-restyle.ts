// Toggling the light/dark theme on a chaptered EPUB restyles the document,
// which resets the chapter table: every chapter counts one placeholder page
// again, so the engine's page count collapses while DisplayModel still holds a
// page number from the old, larger numbering. EngineMupdf::PageMediabox()
// asserted on that stale page number (a reflow doc shares one mediabox across
// every page, so the number does not matter there).
//
// issue-6095.epub is used only because it is the repo's only multi-chapter
// EPUB; TOC destination 3 lands in its second chapter, which forces that
// chapter to lay out and the flat page count to grow.
//
// Run: bun tests/epub-theme-restyle.ts [--no-build]   (or via tests/run-almost-all.ts)

import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { cmdId, EXE, ROOT, runStandalone, SLOW_BUILD_FACTOR, writeAppdata } from "./util.ts";
import { sleep } from "./winapi.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";

// annotations.xhtml#annotation-45, i.e. a page in the second chapter
const TOC_DEST = 3;

async function followToc(client: ControlClient, destNo: number): Promise<void> {
  const res = await client.request(ControlCommand.TestDestZoomNav, [destNo, 0]);
  const raw = String(res[1] ?? "").trim();
  if (res[0] !== 0 || !/^OK /.test(raw)) {
    throw new Error(`epub-theme-restyle: could not follow the TOC destination: ${raw}`);
  }
}

export async function testit(): Promise<void> {
  const appdata = writeAppdata(
    "epub-theme-restyle",
    ["UiLanguage = en", "RestoreSession = false", "ShowStartPage = false", "CheckForUpdates = false"].join("\n"),
  );
  const epub = join(ROOT, "tests", "issue-6095.epub");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      // lay out the second chapter so the flat page count is past chapter 1
      await followToc(client, TOC_DEST);
      await client.waitForRenderIdle(30000);

      // each toggle restyles and collapses the chapter table
      for (let i = 0; i < 2; i++) {
        sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
        await sleep(500 * SLOW_BUILD_FACTOR);
        await client.waitForRenderIdle(30000);
      }
    },
    ["-appdata", appdata, "-window-pos", "1000x900@40x40", "-view", "continuous", epub],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
