// #5917: after a document failed to load, next/prev file in folder was stuck on
// the error page. CollectNextPrevFilesIfChanged() drops files that failed to
// open, and on a warm directory cache it did not re-add the current file, so
// files.Find(currentPath) came back -1 and navigation gave up with the
// "First/Last file in folder." notification instead of moving on.
//
// Drives the real app and asserts where next/prev land, reading the loaded file
// from the frame's title.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util";
import { getWindowText, postMessage, sleep, WM_CLOSE } from "./winapi";
import { launchControlled, sendCommand, waitForExit, waitForTitle, killAndWait } from "./win-automation";

const GOOD = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");

// `names` ending in -broken.pdf are not PDFs at all, so they fail to load
function makeFolder(name: string, names: string[]): string {
  const dir = tmpPath(`issue-5917-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (const n of names) {
    if (n.endsWith("-broken.pdf")) {
      writeFileSync(join(dir, n), "not a pdf at all\n".repeat(64));
    } else {
      copyFileSync(GOOD, join(dir, n));
    }
  }
  return dir;
}

// which document the window shows, from its title ("<file> - SumatraPDF")
function loadedFile(frame: number): string {
  return getWindowText(frame).split(" - ")[0].trim();
}

async function navigate(frame: number, cmd: string, from: string): Promise<string> {
  sendCommand(frame, cmdId(cmd));
  // The load runs on a background thread and a file that fails to load
  // auto-advances, so the first title that differs can be one we are only
  // passing through: a.pdf -> b-broken.pdf -> c.pdf. Wait for the title to
  // change and then to stop changing, so this reports where navigation landed
  // rather than what flashed by. (The last-file case legitimately stays on the
  // error page; there the title settles on it.)
  let title = await waitForTitle(frame, (t) => {
    const name = t.split(" - ")[0].trim();
    return name.length > 0 && name !== from;
  });
  const deadline = Date.now() + 6000;
  for (let stable = 0; stable < 3 && Date.now() < deadline; stable++) {
    await sleep(200);
    const cur = getWindowText(frame);
    if (cur !== title) {
      title = cur;
      stable = -1; // changed again: start counting over
    }
  }
  return title.split(" - ")[0].trim();
}

// open `startFile` in `dir`, run each command in turn, return where each landed
async function openAndNavigate(dir: string, startFile: string, cmds: string[]): Promise<string[]> {
  const { proc, client, frame } = await launchControlled([join(dir, startFile)]);
  try {
    await waitForTitle(frame, (t) => t.includes(startFile));
    await client.waitForRenderIdle();
    let current = loadedFile(frame);
    const got: string[] = [];
    for (const cmd of cmds) {
      current = await navigate(frame, cmd, current);
      got.push(current);
    }
    postMessage(frame, WM_CLOSE, 0, 0);
    await waitForExit(proc, 5000);
    return got;
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  // a broken file in the middle: next skips it and lands on the file after it
  const dir = makeFolder("skip", ["a.pdf", "b-broken.pdf", "c.pdf"]);
  let got = await openAndNavigate(dir, "a.pdf", ["CmdOpenNextFileInFolder"]);
  if (got[0] !== "c.pdf") {
    throw new Error(`issue-5917: next from a.pdf landed on '${got[0]}', want 'c.pdf'`);
  }

  // the reported case: the broken file is last, so next stops on its error page
  // (there is nothing after it to advance to) and prev has to work from there
  const dir2 = makeFolder("stuck", ["a.pdf", "b-broken.pdf"]);
  got = await openAndNavigate(dir2, "a.pdf", ["CmdOpenNextFileInFolder", "CmdOpenPrevFileInFolder"]);
  if (got[0] !== "b-broken.pdf") {
    throw new Error(`issue-5917: next from a.pdf landed on '${got[0]}', want the 'b-broken.pdf' error page`);
  }
  if (got[1] !== "a.pdf") {
    throw new Error(`issue-5917: prev from the error page landed on '${got[1]}', want 'a.pdf'`);
  }

  console.log("issue-5917: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
