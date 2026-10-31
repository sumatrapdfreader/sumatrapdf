// #6448: opening a .html file showed every .html file in its folder as pages of
// one document. HTML now opens as a single file (Markdown keeps folder mode).
// Its home page thumbnail is a WebView2 picture of the page: MuPDF's render
// lost images, so thumbnails looked blank.
import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control";
import { cmdId, loadPng, makePng, runStandalone, tmpPath, USE_NG } from "./util";
import { findChildWindow, sendMessage, sleep, treeExpandRecursively, TVE_EXPAND, TVM_GETCOUNT } from "./winapi";
import {
  killAndWait,
  launchControlled,
  launchSumatra,
  sendCommand,
  waitForFrame,
  waitForTitle,
} from "./win-automation";

async function tocItemCount(frame: number, client: ControlClient | null): Promise<number> {
  if (client) {
    const raw = String((await client.request(ControlCommand.TestUiState, []))[1] ?? "");
    const m = /tocItems=(-?\d+)/.exec(raw);
    return m ? +m[1]! : -1;
  }
  const tree = findChildWindow(frame, "SysTreeView32");
  if (!tree) {
    return -1;
  }
  treeExpandRecursively(tree, TVE_EXPAND);
  return Number(sendMessage(tree, TVM_GETCOUNT, 0, 0));
}

async function testSingleFile(dir: string): Promise<void> {
  for (const name of ["abc", "xyz"]) {
    const body = `<h1>${name} title</h1><p>text</p><h2>${name} section</h2><p>text</p>`;
    writeFileSync(join(dir, `${name}.html`), `<html><body>${body}</body></html>\n`);
  }

  const path = join(dir, "abc.html");
  let proc: Bun.Subprocess;
  let client: ControlClient | null = null;
  let frame = 0;
  if (USE_NG) {
    ({ proc, client, frame } = await launchControlled([path]));
  } else {
    proc = launchSumatra([path]);
  }
  try {
    frame = frame || (await waitForFrame(proc.pid!));
    await waitForTitle(frame, (t) => t.startsWith("abc.html"), 30000);
    sendCommand(frame, cmdId("CmdToggleBookmarks"));

    // wait for the background build to settle: the same count twice in a row
    const deadline = Date.now() + 15000;
    let last = -1;
    let n = -1;
    while (Date.now() < deadline) {
      n = await tocItemCount(frame, client);
      if (n > 0 && n === last) {
        break;
      }
      last = n;
      await sleep(500);
    }
    // the file plus its 2 headings; xyz.html must not be there
    const want = 3;
    if (n !== want) {
      throw new Error(`issue-6448: TOC has ${n} items, want ${want} (abc.html only, no sibling xyz.html)`);
    }
  } finally {
    client?.close();
    await killAndWait(proc);
  }
}

// mirrors GetThumbnailPathTemp: md5 of the path, lowercase hex, .png
function thumbnailPath(appData: string, docPath: string): string {
  const hash = new Bun.CryptoHasher("md5").update(docPath).digest("hex");
  return join(appData, "sumatrapdfcache", `${hash}.png`);
}

// the center pixel of the thumbnail, or null while it doesn't exist yet
function thumbCenter(path: string): number[] | null {
  if (!existsSync(path)) {
    return null;
  }
  try {
    const img = loadPng(path);
    const off = (Math.floor(img.h / 2) * img.w + Math.floor(img.w / 2)) * img.nComp;
    return [img.data[off]!, img.data[off + 1]!, img.data[off + 2]!];
  } catch {
    // being written
    return null;
  }
}

async function testThumbnail(dir: string): Promise<void> {
  if (USE_NG) {
    console.log("issue-6448: ng has no browser view capture, skipping the thumbnail check");
    return;
  }
  const appData = join(dir, "appdata");
  mkdirSync(appData, { recursive: true });
  const settings = ["UiLanguage = en", "CheckForUpdates = false", "RememberOpenedFiles = true", "ShowStartPage = true"];
  writeFileSync(join(appData, "SumatraPDF-settings.txt"), settings.join("\n") + "\n");
  // MuPDF draws an image it can't load as "[image]" on white
  writeFileSync(join(dir, "red.png"), makePng(64, 64, [192, 0, 0]));
  const path = join(dir, "red.html");
  writeFileSync(
    path,
    `<html><body style="margin:0"><img src="red.png" style="width:100%;height:2000px"></body></html>\n`,
  );

  // not -for-testing: that doesn't save thumbnails
  const { proc, client } = await launchControlled(["-appdata", appData, path], { saveSettings: true });
  const thumb = thumbnailPath(appData, path);
  try {
    const deadline = Date.now() + 20000;
    let px: number[] | null = null;
    while (Date.now() < deadline) {
      px = thumbCenter(thumb);
      if (px) {
        break;
      }
      await sleep(200);
    }
    if (!px) {
      throw new Error(`issue-6448: no thumbnail at ${thumb}`);
    }
    const [r, g, b] = px;
    if (r! < 150 || g! > 60 || b! > 60) {
      throw new Error(`issue-6448: thumbnail center is rgb(${px}), want the red image`);
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6448");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  await testSingleFile(dir);
  await testThumbnail(dir);
  console.log("issue-6448: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
