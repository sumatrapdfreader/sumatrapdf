// The window closed while a tab's context menu was open: TrackPopupMenu's nested
// loop ran the close and the deferred TabsCtrl delete, then the menu handler and
// BubbleMouse kept using the freed tab bar. Only an ASan build catches the
// freed read (CI: SUMATRA_TEST_EXE=out/dbg64_asan/SumatraPDF-static.exe).
// ng: the tab bar is drawn in the frame and its menu is a gpui popup; the
// window closing under that menu must still exit cleanly.
//
// Run: bun tests/tab-menu-close.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { makePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { clientToScreen, getClientRect, postMessage, sleep } from "./winapi.ts";
import {
  findChildByClass,
  killAndWait,
  launchControlled,
  openContextMenu,
  waitForContextMenu,
} from "./win-automation.ts";

const WM_CLOSE = 0x0010;
const kExitWaitMs = 10000;

async function closeAndWaitForExit(proc: Bun.Subprocess, frame: number): Promise<void> {
  postMessage(frame, WM_CLOSE, 0, 0);
  const deadline = Date.now() + kExitWaitMs;
  while (proc.exitCode === null && Date.now() < deadline) {
    await sleep(50);
  }
  if (proc.exitCode === null) {
    throw new Error("tab-menu-close: app did not exit");
  }
  if (proc.exitCode !== 0) {
    throw new Error(`tab-menu-close: app exited with ${proc.exitCode}`);
  }
  console.log("tab-menu-close: OK");
}

// right-clicks the first document tab, after Home
async function openNgTabMenu(client: ControlClient): Promise<void> {
  const raw = String((await client.request(ControlCommand.TestTabWidths, []))[1] ?? "");
  const tabs = [...raw.matchAll(/^dx=(\d+) .*\n {2}at x=(-?\d+) y=(-?\d+) dy=(\d+)$/gm)].map((m) => ({
    x: +m[2]! + Math.floor(+m[1]! / 2),
    y: +m[3]! + Math.floor(+m[4]! / 2),
  }));
  if (tabs.length < 2) {
    throw new Error(`tab-menu-close: no document tab:\n${raw}`);
  }
  await client.request(ControlCommand.TestInput, ["click", tabs[1]!.x, tabs[1]!.y, 1, 0]);
  const deadline = Date.now() + 3000;
  while (!/ popup=1/.test(String((await client.request(ControlCommand.TestUiState, []))[1] ?? ""))) {
    if (Date.now() > deadline) {
      throw new Error("tab-menu-close: tab menu did not open");
    }
    await sleep(40);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("tab-menu-close");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdfs = [join(dir, "a.pdf"), join(dir, "b.pdf")];
  for (const p of pdfs) {
    writeFileSync(p, makePdf(2, 601, 0, 0), "latin1");
  }

  const { proc, client, frame } = await launchControlled(pdfs);
  try {
    await client.waitForRenderIdle();
    if (USE_NG) {
      await openNgTabMenu(client);
      await closeAndWaitForExit(proc, frame);
      return;
    }
    const tabs = findChildByClass(frame, "SumatraTabsCtrlClass");
    if (!tabs) {
      throw new Error("tab-menu-close: no tab bar");
    }
    // a document tab: the home tab has no menu, so probe along the bar
    const cr = getClientRect(tabs);
    let menu = 0;
    for (let x = 40; x < cr.right && !menu; x += 80) {
      const pt = clientToScreen(tabs, x, Math.floor(cr.bottom / 2));
      openContextMenu(tabs, pt.x, pt.y);
      menu = await waitForContextMenu(500);
    }
    if (!menu) {
      throw new Error("tab-menu-close: tab menu did not open");
    }

    // the menu loop dispatches the close
    await closeAndWaitForExit(proc, frame);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
