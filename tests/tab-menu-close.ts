// The window closed while a tab's context menu was open: TrackPopupMenu's nested
// loop ran the close and the deferred TabsCtrl delete, then the menu handler and
// BubbleMouse kept using the freed tab bar. Only an ASan build catches the
// freed read (CI: SUMATRA_TEST_EXE=out/dbg64_asan/SumatraPDF-static.exe).
//
// Run: bun tests/tab-menu-close.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { makePdf, runStandalone, tmpPath } from "./util.ts";
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
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
