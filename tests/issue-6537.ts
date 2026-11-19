// Revealing a hidden caption must not cover the toolbar or sit under the
// overlay scrollbar, and must stay up while the cursor is over the scrollbar.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC } from "./host.ts";
import type { ControlClient, LayoutInfo, LayoutItem } from "./control.ts";
import { makeBlankPdf, runStandalone, SLOW_BUILD_FACTOR, tmpPath, USE_NG } from "./util.ts";
import { killAndWait, launchControlled } from "./win-automation.ts";
import { canMoveCursor, clientToScreen, setCursorPos, sleep } from "./winapi.ts";

async function waitLayout(client: ControlClient, what: string, pred: (l: LayoutInfo) => boolean): Promise<LayoutInfo> {
  const deadline = Date.now() + 5_000 * SLOW_BUILD_FACTOR;
  for (;;) {
    const layout = await client.layout();
    if (pred(layout)) return layout;
    if (Date.now() > deadline) throw new Error(`issue-6537: ${what}\n${layout.raw}`);
    await sleep(40);
  }
}

const bottom = (it: LayoutItem) => it.rect.y + it.rect.dy;
const captionUp = (l: LayoutInfo) => !!l.items.captionOverlay?.visible;

async function withSettings(toolbar: string, run: (client: ControlClient, frame: number) => Promise<void>) {
  const dir = tmpPath(`issue-6537-${toolbar}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const doc = join(dir, "a.pdf");
  writeFileSync(doc, makeBlankPdf(), "latin1");
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UseTabs = true\nShowCaption = false\nToolbar = ${toolbar}\nScrollbars = overlay\n` +
      "RestoreSession = false\nNoHomeTab = true\nShowStartPage = false\n",
  );
  setCursorPos(0, 0);
  const { proc, client, frame } = await launchControlled(["-appdata", dir, "-zoom", "300", doc]);
  try {
    await client.waitForRenderIdle();
    await run(client, frame);
  } finally {
    setCursorPos(0, 0);
    client.close();
    await killAndWait(proc);
  }
}

function hover(frame: number, x: number, y: number) {
  const pt = clientToScreen(frame, x, y);
  setCursorPos(pt.x, pt.y);
}

export async function testit(): Promise<void> {
  // ng draws the caption in one GPUI surface; the hwnd checks are orig's
  if (IS_MAC || USE_NG) return;
  // the overlay polls the real cursor, so posted mouse messages can't stand in
  if (!canMoveCursor()) {
    console.log("issue-6537: the cursor can't be moved (no interactive desktop), skipping");
    return;
  }

  await withSettings("show", async (client, frame) => {
    const before = await client.layout();
    hover(frame, Math.floor(before.items.frame.rect.dx / 2), 3);
    const up = await waitLayout(client, "caption not revealed", captionUp);
    const cap = up.items.captionOverlay!;
    if (up.items.toolbar.rect.y < bottom(cap)) throw new Error(`issue-6537: caption covers the toolbar\n${up.raw}`);
    if (JSON.stringify(up.items.canvas.rect) !== JSON.stringify(before.items.canvas.rect))
      throw new Error("issue-6537: revealing the caption resized the document");
    setCursorPos(0, 0);
    const down = await waitLayout(client, "caption not hidden", (l) => !captionUp(l));
    if (down.items.toolbar.rect.y !== before.items.toolbar.rect.y)
      throw new Error(`issue-6537: toolbar not restored\n${down.raw}`);
  });

  await withSettings("hide", async (client, frame) => {
    const before = await client.layout();
    const sb = before.items.overlayScrollV;
    if (!sb?.visible) throw new Error(`issue-6537: no overlay scrollbar\n${before.raw}`);
    // the top right corner, over the scrollbar
    const x = sb.rect.x + Math.floor(sb.rect.dx / 2);
    hover(frame, x, 3);
    const up = await waitLayout(client, "caption not revealed over the scrollbar", captionUp);
    const sbUp = up.items.overlayScrollV!;
    if (!sbUp.visible || sbUp.rect.y < bottom(up.items.captionOverlay!))
      throw new Error(`issue-6537: overlay scrollbar covers the caption\n${up.raw}`);
    hover(frame, x, bottom(up.items.captionOverlay!) - 3);
    await sleep(300);
    await waitLayout(client, "caption hid with the cursor over it", captionUp);
  });

  await withSettings("overlay", async (client, frame) => {
    const before = await client.layout();
    hover(frame, Math.floor(before.items.frame.rect.dx / 2), 3);
    const up = await waitLayout(
      client,
      "caption and toolbar not revealed",
      (l) => captionUp(l) && l.items.toolbar.visible,
    );
    if (up.items.toolbar.rect.y < bottom(up.items.captionOverlay!))
      throw new Error(`issue-6537: caption covers the overlay toolbar\n${up.raw}`);
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
