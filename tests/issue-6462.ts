// Regression test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6462
//
// With tabs, Scrollbars = windows and a Home page long enough to scroll,
// closing the last tab left a black strip where Home's scrollbar belongs.
// Closing hides the document's bars, which widens the canvas without a
// WM_SIZE, so Home kept its layout for the narrower canvas: it neither showed
// its bar again nor painted the strip the bar had freed.
//
// The bar lives in the canvas's non-client area, so this reads the pixels the
// window shows (its window DC), not a PrintWindow, which would repaint it.
//
// Run:  bun tests/issue-6462.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import { ROOT, USE_NG, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  getSystemMetrics,
  getWindowLong,
  getWindowRect,
  readWindowDCColumn,
  sendCopyDataW,
  sendMessage,
  setProcessDpiAware,
  sleep,
  SM_CXVSCROLL,
  WM_COMMAND,
} from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const GWL_STYLE = -16;
const WS_VSCROLL = 0x00200000;
// SumatraPDF's WM_COPYDATA id for DDE-style commands
const kCopyDataDde = 0x44646557;

// the history only needs enough files that Home scrolls in a small window
function historyFiles(): string[] {
  const dir = join(ROOT, "tests");
  return readdirSync(dir)
    .filter((f) => f.endsWith(".pdf"))
    .map((f) => join(dir, f));
}

function settings(files: string[]): string {
  const states = files.map((p, i) => `\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${i + 1}\n\t]`).join("\n");
  return (
    "Theme = Light Warm\nScrollbars = windows\nUseTabs = true\nCheckForUpdates = false\n" +
    "RestoreSession = false\nWindowState = 1\nWindowPos = 100 100 850 450\n" +
    `FileStates [\n${states}\n]\n`
  );
}

type BarState = { shown: boolean; colors: number; black: number; n: number };

function barState(canvas: number): BarState {
  const shown = (getWindowLong(canvas, GWL_STYLE) & WS_VSCROLL) !== 0;
  const r = getWindowRect(canvas);
  const x = r.right - r.left - Math.floor(getSystemMetrics(SM_CXVSCROLL) / 2);
  const col = readWindowDCColumn(canvas, x, 0, r.bottom - r.top);
  return { shown, colors: new Set(col).size, black: col.filter((c) => c === 0).length, n: col.length };
}

const isPainted = (s: BarState) => s.shown && s.colors >= 2 && s.black < s.n / 4;
const describe = (s: BarState) => `shown=${s.shown} colors=${s.colors} black=${s.black}/${s.n}`;

async function waitPainted(canvas: number): Promise<BarState> {
  const deadline = Date.now() + 5000;
  let s = barState(canvas);
  while (!isPainted(s) && Date.now() < deadline) {
    await sleep(50);
    s = barState(canvas);
  }
  return s;
}

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP issue-6462: checks the orig app's native Win32 scrollbar");
    return;
  }
  setProcessDpiAware();
  const files = historyFiles();
  const dir = tmpPath("issue-6462-appdata");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings(files));

  const { proc, client, frame } = await launchControlled(["-appdata", dir], { defaultWindowPos: true });
  try {
    const before = await waitPainted(findCanvas(frame));
    if (!isPainted(before)) {
      throw new Error(`#6462: Home has no painted scrollbar at startup (${describe(before)})`);
    }

    sendCopyDataW(frame, kCopyDataDde, `[Open("${files[0]}", 0, 1, 0)]`);
    await client.waitForRenderIdle();
    sendMessage(frame, WM_COMMAND, BigInt(cmdId("CmdClose")), 0n);

    const after = await waitPainted(findCanvas(frame));
    if (!isPainted(after)) {
      throw new Error(`#6462: after closing the last tab Home's scrollbar is not painted (${describe(after)})`);
    }
    console.log(`issue-6462: OK ${describe(after)}`);
  } finally {
    client.close();
    await killAndWait(proc);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
