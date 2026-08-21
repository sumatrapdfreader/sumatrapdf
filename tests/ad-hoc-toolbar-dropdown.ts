// The Read Aloud toolbar button has a drop-down arrow that opens a menu with
// TrackPopupMenu. That menu is modal, and the click that dismisses it is
// delivered to the toolbar afterwards - so a click on the arrow that closes the
// menu would be seen as a fresh click and open it right back up.
//
// The toolbar guards against that by ignoring a click on the button within
// 200ms of its menu closing (gToolbarDropdownClosedAt in Toolbar.cpp). This
// test drives that guard: open the menu, dismiss it, click the arrow again
// straight away, and check the menu does not come back.
//
// Not in tests/run-almost-all.ts: it drives a modal menu through posted messages, which is
// more environment-sensitive than the rest of the suite.
//
// Uses -appdata so it never touches the user's real settings.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control";
import { cmdId, EXE, ROOT, runStandalone, tmpPath } from "./util";
import { findChildByClass } from "./win-automation";
import { enumWindows, getClassName, postMessage, WM_KEYDOWN, WM_LBUTTONDOWN, WM_LBUTTONUP } from "./winapi";

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
const VK_ESCAPE = 0x1b;
const MENU_CLASS = "#32768";
const FRAME_CLASS = "SUMATRA_PDF_FRAME";
const TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
ToolbarShowReadAloud = true
`;

// menus are top-level windows of the well-known menu class
function menuCount(): number {
  let n = 0;
  enumWindows((h) => {
    if (getClassName(h) === MENU_CLASS) {
      n++;
    }
    return true;
  });
  return n;
}

function findFrame(): number {
  let f = 0;
  enumWindows((h) => {
    if (getClassName(h) === FRAME_CLASS) {
      f = h;
      return false;
    }
    return true;
  });
  return f;
}

// the visible Read Aloud button's rect, in toolbar client coords
function readAloudRect(dump: string): [number, number, number, number] {
  const wanted = cmdId("CmdReadAloud");
  for (const line of dump.split("\n")) {
    const m = /^idx=\d+ cmd=(\d+) hidden=(\d) rect=(\d+),(\d+),(\d+),(\d+)/.exec(line);
    if (m && Number(m[1]) === wanted && m[2] === "0") {
      return [Number(m[3]), Number(m[4]), Number(m[5]), Number(m[6])];
    }
  }
  throw new Error(`no visible Read Aloud button (cmd=${wanted}) in toolbar dump:\n${dump}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-toolbar-dropdown");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  await withControlledSumatra(
    EXE,
    async (client) => {
      const res = await client.request(ControlCommand.TestToolbarButtons, []);
      const [, y0, x1, y1] = readAloudRect(String(res[1] ?? ""));

      const frame = findFrame();
      const toolbar = findChildByClass(frame, TOOLBAR_CLASS);
      if (!toolbar) {
        throw new Error("no toolbar window");
      }

      // the drop-down arrow sits at the button's right edge
      const lp = (Math.round((y0 + y1) / 2) << 16) | ((x1 - 4) & 0xffff);
      const clickArrow = () => {
        postMessage(toolbar, WM_LBUTTONDOWN, 1, lp);
        postMessage(toolbar, WM_LBUTTONUP, 0, lp);
      };

      clickArrow();
      await sleep(700);
      if (menuCount() === 0) {
        throw new Error("clicking the drop-down arrow did not open the menu");
      }

      // Dismiss and click again straight away, which is what happens when
      // the dismissing click lands on the arrow. The guard only covers
      // 200ms, so this must not wait around.
      postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);
      await sleep(30);
      clickArrow();
      await sleep(600);
      if (menuCount() > 0) {
        throw new Error("the menu re-opened: the just-closed guard did not hold");
      }
      console.log("  drop-down menu opens, and a click right after dismissing it doesn't re-open it ✓");
      return [];
    },
    ["-appdata", dir, pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
