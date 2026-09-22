// Laying out an EPUB chapter changes the page count, which aborts the renders
// in flight. An aborted render cannot close its draw device (the clip stack is
// unbalanced), and dropping it unclosed made mupdf log "dropping unclosed
// device" on every EPUB open. The engine now unhooks close on an aborted
// device chain before dropping it.
//
// Run: bun tests/epub-no-unclosed-device.ts [--no-build]   (or via tests/run-almost-all.ts)

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const WARNING = "dropping unclosed device";

export async function testit(): Promise<void> {
  const epub = join(ROOT, "tests", "issue-6095.epub");
  const log = tmpPath("epub-no-unclosed-device.log");

  const { proc, client, frame } = await launchControlled(["-log-to-file", log, epub]);
  try {
    await client.waitForRenderIdle(30000);
    // lay out every chapter and turn pages while renders are in flight
    const info = await client.chapterInfo();
    for (let c = 2; c <= info.chapterCount; c++) {
      await client.goToLocation(c, 1);
      sendCommandSync(frame, cmdId("CmdGoToNextPage"));
    }
    await client.waitForRenderIdle(30000);
  } finally {
    client.close();
    await killAndWait(proc);
  }

  const text = readFileSync(log, "utf8");
  const n = text.split("\n").filter((l) => l.includes(WARNING)).length;
  if (n > 0) {
    throw new Error(`epub-no-unclosed-device: '${WARNING}' logged ${n} times`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
