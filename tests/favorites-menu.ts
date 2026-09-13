// The favorites in the Favorites menu are CmdFavorite commands carrying the
// file path and the page as arguments: each entry needs its own id, the id must
// survive a menu rebuild, and invoking it must go to that page.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { assemblePdf, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, pressKey, waitForContextMenu } from "./win-automation.ts";
import {
  getMenuItemCount,
  getMenuItemId,
  getMenuItemText,
  getPopupMenuHandle,
  getSubMenu,
  postMessage,
  sendMessage,
  VK_ESCAPE,
} from "./winapi.ts";

const WM_COMMAND = 0x0111;
// the menu bar toolbar posts kMenuBarCmdFirst + <top level menu index> (Menu.cpp)
const kMenuBarCmdFirst = 50000;
// index of "F&avorites" in menuDefMenubar
const kFavoritesMenuIdx = 6;
// a re-open within 500ms of dismissing the popup is treated as toggle-close
const kMenuBarToggleMs = 700;

function makePdf(): string {
  const objects = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 4 0 R 5 0 R] /Count 3 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << >> >>",
  ];
  return assemblePdf(objects);
}

// ids of the menu items whose label contains one of `names`, looking inside
// submenus (favorites for a file with more than one favorite get their own)
function collectIds(menu: bigint, names: string[], ids: number[]): void {
  for (let i = 0; i < getMenuItemCount(menu); i++) {
    const sub = getSubMenu(menu, i);
    if (sub) {
      collectIds(sub, names, ids);
      continue;
    }
    const text = getMenuItemText(menu, i);
    const idx = names.findIndex((n) => text.includes(n));
    if (idx >= 0) {
      ids[idx] = getMenuItemId(menu, i);
    }
  }
}

async function favoritesMenuIds(frame: number, names: string[]): Promise<number[]> {
  postMessage(frame, WM_COMMAND, kMenuBarCmdFirst + kFavoritesMenuIdx, 0);
  const popup = await waitForContextMenu();
  const ids = new Array(names.length).fill(0);
  collectIds(getPopupMenuHandle(popup), names, ids);
  await pressKey(frame, VK_ESCAPE);
  return ids;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("favorites-menu");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "document.pdf");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(pdf, makePdf(), "latin1");
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    "CheckForUpdates = false\nRestoreSession = false\nShowStartPage = false\nShowFavorites = false\nUseTabs = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    for (const pageNo of [1, 2]) {
      const res = await client.request(ControlCommand.TestFavoriteNav, ["add", pageNo]);
      if (res[0] !== 0) {
        throw new Error(`could not add favorite for page ${pageNo}: ${String(res[1] ?? "")}`);
      }
    }

    // the Favorites menu only exists as a menu bar, which is off by default here
    sendMessage(frame, WM_COMMAND, cmdId("CmdToggleMenuBar"), 0);
    await Bun.sleep(kMenuBarToggleMs);

    const names = ["Page 1", "Page 2"];
    const [id1, id2] = await favoritesMenuIds(frame, names);
    if (!id1 || !id2) {
      throw new Error(`favorites missing from the Favorites menu: ${id1}, ${id2}`);
    }
    if (id1 === id2) {
      throw new Error(`favorites share command id ${id1}`);
    }

    await Bun.sleep(kMenuBarToggleMs);
    const [id1Again] = await favoritesMenuIds(frame, names);
    if (id1Again !== id1) {
      throw new Error(`command id changed when the menu was rebuilt: ${id1} -> ${id1Again}`);
    }

    // invoking the menu item has to go to the favorite's page
    const at = await client.goToLocation(0, 3);
    if (at.page !== 3) {
      throw new Error(`could not go to page 3, landed on ${at.page}`);
    }
    sendMessage(frame, WM_COMMAND, id2, 0);
    const info = await client.chapterInfo();
    if (info.page !== 2) {
      throw new Error(`CmdFavorite went to page ${info.page}, expected 2`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
