// #6247: removing a home page entry of a file with favorites (it is only hidden,
// not removed from history) left the entry on screen until the next relayout.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone, tmpPath } from "./util";
import { sendMessage, setCursorPos, sleep } from "./winapi";
import { findCanvas, launchControlled, killAndWait } from "./win-automation";
import type { ControlClient, HomeSelection } from "./control.ts";

const WM_KEYDOWN = 0x0100;
const VK_RIGHT = 0x27;
const VK_DELETE = 0x2e;
const nFiles = 3;

function makeAppDir(): string {
  const dir = tmpPath("issue-6247");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const favorites = `\t\tFavorites [\n\t\t\t[\n\t\t\t\tName = fav\n\t\t\t\tPageNo = 1\n\t\t\t]\n\t\t]`;
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "sub", `doc-${i}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n${favorites}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\nFileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

async function waitForHome(
  client: ControlClient,
  pred: (h: HomeSelection) => boolean,
  what: string,
): Promise<HomeSelection> {
  const deadline = Date.now() + 8000;
  for (;;) {
    const h = await client.homeSelection();
    if (h.ready && pred(h)) {
      return h;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6247: ${what} (last: ${h.raw})`);
    }
    await sleep(50);
  }
}

export async function testit(): Promise<void> {
  // a cursor over the thumbnails would move the selection through hover
  setCursorPos(0, 0);
  const { proc, client, frame } = await launchControlled(["-appdata", makeAppDir()]);
  try {
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-6247: home-page canvas not found");
    }
    await waitForHome(client, (h) => h.entries === nFiles && h.sel === 0, "home page never showed the files");

    // select the last (oldest) entry and remove it
    for (let i = 1; i < nFiles; i++) {
      sendMessage(canvas, WM_KEYDOWN, VK_RIGHT, 0);
      await waitForHome(client, (h) => h.sel === i, `Right did not select entry ${i}`);
    }
    sendMessage(canvas, WM_KEYDOWN, VK_DELETE, 0);
    await waitForHome(client, (h) => h.entries === nFiles - 1, "removed entry still shown");
    console.log("issue-6247: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
