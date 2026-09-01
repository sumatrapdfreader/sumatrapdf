// #6080: showing Bookmarks grows the frame only as much as unused canvas
// margin does not cover. Fit Width has no unused dx, so the frame grows by
// the toc. Hide shrinks by the amount we grew. Tab switch does not resize.

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

    // Fit Width: page uses the full canvas dx, so the frame must grow by ~toc.
    sendCommandSync(frame, cmdId("CmdZoomFitWidth"));
    await client.waitForRenderIdle();
    const closedFw = await waitToc(client, false);
    const canvasFw0 = item(closedFw, "canvas").rect;
    const frameFw0 = item(closedFw, "frame").rect;

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const openFw = await waitToc(client, true);
    await client.waitForRenderIdle();
    const canvasFw1 = item(openFw, "canvas").rect;
    const frameFw1 = item(openFw, "frame").rect;
    const tocFw = item(openFw, "toc").rect;

    if (Math.abs(canvasFw1.dx - canvasFw0.dx) > 2) {
      throw new Error(
        `issue-6080: fit-width canvas dx should stay ${canvasFw0.dx}, got ${canvasFw1.dx} (frame ${frameFw0.dx} -> ${frameFw1.dx})`,
      );
    }
    if (frameFw1.dx < frameFw0.dx + tocFw.dx - 8) {
      throw new Error(`issue-6080: fit-width frame should grow by ~toc ${tocFw.dx}, ${frameFw0.dx} -> ${frameFw1.dx}`);
    }
    const wa = getWorkArea();
    if (frameFw1.x < wa.left - 2 || frameFw1.x + frameFw1.dx > wa.right + 2) {
      throw new Error(
        `issue-6080: frame not fully in work area: x=${frameFw1.x} dx=${frameFw1.dx} work=${wa.left}..${wa.right}`,
      );
    }

    const openPath = pdfB.replaceAll("\\", "/");
    if (!sendCopyDataW(frame, kCopyDataDdeW, `[Open("${openPath}")]`)) {
      throw new Error("issue-6080: DDE Open of second tab failed");
    }
    await client.waitForRenderIdle();
    const other = await waitToc(client, false);
    const frameTab = item(other, "frame").rect;
    if (Math.abs(frameTab.dx - frameFw1.dx) > 2) {
      throw new Error(`issue-6080: tab switch changed frame dx ${frameFw1.dx} -> ${frameTab.dx}`);
    }

    sendCommandSync(frame, cmdId("CmdPrevTab"));
    await client.waitForRenderIdle();
    const back = await waitToc(client, true);
    const frameBack = item(back, "frame").rect;
    if (Math.abs(frameBack.dx - frameFw1.dx) > 2) {
      throw new Error(`issue-6080: return tab changed frame dx ${frameFw1.dx} -> ${frameBack.dx}`);
    }

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const closedFwAgain = await waitToc(client, false);
    await client.waitForRenderIdle();
    const canvasFw2 = item(closedFwAgain, "canvas").rect;
    const frameFw2 = item(closedFwAgain, "frame").rect;
    if (Math.abs(canvasFw2.dx - canvasFw0.dx) > 2) {
      throw new Error(`issue-6080: fit-width canvas dx after hide ${canvasFw2.dx}, want ${canvasFw0.dx}`);
    }
    if (Math.abs(frameFw2.dx - frameFw0.dx) > 2) {
      throw new Error(`issue-6080: fit-width frame dx after hide ${frameFw2.dx}, want ${frameFw0.dx}`);
    }

    // 25%: plenty of unused canvas dx, so steal that instead of growing by toc.
    sendCommandSync(frame, cmdId("CmdZoom25"));
    await client.waitForRenderIdle();
    const closedSm = await waitToc(client, false);
    const canvasSm0 = item(closedSm, "canvas").rect;
    const frameSm0 = item(closedSm, "frame").rect;

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const openSm = await waitToc(client, true);
    await client.waitForRenderIdle();
    const canvasSm1 = item(openSm, "canvas").rect;
    const frameSm1 = item(openSm, "frame").rect;
    const tocSm = item(openSm, "toc").rect;

    const frameGrew = frameSm1.dx - frameSm0.dx;
    const canvasLost = canvasSm0.dx - canvasSm1.dx;
    if (canvasLost < tocSm.dx - 16) {
      throw new Error(
        `issue-6080: 25% should steal canvas margin (~toc ${tocSm.dx}), canvas ${canvasSm0.dx} -> ${canvasSm1.dx} frame grew ${frameGrew}`,
      );
    }
    if (frameGrew > 8) {
      throw new Error(
        `issue-6080: 25% should not grow the frame when margin covers toc ${tocSm.dx}, ${frameSm0.dx} -> ${frameSm1.dx}`,
      );
    }

    sendCommandSync(frame, cmdId("CmdToggleBookmarks"));
    const closedSmAgain = await waitToc(client, false);
    await client.waitForRenderIdle();
    const canvasSm2 = item(closedSmAgain, "canvas").rect;
    const frameSm2 = item(closedSmAgain, "frame").rect;
    if (Math.abs(canvasSm2.dx - canvasSm0.dx) > 2) {
      throw new Error(`issue-6080: 25% canvas dx after hide ${canvasSm2.dx}, want ${canvasSm0.dx}`);
    }
    if (Math.abs(frameSm2.dx - frameSm0.dx) > 2) {
      throw new Error(`issue-6080: 25% frame dx after hide ${frameSm2.dx}, want ${frameSm0.dx}`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
