// The overlay toolbar should hide when the pointer leaves its reveal strip for the title bar.
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient, LayoutInfo } from "./control.ts";
import { IS_WIN } from "./host.ts";
import { makeBlankPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import {
  canMoveCursor,
  clientToScreen,
  getCursorPos,
  HWND_TOPMOST,
  setCursorPos,
  setWindowPos,
  sleep,
  SWP_NOACTIVATE,
  SWP_NOMOVE,
  SWP_NOSIZE,
  topLevelWindowFromPoint,
} from "./winapi.ts";

async function waitLayout(client: ControlClient, what: string, ready: (layout: LayoutInfo) => boolean) {
  const deadline = Date.now() + 3000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const layout = await client.layout();
    if (ready(layout)) return;
    if (Date.now() >= deadline) throw new Error(`issue-6593: ${what}\n${layout.raw}`);
    await sleep(40);
  }
}

async function checkCaption(dir: string, doc: string, caption: "pinned" | "auto-hidden") {
  const pinned = caption === "pinned";
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UseTabs = true\nShowCaption = ${pinned}\nToolbar = overlay\nToolbarCustomLayout = CmdOpenFile CmdPrint\n` +
      "Scrollbars = hide\nNoHomeTab = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir, doc]);
  try {
    await client.waitForRenderIdle();
    if (!setWindowPos(frame, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE, HWND_TOPMOST)) {
      throw new Error("issue-6593: could not raise the test window");
    }
    const hover = (x: number, y: number) => {
      const pt = clientToScreen(frame, x, y);
      if (topLevelWindowFromPoint(pt.x, pt.y) !== frame) {
        throw new Error("issue-6593: another window covers the hover point");
      }
      if (!setCursorPos(pt.x, pt.y)) throw new Error("issue-6593: could not move the cursor");
    };
    const { canvas, toolbar } = (await client.layout()).items;
    const { x, y, dx, dy } = canvas.rect;
    const left = Math.floor((x + toolbar.rect.x) / 2);
    const right = Math.floor((x + dx + toolbar.rect.x + toolbar.rect.dx) / 2);
    const center = Math.floor(x + dx / 2);
    const captionY = pinned ? Math.floor(y / 2) : y + 2;

    for (const [side, hoverX] of [
      ["left", left],
      ["right", right],
    ] as const) {
      hover(center, y + Math.floor(dy / 2));
      await waitLayout(
        client,
        "toolbar or overlay caption still visible over the page",
        ({ items }) => !items.toolbar.visible && !items.captionOverlay.visible,
      );
      hover(hoverX, y + 20);
      await waitLayout(
        client,
        `toolbar did not reveal at the ${side} of the page`,
        ({ items }) => items.toolbar.visible,
      );
      hover(hoverX, captionY);
      await waitLayout(
        client,
        `toolbar stayed visible over the ${side} of the ${caption} caption`,
        ({ items }) => !items.toolbar.visible && (pinned || items.captionOverlay.visible),
      );
    }
  } finally {
    try {
      await client.quit();
    } finally {
      client.close();
      await killAndWait(proc);
    }
  }
}

export async function testit(): Promise<void> {
  if (!IS_WIN || USE_NG) return;
  if (!canMoveCursor()) {
    console.log("SKIP issue-6593: needs a desktop where the cursor can be moved");
    return;
  }

  const cursor = getCursorPos();
  const dir = mkdtempSync(tmpPath("issue-6593-"));
  try {
    const doc = join(dir, "blank.pdf");
    writeFileSync(doc, makeBlankPdf(), "latin1");
    await checkCaption(dir, doc, "pinned");
    await checkCaption(dir, doc, "auto-hidden");
  } finally {
    setCursorPos(cursor.x, cursor.y);
    rmSync(dir, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
