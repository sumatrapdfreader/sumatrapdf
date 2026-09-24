// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6244
//
// "Show the bookmarks sidebar when available" (ShowToc) was ignored for comic
// archives: they never showed it on open, because without ComicInfo.xml their
// bookmarks are synthesized from file names (#543). Bookmarks from ComicInfo.xml
// are real and must show the sidebar; synthesized ones still must not.
//
// Run:  bun tests/issue-6244.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath, writeStoredZip } from "./util.ts";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
ShowToc = true
`;

// 1x1 PNG
const PNG = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==",
  "base64",
);

const COMIC_INFO = `<?xml version="1.0" encoding="utf-8"?>
<ComicInfo>
  <Pages>
    <Page Image="0" Bookmark="Cover"/>
    <Page Image="2" Bookmark="Chapter 1"/>
  </Pages>
</ComicInfo>
`;

function pages(): { name: string; data: Buffer }[] {
  return ["01.png", "02.png", "03.png"].map((name) => ({ name, data: PNG }));
}

async function tocVisibleOnOpen(cbz: string, appdata: string): Promise<boolean> {
  let vis = false;
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestSidebarLayout, []);
      const raw = String(res[1] ?? "").trim();
      const m = /OK .*tocVis=(\d)/.exec(raw);
      if (res[0] !== 0 || !m) {
        throw new Error(`issue-6244: TestSidebarLayout: ${raw}`);
      }
      vis = m[1] === "1";
    },
    ["-appdata", appdata, cbz],
  );
  return vis;
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("issue-6244-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const withInfo = tmpPath("issue-6244-comicinfo.cbz");
  writeStoredZip(withInfo, [...pages(), { name: "ComicInfo.xml", data: Buffer.from(COMIC_INFO) }]);
  if (!(await tocVisibleOnOpen(withInfo, appdata))) {
    throw new Error("issue-6244: ComicInfo.xml bookmarks didn't show the bookmarks sidebar on open");
  }
  console.log("  ComicInfo.xml bookmarks: sidebar shown ✓");

  const plain = tmpPath("issue-6244-plain.cbz");
  writeStoredZip(plain, pages());
  if (await tocVisibleOnOpen(plain, appdata)) {
    throw new Error("issue-6244: file-name bookmarks (no ComicInfo.xml) showed the bookmarks sidebar on open");
  }
  console.log("  file-name bookmarks: sidebar hidden ✓");
}

if (import.meta.main) {
  await runStandalone(testit);
}
