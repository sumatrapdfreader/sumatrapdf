// Quit while background chapter layout is still inside FreeType. Those faces
// alias the system-font cache, which exit frees. ASan reports the use-after-free.
//
// Chapter 1 is short so open returns quickly. Chapter 2 is long so the layout
// thread is still measuring text when we quit.
//
// Run: bun tests/font-cache-shutdown.ts

import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { withControlledSumatra } from "./control.ts";
import { makeEpub } from "./epub-relayout-stale-page.ts";
import { EXE, runStandalone, tmpPath, writeAppdata } from "./util.ts";

const PASSES = 1;

export async function testit(): Promise<void> {
  const dir = tmpPath("font-cache-shutdown-data");
  mkdirSync(dir, { recursive: true });
  const epub = join(dir, "chapters.epub");
  // chapter 1 fills the window so nothing asks the UI to lay chapter 2 out.
  // chapter 2 stays on the background thread, still inside FreeType at quit.
  writeFileSync(epub, makeEpub({ parasPerChapter: [40, 40_000] }));

  const appdata = writeAppdata(
    "font-cache-shutdown",
    ["UiLanguage = en", "RestoreSession = false", "ShowStartPage = false", "CheckForUpdates = false"].join("\n"),
  );

  for (let i = 0; i < PASSES; i++) {
    const logPath = join(dir, `log-${i}.txt`);
    await withControlledSumatra(EXE, async () => {}, ["-appdata", appdata, "-log-to-file", logPath, epub]);
    const log = readFileSync(logPath, "utf8");
    // a stuck engine must not look like a pass: we leak the cache instead of freeing it
    if (log.includes("timed out waiting for engines")) {
      throw new Error("shutdown leaked the system font cache; chapter layout outlasted the wait");
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
