import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import {
  findChildWindow,
  findTopWindow,
  getClassName,
  getFocusedHwnd,
  getRootWindow,
  isWindowVisible,
  postMessage,
  sendText,
  sleep,
  VK_HOME,
  VK_RIGHT,
  VK_RETURN,
  WM_KEYDOWN,
} from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand } from "./win-automation.ts";

const THUMBNAIL_CLASS = "SumatraPDF_ThumbnailNavigation";
const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

function findPalette(frame: number): { palette: number; edit: number } {
  const edit = getFocusedHwnd(frame);
  if (!edit || getClassName(edit) !== "Edit") {
    return { palette: 0, edit: 0 };
  }
  const palette = getRootWindow(edit);
  return { palette: palette === frame ? 0 : palette, edit };
}

type ThumbnailState = { active: boolean; page: number; rendered: number };

async function thumbnailState(client: ControlClient): Promise<ThumbnailState | null> {
  const res = await client.request(ControlCommand.TestCommandPalette, []);
  if ((res[0] as number) !== 0) {
    return null;
  }
  const out = String(res[1] ?? "");
  const m = /thumb=(\d+) page=(\d+) rendered=(\d+)/.exec(out);
  return m ? { active: m[1] === "1", page: +m[2]!, rendered: +m[3]! } : null;
}

async function waitForThumbnail(client: ControlClient, expected: boolean): Promise<ThumbnailState | null> {
  const deadline = Date.now() + 5_000;
  while (Date.now() < deadline) {
    const state = await thumbnailState(client);
    if (state?.active === expected && (!expected || state.rendered > 0)) {
      return state;
    }
    await sleep(40);
  }
  return thumbnailState(client);
}

async function waitForPage(client: ControlClient, page: number): Promise<boolean> {
  const deadline = Date.now() + 3_000;
  while (Date.now() < deadline) {
    if ((await thumbnailState(client))?.page === page) {
      return true;
    }
    await sleep(40);
  }
  return false;
}

async function currentPage(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestFavoriteNav, ["page", 0]);
  const m = /OK page=(\d+)/.exec(String(res[1] ?? ""));
  return m ? +m[1]! : 0;
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("command-palette-thumbnails");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdNavigateThumbnail"));

    const deadline = Date.now() + 8_000;
    let handles = { palette: 0, edit: 0 };
    while (Date.now() < deadline) {
      handles = findPalette(frame);
      if (handles.palette && handles.edit) {
        break;
      }
      await sleep(50);
    }
    if (!handles.palette || !handles.edit) {
      throw new Error("command-palette-thumbnails: palette did not open");
    }

    postMessage(handles.palette, WM_KEYDOWN, VK_HOME, 0);
    const thumbnail = await waitForThumbnail(client, true);
    if (!thumbnail?.active || thumbnail.page !== 1 || thumbnail.rendered < 1) {
      throw new Error(`command-palette-thumbnails: thumbnail grid not ready: ${JSON.stringify(thumbnail)}`);
    }
    if (findChildWindow(handles.palette, THUMBNAIL_CLASS) || findTopWindow(proc.pid!, THUMBNAIL_CLASS)) {
      throw new Error("command-palette-thumbnails: thumbnail window opened");
    }

    postMessage(handles.palette, WM_KEYDOWN, VK_RIGHT, 0);
    if (!(await waitForPage(client, 2))) {
      throw new Error("command-palette-thumbnails: right arrow did not select page 2");
    }

    sendText(handles.edit, ">");
    if ((await waitForThumbnail(client, false))?.active !== false) {
      throw new Error("command-palette-thumbnails: thumbnail grid remained in command view");
    }
    const commandView = findPalette(frame);
    if (commandView.palette !== handles.palette) {
      throw new Error("command-palette-thumbnails: switching views replaced the palette");
    }
    if (!isWindowVisible(handles.palette)) {
      throw new Error("command-palette-thumbnails: switching to command view closed the palette");
    }

    sendText(commandView.edit, "&");
    const returned = await waitForThumbnail(client, true);
    if (!returned?.active || returned.page !== 2) {
      throw new Error(`command-palette-thumbnails: thumbnail grid did not return: ${JSON.stringify(returned)}`);
    }
    if (findPalette(frame).palette !== handles.palette) {
      throw new Error("command-palette-thumbnails: switching views replaced the palette");
    }

    postMessage(handles.palette, WM_KEYDOWN, VK_RETURN, 0);
    const navigateDeadline = Date.now() + 3_000;
    while (Date.now() < navigateDeadline && (isWindowVisible(handles.palette) || (await currentPage(client)) !== 2)) {
      await sleep(40);
    }
    if (isWindowVisible(handles.palette) || (await currentPage(client)) !== 2) {
      throw new Error("command-palette-thumbnails: Enter did not open page 2 and close the palette");
    }

    console.log("command-palette-thumbnails: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
