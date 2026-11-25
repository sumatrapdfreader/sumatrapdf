// The app icon at the left of the caption (tabs in the title bar) moves the
// window when dragged, like in 3.2 - 3.6.1: a steady, big enough target for
// touch. A click without a drag still opens the system menu.
//
// Run: bun tests/issue-6261.ts [--no-build]

import { writeFileSync } from "node:fs";
import { IS_MAC } from "./host.ts";
import { makeOnePagePdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import {
  clientToScreen,
  isWindowVisible,
  MK_LBUTTON,
  packCoords,
  postMessage,
  setCursorPos,
  VK_ESCAPE,
  WM_KEYDOWN,
  WM_LBUTTONDOWN,
  WM_LBUTTONUP,
  WM_MOUSEMOVE,
} from "./winapi.ts";
import { killAndWait, launchControlled, waitForContextMenu } from "./win-automation.ts";

// posts a mouse message at client pt, with the real cursor there too: the
// move loop and SetCapture look at it
function mouse(frame: number, msg: number, mk: number, pt: { x: number; y: number }) {
  const screen = clientToScreen(frame, pt.x, pt.y);
  setCursorPos(screen.x, screen.y);
  postMessage(frame, msg, mk, packCoords(pt.x, pt.y));
}

export async function testit(): Promise<void> {
  if (IS_MAC) {
    console.log("SKIP issue-6261: macOS has no caption system menu");
    return;
  }
  const pdf = tmpPath("issue-6261.pdf");
  writeFileSync(pdf, makeOnePagePdf(), "latin1");
  const { proc, client, frame } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
    // the icon is the caption's first button
    const info = await client.layout("get");
    const icon = info.nodes.find((n) => n.path === "caption/0/0");
    if (!icon || icon.kind !== "captionBtn" || !icon.visible) {
      throw new Error(`issue-6261: no app icon in the caption: ${JSON.stringify(icon)}`);
    }
    const at = { x: icon.rect.x + Math.floor(icon.rect.dx / 2), y: icon.rect.y + Math.floor(icon.rect.dy / 2) };

    // a click opens the system menu
    mouse(frame, WM_LBUTTONDOWN, MK_LBUTTON, at);
    mouse(frame, WM_LBUTTONUP, 0, at);
    const menu = await waitForContextMenu(3000, proc.pid);
    if (!menu) {
      throw new Error("issue-6261: a click on the icon didn't open the system menu");
    }
    postMessage(menu, WM_KEYDOWN, VK_ESCAPE, 0);
    await pollUntil(
      () => isWindowVisible(menu),
      (visible) => !visible,
      { timeoutMs: 3000, error: "issue-6261: the system menu didn't close" },
    );
    await client.waitForRenderIdle();

    // A drag moves the window instead: the release that follows is no click.
    // Windows ends the move loop at once without a real button down, so all
    // that can be seen here is that the menu stays shut. The drag stays on the
    // icon, where a click would open it.
    const to = { x: at.x + 8, y: at.y };
    mouse(frame, WM_LBUTTONDOWN, MK_LBUTTON, at);
    mouse(frame, WM_MOUSEMOVE, MK_LBUTTON, to);
    mouse(frame, WM_LBUTTONUP, 0, to);
    const menuAfterDrag = await waitForContextMenu(1000, proc.pid);
    if (menuAfterDrag) {
      postMessage(menuAfterDrag, WM_KEYDOWN, VK_ESCAPE, 0);
      throw new Error("issue-6261: dragging the icon opened the system menu instead of moving the window");
    }
    console.log("issue-6261: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
