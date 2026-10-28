// #5819: SidebarAutoHide. The sidebar takes no space: the canvas spans the
// window, and the sidebar slides over it when the mouse reaches the canvas's
// left edge, then hides when the mouse leaves it. Moving the focus into it
// (showing the bookmarks with F12) shows it until the focus leaves.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient, LayoutInfo, LayoutRect } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { clientToScreen, getCursorPos, postMessage, setCursorPos, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled, ngInput, parkCursorAway, sendCommand } from "./win-automation.ts";

const sidebarDx = 250;
const WM_LBUTTONDOWN = 0x0201;
const WM_LBUTTONUP = 0x0202;
const MK_LBUTTON = 0x0001;

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Type /Outlines /Count 1 /First 5 0 R /Last 5 0 R >>",
    "<< /Title (Chapter) /Parent 4 0 R /Dest [3 0 R /XYZ null null null] >>",
  ]);
}

function item(layout: LayoutInfo, name: string): { visible: boolean; rect: LayoutRect } {
  const it = layout.items[name];
  if (!it) {
    throw new Error(`issue-5819: no '${name}' in:\n${layout.raw}`);
  }
  return it;
}

async function waitSidebar(client: ControlClient, visible: boolean, what: string): Promise<LayoutInfo> {
  const deadline = Date.now() + 5000;
  for (;;) {
    const layout = await client.layout();
    if (item(layout, "sidebarTop").visible === visible) {
      return layout;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5819: ${what}:\n${layout.raw}`);
    }
    await sleep(50);
  }
}

// orig polls GetCursorPos, so the cursor must really be there; ng takes the
// move through TestInput
async function moveCursor(client: ControlClient, frame: number, x: number, y: number): Promise<void> {
  if (USE_NG) {
    await ngInput(client, ["move", x, y, 0, 0]);
    return;
  }
  const p = clientToScreen(frame, x, y);
  const c = setCursorPos(p.x, p.y) ? getCursorPos() : null;
  if (!c || c.x !== p.x || c.y !== p.y) {
    throw new Error(`issue-5819: could not move the cursor to ${p.x},${p.y}`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5819");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "a.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\n" +
      `ShowToc = true\nShowFavorites = false\nSidebarAutoHide = true\nSidebarDx = ${sidebarDx}\n`,
  );

  parkCursorAway();
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    // hidden, and the canvas has the sidebar's column too
    let layout = await waitSidebar(client, false, "the sidebar should start hidden");
    const canvas = item(layout, "canvas").rect;
    const frameRc = item(layout, "frame").rect;
    if (canvas.dx < frameRc.dx - 40) {
      throw new Error(`issue-5819: canvas ${canvas.dx} wide should span the frame ${frameRc.dx}:\n${layout.raw}`);
    }

    // the mouse at the left edge shows it over the canvas, which keeps its size
    const midY = canvas.y + Math.floor(canvas.dy / 2);
    await moveCursor(client, frame, canvas.x + 2, midY);
    layout = await waitSidebar(client, true, "the mouse at the left edge should show the sidebar");
    const toc = item(layout, "sidebarTop").rect;
    const canvas2 = item(layout, "canvas").rect;
    if (toc.x !== canvas.x || toc.y !== canvas.y || toc.dy !== canvas.dy || canvas2.dx !== canvas.dx) {
      throw new Error(
        `issue-5819: sidebar ${JSON.stringify(toc)} should float over the canvas ${JSON.stringify(canvas2)}:\n${layout.raw}`,
      );
    }

    // still shown over it, hidden once the mouse leaves it
    await moveCursor(client, frame, toc.x + Math.floor(toc.dx / 2), midY);
    await sleep(300);
    await waitSidebar(client, true, "the sidebar should stay while the mouse is over it");
    await moveCursor(client, frame, toc.x + toc.dx + 100, midY);
    await waitSidebar(client, false, "the sidebar should hide when the mouse leaves it");

    // F12 off and on puts the focus in the bookmarks, which shows them
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await sleep(300);
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await waitSidebar(client, true, "showing the bookmarks should show the sidebar");

    // a click in the document takes the focus out and hides it
    if (USE_NG) {
      await ngInput(client, ["click", toc.x + toc.dx + 100, midY, 0, 0]);
    } else {
      const canvasHwnd = findCanvas(frame);
      const lp = (100 << 16) | (toc.dx + 100);
      postMessage(canvasHwnd, WM_LBUTTONDOWN, MK_LBUTTON, lp);
      postMessage(canvasHwnd, WM_LBUTTONUP, 0, lp);
    }
    await waitSidebar(client, false, "a click in the document should hide the sidebar");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
