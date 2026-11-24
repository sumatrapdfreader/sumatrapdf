// #6588: with SidebarAutoHide, hiding the floating bookmarks with F12 left its
// 1px separator line (a child window of the frame) on screen. Hiding it by
// moving the mouse away didn't.
//
// Run: bun tests/issue-6588.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { findChildWindow, isWindowVisible, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, parkCursorAway, sendCommand } from "./win-automation.ts";

const EDGE_CLASS = "SumatraSidebarOverlayEdge";

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R /Outlines 4 0 R >>",
    "<< /Type /Pages /Count 1 /Kids [3 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Type /Outlines /Count 1 /First 5 0 R /Last 5 0 R >>",
    "<< /Title (Chapter) /Parent 4 0 R /Dest [3 0 R /XYZ null null null] >>",
  ]);
}

async function sidebarVisible(client: ControlClient): Promise<boolean> {
  const layout = await client.layout();
  return layout.items["sidebarTop"]?.visible ?? false;
}

// polls until cond() holds or the deadline passes
async function waitUntil(what: string, cond: () => Promise<boolean>): Promise<void> {
  const deadline = Date.now() + 5000;
  while (!(await cond())) {
    if (Date.now() > deadline) {
      throw new Error(`issue-6588: ${what}`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  if (USE_NG) {
    // ng draws the floating sidebar's edge itself, not with a child window
    console.log("SKIP issue-6588: orig only");
    return;
  }
  const dir = tmpPath("issue-6588");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  const pdf = join(dir, "a.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\n" +
      "ShowToc = true\nShowFavorites = false\nSidebarAutoHide = true\n",
  );

  parkCursorAway();
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    await waitUntil("the sidebar should start hidden", async () => !(await sidebarVisible(client)));

    // F12 off and on puts the focus in the bookmarks, which shows them
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await sleep(300);
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await waitUntil("F12 should show the sidebar", () => sidebarVisible(client));
    const edgeShown = () => {
      const edge = findChildWindow(frame, EDGE_CLASS);
      return Promise.resolve(edge !== 0 && isWindowVisible(edge));
    };
    await waitUntil("the shown sidebar should have its separator line", edgeShown);

    // F12 again hides the sidebar, separator line included
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await waitUntil("F12 should hide the sidebar", async () => !(await sidebarVisible(client)));
    await waitUntil("the separator line stayed after F12 hid the sidebar", async () => !(await edgeShown()));
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
