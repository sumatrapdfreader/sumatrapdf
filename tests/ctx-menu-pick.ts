// ng: picking an item of the page context menu ran nothing. gpui dispatches a
// popup item's action from the focus the menu was opened with, and the canvas
// menu is opened by the canvas's own mouse-down, which left the focus where it
// was - not inside the canvas, whose action handler runs the command.
// The test opens the menu with a right click and clicks "Add page 1 to
// favorites"; the Add Favorite dialog has to open.
//
// orig shows a Win32 menu (TrackPopupMenu); this is about ng's gpui menu.
//
// Run: bun tests/ctx-menu-pick.ts -ng [--no-build]
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control";
import { ROOT, runStandalone, USE_NG } from "./util";
import { sleep } from "./winapi";
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

async function addFavoriteOpen(client: ControlClient): Promise<boolean> {
  const list = String((await client.request(ControlCommand.TestToolWindow, ["list"]))[1] ?? "");
  if (/addfavorite made=1/.test(list)) {
    return true;
  }
  // the dialog in the frame, where there are no windows of their own
  return / dialog=1/.test(await uiState(client));
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log("ctx-menu-pick: ng only (orig's context menu is a Win32 menu)");
    return;
  }
  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client } = await launchControlled([pdf]);
  try {
    await client.waitForRenderIdle();
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

    const rowY = y + kFirstRowDy + kAddFavoriteRow * kRowDy;
    await client.request(ControlCommand.TestInput, ["click", x + kMenuItemDx, rowY, 0, 0]);

    const until = Date.now() + 3000;
    while (!(await addFavoriteOpen(client))) {
      if (Date.now() > until) {
        throw new Error("ctx-menu-pick: clicking 'Add page 1 to favorites' did not open Add Favorite");
      }
      await sleep(50);
    }
  } finally {
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
