// #6312: clicking a DjVu bookmark did nothing. A TOC click navigates with a
// scroll-to snapshot of the destination, which EngineDjvuDec::HandleLink rejected.
//
// Run: bun tests/issue-6312.ts [--no-build]

import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { EXE, ROOT, runStandalone } from "./util.ts";

// 20 pages, bookmark N -> page N
const FIXTURE = join(ROOT, "tests", "issue-6312.djvu");

const NAV_TIMEOUT_MS = 5000;
const POLL_MS = 50;

export async function testit(): Promise<void> {
  await withControlledSumatra(
    EXE,
    async (client) => {
      for (const bookmarkNo of [5, 20, 2]) {
        await client.tocSidebarNav(bookmarkNo);

        const deadline = Date.now() + NAV_TIMEOUT_MS;
        let page = 0;
        while (Date.now() < deadline) {
          page = (await client.chapterInfo()).page;
          if (page === bookmarkNo) {
            break;
          }
          await Bun.sleep(POLL_MS);
        }
        if (page !== bookmarkNo) {
          throw new Error(`issue-6312: bookmark ${bookmarkNo} went to page ${page}`);
        }
      }
      console.log("issue-6312: DjVu bookmarks navigate OK");
    },
    [FIXTURE],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
