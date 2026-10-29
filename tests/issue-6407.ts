// #6407: ShowThumbnailsIfNoToc. A document without bookmarks opens with page
// thumbnails in the sidebar; one with bookmarks still opens with them; a
// single page or the setting off means no sidebar, as before.
//
// Run: bun tests/issue-6407.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { assemblePdf, EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(pages: number, withToc: boolean): string {
  const kids = Array.from({ length: pages }, (_, i) => `${5 + i} 0 R`).join(" ");
  const objs = [
    `<< /Type /Catalog /Pages 2 0 R${withToc ? " /Outlines 3 0 R" : ""} >>`,
    `<< /Type /Pages /Count ${pages} /Kids [${kids}] >>`,
    "<< /Type /Outlines /First 4 0 R /Last 4 0 R /Count 1 >>",
    "<< /Title (Chapter) /Parent 3 0 R /Dest [5 0 R /Fit] >>",
  ];
  for (let i = 0; i < pages; i++) {
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  }
  return assemblePdf(objs);
}

type Sidebar = { topVis: boolean; topView: string };

async function sidebarOnOpen(name: string, pdf: string, setting: boolean): Promise<Sidebar> {
  const appdata = tmpPath(`issue-6407-${name}`);
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  const settings = [
    "UiLanguage = en",
    "CheckForUpdates = false",
    "RestoreSession = false",
    `ShowThumbnailsIfNoToc = ${setting}`,
    "",
  ].join("\n");
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), settings);
  let got: Sidebar = { topVis: false, topView: "" };
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestSidebarLayout, []);
      const raw = String(res[1] ?? "").trim();
      const m = /OK .*topVis=(\d).*topView=(\S+)/.exec(raw);
      if (res[0] !== 0 || !m) {
        throw new Error(`issue-6407: TestSidebarLayout: ${raw}`);
      }
      got = { topVis: m[1] === "1", topView: m[2]! };
    },
    ["-appdata", appdata, pdf],
  );
  return got;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6407");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const noToc = join(dir, "no-toc.pdf");
  const withToc = join(dir, "with-toc.pdf");
  const onePage = join(dir, "one-page.pdf");
  writeFileSync(noToc, makePdf(3, false), "latin1");
  writeFileSync(withToc, makePdf(3, true), "latin1");
  writeFileSync(onePage, makePdf(1, false), "latin1");

  const fail = (what: string, s: Sidebar) => {
    throw new Error(`issue-6407: ${what}; got topVis=${s.topVis} topView=${s.topView}`);
  };
  let s = await sidebarOnOpen("no-toc", noToc, true);
  if (!s.topVis || s.topView !== "thumbnails") {
    fail("a document without bookmarks should open with thumbnails", s);
  }
  s = await sidebarOnOpen("with-toc", withToc, true);
  if (!s.topVis || s.topView !== "bookmarks") {
    fail("a document with bookmarks should open with bookmarks", s);
  }
  s = await sidebarOnOpen("one-page", onePage, true);
  if (s.topVis) {
    fail("a one-page document should open without the sidebar", s);
  }
  s = await sidebarOnOpen("off", noToc, false);
  if (s.topVis) {
    fail("without the setting a document without bookmarks should open without the sidebar", s);
  }
  console.log("issue-6407: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
