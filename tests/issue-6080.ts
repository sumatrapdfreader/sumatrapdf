// #6080: showing Bookmarks should grow the frame when the work area has room,
// so the canvas (and the page) keep their size. Hide shrinks the frame again.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { getWorkArea, sendCopyDataW, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

const kCopyDataDdeW = 0x44646557;
import type { LayoutInfo } from "./control.ts";

function makePdf(): string {
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Type /Outlines /Count 1 /First 5 0 R /Last 5 0 R >>",
    "<< /Title (Chapter) /Parent 4 0 R /Dest [3 0 R /XYZ null null null] >>",
  ];
  return assemblePdf(objs);
}

function item(layout: LayoutInfo, name: string) {
  const it = layout.items[name];
  if (!it) {
    throw new Error(`issue-6080: missing '${name}' in:\n${layout.raw}`);
  }
  return it;
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
    if (item(layout, "toc").visible === want) {
      return layout;
    }
    await sleep(50);
  }
  throw new Error(`issue-6080: toc visible=${want} never happened:\n${last}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6080");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfA = join(dir, "a.pdf");
  const pdfB = join(dir, "b.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdfA, makePdf(), "latin1");
  writeFileSync(pdfB, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\nShowToc = false\nShowFavorites = false\nUseTabs = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdfA]);
  try {
    await client.waitForRenderIdle();
    const closed = await waitToc(client, false);
    const canvas0 = item(closed, "canvas").rect;
    const frame0 = item(closed, "frame").rect;

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const open = await waitToc(client, true);
    await client.waitForRenderIdle();
    const canvas1 = item(open, "canvas").rect;
    const frame1 = item(open, "frame").rect;
    const toc1 = item(open, "toc").rect;

    if (Math.abs(canvas1.dx - canvas0.dx) > 2) {
      throw new Error(
        `issue-6080: canvas dx should stay ${canvas0.dx} when showing bookmarks, got ${canvas1.dx} (frame ${frame0.dx} -> ${frame1.dx})`,
      );
    }
    if (frame1.dx < frame0.dx + toc1.dx - 8) {
      throw new Error(`issue-6080: frame should grow by ~toc width ${toc1.dx}, ${frame0.dx} -> ${frame1.dx}`);
    }
    const wa = getWorkArea();
    if (frame1.x < wa.left - 2 || frame1.x + frame1.dx > wa.right + 2) {
      throw new Error(
        `issue-6080: frame not fully in work area: x=${frame1.x} dx=${frame1.dx} work=${wa.left}..${wa.right}`,
      );
    }

    const openPath = pdfB.replaceAll("\\", "/");
    if (!sendCopyDataW(frame, kCopyDataDdeW, `[Open("${openPath}")]`)) {
      throw new Error("issue-6080: DDE Open of second tab failed");
    }
    await client.waitForRenderIdle();
    const other = await waitToc(client, false);
    const frameTab = item(other, "frame").rect;
    if (Math.abs(frameTab.dx - frame1.dx) > 2) {
      throw new Error(`issue-6080: tab switch changed frame dx ${frame1.dx} -> ${frameTab.dx}`);
    }

    sendCommandSync(frame, cmdId("CmdPrevTab"));
    await client.waitForRenderIdle();
    const back = await waitToc(client, true);
    const frameBack = item(back, "frame").rect;
    if (Math.abs(frameBack.dx - frame1.dx) > 2) {
      throw new Error(`issue-6080: return tab changed frame dx ${frame1.dx} -> ${frameBack.dx}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const closedAgain = await waitToc(client, false);
    const canvas2 = item(closedAgain, "canvas").rect;
    const frame2 = item(closedAgain, "frame").rect;
    if (Math.abs(canvas2.dx - canvas0.dx) > 2) {
      throw new Error(`issue-6080: canvas dx after hide ${canvas2.dx}, want ${canvas0.dx}`);
    }
    if (Math.abs(frame2.dx - frame0.dx) > 2) {
      throw new Error(`issue-6080: frame dx after hide ${frame2.dx}, want ${frame0.dx}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
