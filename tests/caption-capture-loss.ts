// A caption button pressed and then losing the mouse capture (Alt+Tab, a
// dialog) gets no button-up. It stayed pressed, so a later button-up over it,
// from a press elsewhere, clicked it. Here: the app icon, whose click opens
// the system menu.
//
// orig only: ng's caption is drawn by GPUI.
// Run: bun tests/caption-capture-loss.ts [--no-build]

import { writeFileSync } from "node:fs";
import { IS_MAC } from "./host.ts";
import { makeOnePagePdf, runStandalone, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled, waitForContextMenu } from "./win-automation.ts";
import {
  MK_LBUTTON,
  packCoords,
  postMessage,
  sendMessage,
  sleep,
  VK_ESCAPE,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
} from "./winapi.ts";

const WM_CAPTURECHANGED = 0x0215;

export async function testit(): Promise<void> {
  if (IS_MAC || USE_NG) {
    console.log("SKIP caption-capture-loss: orig on Windows only");
    return;
  }
  const pdf = tmpPath("caption-capture-loss.pdf");
  writeFileSync(pdf, makeOnePagePdf(), "latin1");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    const info = await client.layout("get");
    const icon = info.nodes.find((n) => n.path === "caption/0/0");
    if (!icon || icon.kind !== "captionBtn" || !icon.visible) {
      throw new Error(`caption-capture-loss: no app icon in the caption: ${JSON.stringify(icon)}`);
    }
    const at = packCoords(icon.rect.x + Math.floor(icon.rect.dx / 2), icon.rect.y + Math.floor(icon.rect.dy / 2));

    postMessage(frame, WM_LBUTTONDOWN, MK_LBUTTON, at);
    // a sent message is handled before posted ones: let the press land first
    await sleep(300);
    sendMessage(frame, WM_CAPTURECHANGED, 0, 0);
    postMessage(frame, WM_LBUTTONUP, 0, at);
    const menu = await waitForContextMenu(1000);
    if (menu) {
      postMessage(menu, WM_KEYDOWN, VK_ESCAPE, 0);
      throw new Error("caption-capture-loss: a button-up after losing the capture opened the system menu");
    }
    console.log("caption-capture-loss: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
