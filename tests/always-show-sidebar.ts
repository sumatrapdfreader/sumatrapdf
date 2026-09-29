// AlwaysShowSidebar: a document with bookmarks opens with the Bookmarks
// sidebar even when it was last closed with the sidebar hidden.
//
// Run: bun tests/always-show-sidebar.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { assemblePdf, EXE, runStandalone, tmpPath } from "./util.ts";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Type /Outlines /First 5 0 R /Last 5 0 R /Count 1 >>",
    "<< /Title (Chapter) /Parent 4 0 R /Dest [3 0 R /Fit] >>",
  ]);
}

async function tocVisibleOnOpen(pdf: string, alwaysShow: boolean): Promise<boolean> {
  const appdata = tmpPath(`always-show-sidebar-${alwaysShow}`);
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  // the file was last closed with the sidebar hidden
  const settings = [
    "UiLanguage = en",
    "CheckForUpdates = false",
    "RestoreSession = false",
    `AlwaysShowSidebar = ${alwaysShow}`,
    "FileStates [",
    "\t[",
    `\t\tFilePath = ${pdf}`,
    "\t\tShowToc = false",
    "\t]",
    "]",
    "",
  ].join("\n");
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), settings);
  let vis = false;
  await withControlledSumatra(
    EXE,
    async (client) => {
      await client.waitForRenderIdle();
      const res = await client.request(ControlCommand.TestSidebarLayout, []);
      const raw = String(res[1] ?? "").trim();
      const m = /OK .*tocVis=(\d)/.exec(raw);
      if (res[0] !== 0 || !m) {
        throw new Error(`always-show-sidebar: TestSidebarLayout: ${raw}`);
      }
      vis = m[1] === "1";
    },
    ["-appdata", appdata, pdf],
  );
  return vis;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("always-show-sidebar");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "bookmarks.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  if (await tocVisibleOnOpen(pdf, false)) {
    throw new Error("always-show-sidebar: without the setting the remembered hidden sidebar showed");
  }
  if (!(await tocVisibleOnOpen(pdf, true))) {
    throw new Error("always-show-sidebar: AlwaysShowSidebar = true didn't show the sidebar");
  }
  console.log("always-show-sidebar: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
