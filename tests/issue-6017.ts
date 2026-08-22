// #6017: Skip Permissions checkbox in the AI chat panel used a different
// background than the rest of the panel. The close-button size is a DPI
// issue covered by #5979 (and the header now uses the window's DPI).
//
// Run: bun tests/issue-6017.ts [--no-build]

import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, ROOT, cmdId, runStandalone, SLOW_BUILD_FACTOR } from "./util.ts";
import { FRAME_CLASS, sendCommandSync } from "./win-automation.ts";
import {
  captureWindowDCRegionPixels,
  enumChildWindows,
  getClientRect,
  getClassName,
  isWindowVisible,
  postMessage,
  sleep,
  waitForTopWindow,
  WM_KEYDOWN,
  VK_ESCAPE,
} from "./winapi.ts";

type Rect = { x: number; y: number; dx: number; dy: number };

function parseItem(raw: string, name: string): { visible: boolean; rect: Rect } | null {
  const m = new RegExp(`item name=${name} visible=(\\d+) rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`).exec(raw);
  if (!m) {
    return null;
  }
  return { visible: m[1] === "1", rect: { x: +m[2]!, y: +m[3]!, dx: +m[4]!, dy: +m[5]! } };
}

function parseLayoutKind(raw: string, kind: string): Rect | null {
  const m = new RegExp(
    `layout path=aiChat\\S* kind=${kind} visibility=\\d+ rect=(-?\\d+),(-?\\d+),(-?\\d+),(-?\\d+)`,
  ).exec(raw);
  if (!m) {
    return null;
  }
  return { x: +m[1]!, y: +m[2]!, dx: +m[3]!, dy: +m[4]! };
}

function pixelAt(data: Uint8Array, w: number, x: number, y: number): [number, number, number] {
  const i = (y * w + x) * 4;
  return [data[i]!, data[i + 1]!, data[i + 2]!];
}

function findPanelHwnd(frame: number, panel: Rect): number {
  let found = 0;
  enumChildWindows(frame, (hwnd) => {
    if (!isWindowVisible(hwnd) || getClassName(hwnd) !== "Static") {
      return true;
    }
    const cr = getClientRect(hwnd);
    const dx = cr.right - cr.left;
    const dy = cr.bottom - cr.top;
    if (Math.abs(dx - panel.dx) <= 2 && Math.abs(dy - panel.dy) <= 2) {
      found = hwnd;
      return false;
    }
    return true;
  });
  return found;
}

function colorDelta(a: [number, number, number], b: [number, number, number]): number {
  return Math.max(Math.abs(a[0] - b[0]), Math.abs(a[1] - b[1]), Math.abs(a[2] - b[2]));
}

export async function testit(): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  await withControlledSumatra(
    EXE,
    async (client: ControlClient, proc) => {
      const frame = await waitForTopWindow(proc.pid!, FRAME_CLASS);
      if (!frame) {
        throw new Error("no frame window");
      }
      await client.waitForRenderIdle();
      await client.setNotificationsEnabled(false);

      sendCommandSync(frame, cmdId("CmdAIChatWithClaudeCode"));
      await sleep(200 * SLOW_BUILD_FACTOR);
      postMessage(frame, WM_KEYDOWN, VK_ESCAPE, 0);

      const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
      let raw = "";
      let panel: { visible: boolean; rect: Rect } | null = null;
      while (Date.now() < deadline) {
        raw = String((await client.request(ControlCommand.TestLayout, ["get"]))[1] ?? "");
        panel = parseItem(raw, "aiChat");
        if (panel?.visible && panel.rect.dx > 40 && panel.rect.dy > 40) {
          break;
        }
        await sleep(80);
      }
      if (!panel?.visible || panel.rect.dx <= 40) {
        console.log("issue-6017: AI chat panel did not open (Claude/WebView2 missing); skipping checkbox check");
        return;
      }

      const checkbox = parseLayoutKind(raw, "checkbox");
      if (!checkbox || checkbox.dx < 20 || checkbox.dy < 10) {
        throw new Error(`issue-6017: no checkbox layout in AI chat panel\n${raw.slice(0, 800)}`);
      }
      console.log(`issue-6017: panel=${JSON.stringify(panel.rect)} checkbox=${JSON.stringify(checkbox)}`);

      const panelHwnd = findPanelHwnd(frame, panel.rect);
      if (!panelHwnd) {
        throw new Error("issue-6017: could not find AI chat panel HWND");
      }
      const cr = getClientRect(panelHwnd);
      const pw = cr.right - cr.left;
      const ph = cr.bottom - cr.top;
      const bits = captureWindowDCRegionPixels(panelHwnd, 0, 0, pw, ph);
      if (!bits) {
        throw new Error("issue-6017: could not capture AI chat panel");
      }
      // Sample above the label glyphs (right of the ~13px box) vs the same
      // row just left of the checkbox (panel chrome).
      const cx = checkbox.x + Math.min(checkbox.dx - 6, 40);
      const cy = checkbox.y + 2;
      // 8px spacer immediately left of the checkbox, not the combo
      const px = checkbox.x - 4;
      const py = checkbox.y + 2;
      if (cx < 0 || cy < 0 || cx >= pw || cy >= ph) {
        throw new Error(`issue-6017: sample out of panel ${pw}x${ph} checkbox@${cx},${cy}`);
      }
      const check = pixelAt(bits, pw, cx, cy);
      const panelBg = pixelAt(bits, pw, px, py);
      const delta = colorDelta(check, panelBg);
      console.log(`issue-6017: check=${check.join(",")} panel=${panelBg.join(",")} delta=${delta}`);
      if (delta > 16) {
        throw new Error(
          `issue-6017: checkbox background ${check.join(",")} @${cx},${cy} differs from panel ${panelBg.join(",")} @${px},${py} by ${delta}`,
        );
      }
    },
    [pdf],
  );
}

if (import.meta.main) {
  await runStandalone(testit);
}
