// #6120: main toolbar buttons on RTL UI did not respond to clicks.
// With tabs in the title bar, an empty part of the toolbar starts a caption
// drag. OnCaptionDrag hit-tested the raw (mirrored) WM_LBUTTONDOWN coords
// against the unmirrored layout bounds, so clicks on the main row — packed
// to the right, with empty space on the left — were stolen as window drags.
// Annotation buttons sit in the center, where mirrored and physical x nearly
// match, so they still worked.
//
// A real click on an RTL hwnd arrives with x measured from the visual right.
// The dump rects are layout coords (physical left), so the test mirrors x
// the way Windows does before posting.
//
// Run: bun tests/issue-6120.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { clickAt, findChildByClass, killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";
import { getClientRect, getWindowLong, GWL_EXSTYLE } from "./winapi.ts";

const TOOLBAR_CLASS = "SUMATRA_VIRT_TOOLBAR";
const WS_EX_LAYOUTRTL = 0x00400000;

const SETTINGS = `UiLanguage = en
UseTabs = true
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

function makePdf(): string {
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Count 2 /Kids [3 0 R 4 0 R] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>",
  ]);
}

function parseBtn(dump: string, cmd: number): { x: number; y: number; dx: number; dy: number } {
  const re = new RegExp(`^idx=\\d+ cmd=${cmd} hidden=0 rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`, "m");
  const m = re.exec(dump);
  if (!m) {
    throw new Error(`issue-6120: cmd ${cmd} not on the toolbar\n${dump}`);
  }
  const x = +m[1]!;
  const y = +m[2]!;
  return { x, y, dx: +m[3]! - x, dy: +m[4]! - y };
}

async function toolbarDump(client: ControlClient): Promise<string> {
  const res = await client.request(ControlCommand.TestToolbarButtons, []);
  if (res[0] !== 0) {
    throw new Error(`issue-6120: TestToolbarButtons failed: ${String(res[1] ?? "").trim()}`);
  }
  return String(res[1] ?? "");
}

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
  const m = /OK page=(\d+)/.exec(String(res[1] ?? ""));
  if (!m) {
    throw new Error(`issue-6120: could not read current page: ${String(res[1] ?? "").trim()}`);
  }
  return +m[1]!;
}

// Windows delivers mouse x from the visual right on WS_EX_LAYOUTRTL.
function rtlClientX(hwnd: number, layoutX: number): number {
  const rc = getClientRect(hwnd);
  return rc.right - rc.left - 1 - layoutX;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6120");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS);
  writeFileSync(join(dir, "doc.pdf"), makePdf(), "latin1");

  const { proc, client, frame } = await launchControlled([
    "-appdata",
    dir,
    "-window-pos",
    "1100x900@40x40",
    join(dir, "doc.pdf"),
  ]);
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);

    sendCommandSync(frame, cmdId("CmdDebugToggleRtl"));
    await client.waitForRenderIdle();

    const toolbar = findChildByClass(frame, TOOLBAR_CLASS);
    if (!toolbar) {
      throw new Error("issue-6120: no toolbar window");
    }
    if ((getWindowLong(toolbar, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) === 0) {
      throw new Error("issue-6120: toolbar hwnd is not RTL after CmdDebugToggleRtl");
    }

    const dump = await toolbarDump(client);
    const btn = parseBtn(dump, cmdId("CmdGoToNextPage"));
    const x = rtlClientX(toolbar, btn.x + Math.floor(btn.dx / 2));
    const y = btn.y + Math.floor(btn.dy / 2);
    await clickAt(toolbar, x, y, 400);

    const page = await currentPage(client);
    if (page !== 2) {
      throw new Error(`issue-6120: Next Page click did nothing in RTL (page=${page}, want 2)`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
