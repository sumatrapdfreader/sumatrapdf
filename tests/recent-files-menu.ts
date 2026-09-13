// The recent files at the bottom of the File menu are CmdFileHistory commands
// carrying the file path as an argument: each entry needs its own id, the id
// must survive a menu rebuild, and invoking it must open that file.
import { join } from "node:path";
import { ROOT, cmdId, runStandalone } from "./util.ts";
import { killAndWait, launchControlled, pressKey, waitForContextMenu, waitForTitle } from "./win-automation.ts";
import {
  getMenuItemCount,
  getMenuItemId,
  getMenuItemText,
  getPopupMenuHandle,
  postMessage,
  sendMessage,
  VK_ESCAPE,
} from "./winapi.ts";

const WM_COMMAND = 0x0111;
// the menu bar toolbar posts kMenuBarCmdFirst + <top level menu index> (Menu.cpp)
const kMenuBarCmdFirst = 50000;
// a re-open within 500ms of dismissing the popup is treated as toggle-close
const kMenuBarToggleMs = 700;

const docA = join(ROOT, "tests", "issue-1189.pdf");
const docB = join(ROOT, "tests", "issue-3219.pdf");

// the ids of the File menu entries whose label contains one of `names`
async function fileMenuIds(frame: number, names: string[]): Promise<number[]> {
  postMessage(frame, WM_COMMAND, kMenuBarCmdFirst, 0);
  const popup = await waitForContextMenu();
  const menu = getPopupMenuHandle(popup);
  const ids = new Array(names.length).fill(0);
  for (let i = 0; i < getMenuItemCount(menu); i++) {
    const text = getMenuItemText(menu, i);
    const idx = names.findIndex((n) => text.includes(n));
    if (idx >= 0) {
      ids[idx] = getMenuItemId(menu, i);
    }
  }
  await pressKey(frame, VK_ESCAPE);
  return ids;
}

export async function testit(): Promise<void> {
  const { proc, client, frame } = await launchControlled([docA, docB]);
  try {
    await waitForTitle(frame, (t) => t.includes("issue-3219"));
    // the File menu only exists as a menu bar, which is off by default here
    sendMessage(frame, WM_COMMAND, cmdId("CmdToggleMenuBar"), 0);
    await Bun.sleep(kMenuBarToggleMs);

    const [idA, idB] = await fileMenuIds(frame, ["issue-1189", "issue-3219"]);
    if (!idA || !idB) {
      throw new Error(`recent files missing from the File menu: ${idA}, ${idB}`);
    }
    if (idA === idB) {
      throw new Error(`recent files share command id ${idA}`);
    }

    await Bun.sleep(kMenuBarToggleMs);
    const [idA2] = await fileMenuIds(frame, ["issue-1189"]);
    if (idA2 !== idA) {
      throw new Error(`command id changed when the menu was rebuilt: ${idA} -> ${idA2}`);
    }

    // close the document, then open it again from the recent files
    sendMessage(frame, WM_COMMAND, cmdId("CmdPrevTab"), 0);
    await waitForTitle(frame, (t) => t.includes("issue-1189"));
    sendMessage(frame, WM_COMMAND, cmdId("CmdCloseCurrentDocument"), 0);
    await waitForTitle(frame, (t) => !t.includes("issue-1189"));
    sendMessage(frame, WM_COMMAND, idA, 0);
    await waitForTitle(frame, (t) => t.includes("issue-1189"));
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
