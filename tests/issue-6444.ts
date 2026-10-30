// DisableLeftDragPan (issue #6444): with it, a left drag where there is no
// text doesn't pan the page; without it, the same drag does.
//
// Run: bun tests/issue-6444.ts [--no-build] [-ng]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { type ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { assemblePdf, EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";
import { ensureModifierKeysUp, findCanvas, waitForFrame } from "./win-automation.ts";
import {
  getClientRect,
  MK_LBUTTON,
  packCoords,
  sendMessage,
  sleep,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";

const WINDOW_POS = ["-window-pos", "900x700@40x40"];
const kDragDy = 200;
const SETTINGS_HEAD = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
RememberStatePerDocument = false
ShowToc = false
ShowFavorites = false
DefaultDisplayMode = continuous
DefaultZoom = fit width
`;

// three blank pages: nothing to select, so a plain drag pans by default
function makePdf(): string {
  const page = "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>",
    page,
    page,
    page,
  ]);
}

async function scrollY(client: ControlClient): Promise<number> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestScrollState, []);
    const raw = String(res[1] ?? "");
    const m = /^OK page=(\d+) x=[-\d.e]+ y=([-\d.e]+)/.exec(raw);
    if (res[0] === 0 && m) {
      // page units across pages: a page is 792 tall
      return (+m[1]! - 1) * 792 + +m[2]!;
    }
    if (res[0] !== 2 || Date.now() > deadline) {
      throw new Error(`issue-6444: TestScrollState failed: ${raw}`);
    }
    await sleep(100);
  }
}

// left drag from the canvas center up by kDragDy pixels
async function dragUp(pid: number): Promise<void> {
  const frame = await waitForFrame(pid);
  const canvas = frame ? findCanvas(frame) : 0;
  if (!canvas) {
    throw new Error("issue-6444: no canvas");
  }
  await ensureModifierKeysUp();
  const rc = getClientRect(canvas);
  const x = Math.floor((rc.right - rc.left) / 2);
  const y = Math.floor((rc.bottom - rc.top) / 2) + kDragDy / 2;
  sendMessage(canvas, WM_LBUTTONDOWN, MK_LBUTTON, packCoords(x, y));
  for (let i = 1; i <= 10; i++) {
    sendMessage(canvas, WM_MOUSEMOVE, MK_LBUTTON, packCoords(x, y - (i * kDragDy) / 10));
  }
  sendMessage(canvas, WM_LBUTTONUP, 0, packCoords(x, y - kDragDy));
}

// how far a left drag scrolled, with <settings> appended to the settings file
async function dragScroll(name: string, settings: string, pdf: string): Promise<number> {
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), SETTINGS_HEAD + settings);
  let moved = 0;
  await withControlledSumatra(
    EXE,
    async (client, proc) => {
      await client.setNotificationsEnabled(false);
      await client.waitForRenderIdle();
      const before = await scrollY(client);
      await dragUp(proc.pid!);
      await sleep(300 * SLOW_BUILD_FACTOR);
      moved = (await scrollY(client)) - before;
    },
    [...WINDOW_POS, "-appdata", dir, pdf],
  );
  return moved;
}

export async function testit(): Promise<void> {
  const pdf = tmpPath("issue-6444.pdf");
  writeFileSync(pdf, makePdf(), "latin1");

  const panned = await dragScroll("issue-6444-default", "", pdf);
  if (panned <= 0) {
    throw new Error(`issue-6444: by default a left drag should pan, it scrolled ${panned}`);
  }
  const moved = await dragScroll("issue-6444-disabled", "DisableLeftDragPan = true\n", pdf);
  if (moved !== 0) {
    throw new Error(`issue-6444: with DisableLeftDragPan a left drag scrolled ${moved}`);
  }
  console.log(`issue-6444: OK (default drag scrolled ${panned})`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
