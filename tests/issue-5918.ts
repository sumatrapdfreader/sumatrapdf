// #5918: opening a .html file parsed every sibling .html file to build the TOC
// before showing the document, which hangs for minutes in a directory with
// thousands of files. The TOC now starts as a flat list of the files and a
// background thread replaces it with the real one (headings included).
//
// The hang itself needs a directory big or slow enough that it can't be
// reproduced at a reasonable test size (1000 files parse in about a second
// here), so this covers the machinery instead: the TOC ends up complete once
// the background build lands, and closing the document while a build is still
// in flight doesn't take the app down with it.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util";
import { findChildWindow, getWindowText, postMessage, sendMessage, sleep, WM_CLOSE } from "./winapi";
import { killAndWait, launchSumatra, sendCommand, waitForExit, waitForFrame, waitForTitle } from "./win-automation";

const TVM_GETCOUNT = 0x1100 + 5;
const nFiles = 400;
const headingsPerFile = 3;

function makeFolder(): string {
  const dir = tmpPath("issue-5918");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  for (let i = 0; i < nFiles; i++) {
    const name = `page-${String(i).padStart(4, "0")}.html`;
    // 3 headings each, plus filler so parsing isn't free
    const body = [
      `<h1>Title ${i}</h1>`,
      `<p>${"lorem ipsum dolor sit amet ".repeat(200)}</p>`,
      `<h2>Section ${i}.1</h2>`,
      `<p>${"lorem ipsum dolor sit amet ".repeat(200)}</p>`,
      `<h2>Section ${i}.2</h2>`,
      `<p>${"lorem ipsum dolor sit amet ".repeat(200)}</p>`,
    ].join("\n");
    writeFileSync(join(dir, name), `<html><head><title>Page ${i}</title></head><body>\n${body}\n</body></html>\n`);
  }
  return dir;
}

function tocItemCount(frame: number): number {
  const tree = findChildWindow(frame, "SysTreeView32");
  if (!tree) {
    return -1;
  }
  return Number(sendMessage(tree, TVM_GETCOUNT, 0, 0));
}

async function waitForTocCount(frame: number, want: number, timeoutMs = 30000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let last = -1;
  while (Date.now() < deadline) {
    last = tocItemCount(frame);
    if (last === want) {
      return last;
    }
    await sleep(40);
  }
  return last;
}

function launch(dir: string, file: string): Bun.Subprocess {
  return launchSumatra([join(dir, file)]);
}

async function waitForTocTree(frame: number, timeoutMs = 8000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const n = tocItemCount(frame);
    if (n > 0) {
      return n;
    }
    await sleep(40);
  }
  throw new Error("issue-5918: TOC tree never appeared");
}

// the TOC is complete once the background build finished; next/prev file in
// folder then navigates within the browser view instead of reloading (a reload
// would drop the TOC back to the flat file list)
async function testTocAndNextFile(dir: string): Promise<void> {
  const proc = launch(dir, "page-0000.html");
  const wantFull = nFiles * (1 + headingsPerFile);
  try {
    const frame = await waitForFrame(proc.pid!);
    await waitForTitle(frame, (t) => t.startsWith("page-0000.html"), 60000);
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    const first = await waitForTocTree(frame);
    if (first < nFiles) {
      throw new Error(`issue-5918: TOC has ${first} items, want at least ${nFiles} (one per file)`);
    }

    const settled = await waitForTocCount(frame, wantFull);
    if (settled !== wantFull) {
      throw new Error(`issue-5918: TOC settled at ${settled} items, want ${wantFull} (files plus their headings)`);
    }

    sendCommand(frame, cmdId("CmdOpenNextFileInFolder"));
    // TOC must stay complete through the switch (a reload would drop to the flat list)
    const deadline = Date.now() + 5000;
    let switched = false;
    while (Date.now() < deadline) {
      const n = tocItemCount(frame);
      if (n !== wantFull) {
        throw new Error(`issue-5918: TOC dropped to ${n} items after next-file, want it kept at ${wantFull}`);
      }
      if (getWindowText(frame).startsWith("page-0001.html")) {
        switched = true;
        break;
      }
      await sleep(40);
    }
    if (!switched) {
      throw new Error(`issue-5918: next file did not open, title is '${getWindowText(frame)}'`);
    }

    postMessage(frame, WM_CLOSE, 0, 0);
    await waitForExit(proc, 5000);
  } finally {
    await killAndWait(proc);
  }
}

// closing the document while the background build is still running must not
// crash: the build has no model left to deliver its result to
async function testCloseDuringBuild(dir: string): Promise<void> {
  const proc = launch(dir, "page-0001.html");
  const wantFull = nFiles * (1 + headingsPerFile);
  try {
    const frame = await waitForFrame(proc.pid!);
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    // close once the background build has started, before it finishes
    const deadline = Date.now() + 5000;
    while (Date.now() < deadline) {
      const n = tocItemCount(frame);
      if (n > 0 && n < wantFull) {
        break;
      }
      if (n === wantFull) {
        break;
      }
      await sleep(30);
    }
    sendCommand(frame, cmdId("CmdClose"));
    postMessage(frame, WM_CLOSE, 0, 0);
    // only that it goes away: a crash would leave the report dialog up and a
    // deadlock would hang here. The exit code isn't asserted because closing
    // this fast intermittently exits 1 from the WebView2 teardown, with or
    // without a TOC build in flight.
    const code = await Promise.race([proc.exited, sleep(8000).then(() => "timeout")]);
    if (code === "timeout") {
      throw new Error("issue-5918: app did not exit after closing during the TOC build");
    }
  } finally {
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const dir = makeFolder();
  await testTocAndNextFile(dir);
  await testCloseDuringBuild(dir);
  console.log("issue-5918: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
