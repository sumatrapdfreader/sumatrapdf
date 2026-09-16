// #6205: by default showing / hiding Bookmarks keeps the window where it is and
// its size; the canvas gives up the space. Growing the frame (#6080) is opt-in
// via SidebarWindowSize = grow.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { LayoutInfo } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Type /Outlines /Count 1 /First 5 0 R /Last 5 0 R >>",
    "<< /Title (Chapter) /Parent 4 0 R /Dest [3 0 R /XYZ null null null] >>",
  ]);
}

async function waitToc(
  client: Awaited<ReturnType<typeof launchControlled>>["client"],
  want: boolean,
): Promise<LayoutInfo> {
  const deadline = Date.now() + 8000;
  let last = "";
  while (Date.now() < deadline) {
    const layout = await client.layout();
    last = layout.raw;
    if (layout.items["toc"]?.visible === want) {
      return layout;
    }
    await sleep(50);
  }
  throw new Error(`issue-6205: toc visible=${want} never happened:\n${last}`);
}

function frameRect(layout: LayoutInfo) {
  const it = layout.items["frame"];
  if (!it) {
    throw new Error(`issue-6205: missing 'frame' in:\n${layout.raw}`);
  }
  return it.rect;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6205");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "a.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\nShowToc = false\nShowFavorites = false\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    // Fit Width has no unused canvas margin, so the grow path would resize
    sendCommandSync(frame, cmdId("CmdZoomFitWidth"));
    await client.waitForRenderIdle();
    const f0 = frameRect(await waitToc(client, false));

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const f1 = frameRect(await waitToc(client, true));
    if (f1.x !== f0.x || f1.dx !== f0.dx) {
      throw new Error(`issue-6205: showing toc moved/resized frame x=${f0.x} dx=${f0.dx} -> x=${f1.x} dx=${f1.dx}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const f2 = frameRect(await waitToc(client, false));
    if (f2.x !== f0.x || f2.dx !== f0.dx) {
      throw new Error(`issue-6205: hiding toc moved/resized frame x=${f0.x} dx=${f0.dx} -> x=${f2.x} dx=${f2.dx}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
