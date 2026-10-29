// #6429: a zoomed-in citation hover popup scrolls sideways.
//  - Ctrl + wheel over the popup zooms around the cursor (the popup's center
//    here), so the region's left edge moves right
//  - over the popup a horizontal wheel and Shift + wheel scroll sideways:
//    right / Shift + wheel down go right, Shift + wheel up goes left

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control";
import { runStandalone, tmpPath, USE_NG } from "./util";
import { clientToScreen, getClientRect, getWindowRect } from "./winapi";
import { findCanvas, killAndWait, launchControlled } from "./win-automation";
import {
  makePdf,
  MK_CONTROL,
  MK_SHIFT,
  waitFor,
  wheel,
  WHEEL_DELTA,
  WM_MOUSEHWHEEL,
  WM_MOUSEWHEEL,
} from "./issue-6252";

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6429");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "doc.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    ["UiLanguage = en", "CheckForUpdates = false", "RestoreSession = false", "CitationHoverDelay = 100", ""].join("\n"),
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await client.setNotificationsEnabled(false);
    const canvas = findCanvas(frame);
    const rc = getClientRect(canvas);
    const x = Math.floor((rc.right - rc.left) / 2);
    const y = Math.floor((rc.bottom - rc.top) / 3);
    const res = await client.request(ControlCommand.TestRefHover, ["show", x, y]);
    if (res[0] !== 0) {
      throw new Error(`issue-6429: could not show the popup: ${res[1]}`);
    }
    let p = await waitFor(client, "the popup did not open", (p) => p.visible && p.page === 2);

    // the popup's center: screen pixels for the original, frame pixels for ng
    const pop = p.pop ?? { x, y };
    let onPopup = clientToScreen(canvas, pop.x, pop.y);
    if (!USE_NG) {
      const wr = getWindowRect(p.hwnd);
      onPopup = { x: Math.floor((wr.left + wr.right) / 2), y: Math.floor((wr.top + wr.bottom) / 2) };
    }
    const popPt = { x: pop.x, y: pop.y, scale: p.scale };
    const send = (msg: number, delta: number, keys: number) => wheel(client, p.hwnd, msg, delta, keys, onPopup, popPt);

    const zoomed = await (async () => {
      await send(WM_MOUSEWHEEL, WHEEL_DELTA, MK_CONTROL);
      return waitFor(client, "Ctrl + wheel on the popup did not zoom", (q) => q.zoom > p.zoom);
    })();
    if (zoomed.x <= p.x) {
      throw new Error(`issue-6429: zoom did not keep the center: x ${p.x} -> ${zoomed.x}`);
    }
    p = zoomed;

    await send(WM_MOUSEHWHEEL, WHEEL_DELTA, 0);
    p = await waitFor(client, "horizontal wheel on the popup did not scroll right", (q) => q.x > p.x);

    await send(WM_MOUSEWHEEL, WHEEL_DELTA, MK_SHIFT);
    p = await waitFor(client, "Shift + wheel up on the popup did not scroll left", (q) => q.x < p.x);

    await send(WM_MOUSEWHEEL, -WHEEL_DELTA, MK_SHIFT);
    await waitFor(client, "Shift + wheel down on the popup did not scroll right", (q) => q.x > p.x);
    console.log("issue-6429: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
