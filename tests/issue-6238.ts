// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6238
//
// In Facing / Book view, Next Page in a chaptered EPUB stepped one page in
// chapter coordinates, which is the other page of the row already on screen,
// so nothing moved. Prev Page from the second row landed on the first row's
// second page, so it seemed to work. Both must step a whole row.
//
// issue-6095.epub has three spine items (chapters); part1 (chapter 2) is long
// enough to span several pages at the window size used here.
//
// Run: bun tests/issue-6238.ts [--no-build]   (or via tests/run-almost-all.ts)

import { join } from "node:path";
import { ControlClient, withControlledSumatra } from "./control.ts";
import { cmdId, EXE, ROOT, runStandalone } from "./util.ts";
import { sendCommandSync, waitForFrame } from "./win-automation.ts";

async function pageAfter(client: ControlClient, frame: number, cmd: string): Promise<number> {
  sendCommandSync(frame, cmdId(cmd));
  await client.waitForRenderIdle(30000);
  return (await client.chapterInfo()).page;
}

function expectPage(label: string, got: number, want: number): void {
  if (got !== want) {
    throw new Error(`issue-6238: ${label}: on chapter page ${got}, want ${want}`);
  }
}

// view: the -view name; startPage: a chapter-2 page that opens a row of two
// pages; next/prev: the chapter-2 page Next / Prev Page must land on
async function checkView(view: string, startPage: number, next: number, prev: number): Promise<void> {
  const epub = join(ROOT, "tests", "issue-6095.epub");

  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      const frame = await waitForFrame(proc.pid!);
      await client.waitForRenderIdle(30000);
      await client.setNotificationsEnabled(false);

      const info = await client.chapterInfo();
      if (!info.hasChapters) {
        throw new Error("issue-6238: fixture must have chapters");
      }
      const start = await client.goToLocation(2, startPage);
      expectPage(`${view}: goToLocation(2,${startPage})`, start.page, startPage);
      const n = (await client.chapterInfo()).chapterPageCount;
      if (n < next + 2) {
        throw new Error(`issue-6238: chapter 2 has ${n} pages, need at least ${next + 2}`);
      }

      expectPage(`${view}: next`, await pageAfter(client, frame, "CmdGoToNextPage"), next);
      expectPage(`${view}: prev`, await pageAfter(client, frame, "CmdGoToPrevPage"), prev);
    },
    ["-window-pos", "1000x700@40x40", "-view", view, epub],
  );
}

export async function testit(): Promise<void> {
  // chapter 1 is the one-page nav, so flat page 2 is chapter 2 page 1.
  // facing rows: (1,2) (3,4) (5,6) -> chapter-2 pages (2,3) (4,5)
  await checkView("facing", 2, 4, 2);
  // book rows: (1) (2,3) (4,5) -> chapter-2 pages (1,2) (3,4)
  await checkView("book view", 1, 3, 1);
}

if (import.meta.main) {
  await runStandalone(testit);
}
