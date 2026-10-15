// ng: picking an item of the page context menu ran nothing. gpui dispatches a
// popup item's action from the focus the menu was opened with (previousFocus),
// and the canvas menu is opened by the canvas's own mouse-down, which left the
// focus where it was - not inside the canvas, whose action handler runs the
// command. OpenPopupMenuAt focuses the trigger first.
// The test opens the menu with a right click and picks "Add page 1 to
// favorites", by click and from the keyboard; the Add Favorite dialog has to
// open.
//
// orig shows a Win32 menu (TrackPopupMenu); this is about ng's gpui menu.
//
// Run: bun tests/ctx-menu-pick.ts -ng [--no-build]
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control";
import { ROOT, runStandalone, USE_NG } from "./util";
import { sleep, VK_DOWN, VK_RETURN } from "./winapi";
import { killAndWait, launchControlled } from "./win-automation";

async function uiState(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
}

// the menu opens with its top left at the click; its rows are 28 dips apart,
// the first one's middle 18 under the top
const kMenuItemDx = 90;
const kFirstRowDy = 18;
const kRowDy = 28;
// Selection >, Visual Search With Google Lens >, Add page 1 to favorites
const kAddFavoriteRow = 2;

async function toolList(client: ControlClient): Promise<string> {
  return String((await client.request(ControlCommand.TestToolWindow, ["list"]))[1] ?? "");
}

async function addFavoriteOpen(client: ControlClient): Promise<boolean> {
  if (/addfavorite made=1/.test(await toolList(client))) {
    return true;
  }
  // the dialog in the frame, where there are no windows of their own
  return / dialog=1/.test(await uiState(client));
}

async function openPageMenu(client: ControlClient): Promise<{ x: number; y: number }> {
  const m = /canvas=(-?\d+),(-?\d+),(\d+),(\d+)/.exec(await uiState(client));
  if (!m) {
    throw new Error("ctx-menu-pick: no canvas rect");
  }
  const x = +m[1]! + Math.floor(+m[3]! / 2);
  const y = +m[2]! + Math.floor(+m[4]! / 2);
  await client.request(ControlCommand.TestInput, ["click", x, y, 1, 0]);
  const deadline = Date.now() + 3000;
  while (!/ popup=1/.test(await uiState(client))) {
    if (Date.now() > deadline) {
      throw new Error("ctx-menu-pick: the context menu did not open");
    }
    await sleep(40);
  }
  return { x, y };
}

async function waitAddFavorite(client: ControlClient, how: string): Promise<void> {
  const until = Date.now() + 3000;
  while (!(await addFavoriteOpen(client))) {
    if (Date.now() > until) {
      throw new Error(`ctx-menu-pick: ${how} 'Add page 1 to favorites' did not open Add Favorite`);
    }
    await sleep(50);
  }
}

async function pickByClick(client: ControlClient): Promise<void> {
  await client.waitForRenderIdle();
  const { x, y } = await openPageMenu(client);
  const rowY = y + kFirstRowDy + kAddFavoriteRow * kRowDy;
  await client.request(ControlCommand.TestInput, ["click", x + kMenuItemDx, rowY, 0, 0]);
  await waitAddFavorite(client, "clicking");
}

async function pickByKey(client: ControlClient): Promise<void> {
  await client.waitForRenderIdle();
  await openPageMenu(client);
  await client.waitForRenderIdle();
  if (/addfavorite made=1/.test(await toolList(client))) {
    throw new Error("ctx-menu-pick: Add Favorite was already open before Enter");
  }
  // Selection >, Visual Search With Google Lens >, Add page 1 to favorites
  for (let i = 0; i < 3; i++) {
    await client.request(ControlCommand.TestInput, ["key", VK_DOWN, 0, 0, 0]);
  }
  await client.request(ControlCommand.TestInput, ["key", VK_RETURN, 0, 0, 0]);
  const until = Date.now() + 3000;
  while (!/addfavorite made=1/.test(await toolList(client))) {
    if (Date.now() > until) {
      throw new Error("ctx-menu-pick: Enter on 'Add page 1 to favorites' did not open Add Favorite");
    }
    await sleep(50);
  }
}

async function runPick(pick: (client: ControlClient) => Promise<void>): Promise<void> {
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client } = await launchControlled([pdf]);
  try {
    await pick(client);
  } finally {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("ctx-menu-pick: ng only (orig's context menu is a Win32 menu)");
    return;
  }
  await runPick(pickByClick);
  await runPick(pickByKey);
}

if (import.meta.main) {
  await runStandalone(testit);
}
