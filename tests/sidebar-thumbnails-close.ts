// Closing the window after a sidebar panel showed the thumbnails and then
// another view must not touch freed memory: that panel is deleted first.
// Only an ASan build (SUMATRA_TEST_EXE=out/dbg64_asan/SumatraPDF-static.exe)
// sees the use-after-free; it exits non-zero then.
//
// Run: bun tests/sidebar-thumbnails-close.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { cmdId, makePdf, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

// what the panels show, e.g. "thumbnails/favorites" ("-" for a hidden panel)
async function panels(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestSidebarThumbnails, []);
  const raw = String(res[1] ?? "");
  const m = / top=\d+,(\d),(\w+),.* bottom=\d+,(\d),(\w+),/.exec(raw);
  if (!m) {
    throw new Error(`sidebar-thumbnails-close: TestSidebarThumbnails: ${raw}`);
  }
  return `${m[1] === "1" ? m[2] : "-"}/${m[3] === "1" ? m[4] : "-"}`;
}

async function waitFor(what: string, f: () => Promise<boolean>) {
  const deadline = Date.now() + 5000;
  while (!(await f())) {
    if (Date.now() > deadline) {
      throw new Error(`sidebar-thumbnails-close: ${what}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("sidebar-thumbnails-close");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(4, 601, 3, 0), "latin1");

  const { proc, client, frame } = await launchControlled([pdf]);
  let quit = false;
  try {
    await client.waitForRenderIdle();
    const want = (layout: string) => waitFor(`want ${layout}`, async () => (await panels(client)) === layout);

    // the top panel shows the thumbnails, then switches to the bookmarks
    await want("bookmarks/-");
    sendCommand(frame, cmdId("CmdFavoriteToggle"));
    await want("bookmarks/favorites");
    sendCommand(frame, cmdId("CmdToggleThumbnails"));
    await want("thumbnails/favorites");
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await want("bookmarks/favorites");

    await client.quit();
    quit = true;
    const code = await proc.exited;
    if (code !== 0) {
      throw new Error(`sidebar-thumbnails-close: closing exited with code ${code}`);
    }
    console.log("sidebar-thumbnails-close: OK");
  } finally {
    client.close();
    if (!quit) {
      await killAndWait(proc);
    }
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
