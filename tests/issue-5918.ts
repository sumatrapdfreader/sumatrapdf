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
import { cmdId, EXE, runStandalone, tmpPath } from "./util";
import { findChildWindow, getWindowText, postMessage, sendMessage, sleep, WM_CLOSE } from "./winapi";
import { sendCommand, waitForFrame } from "./win-automation";

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

// poll until the tree stops growing (the background build landed)
async function waitForTocToSettle(frame: number, timeoutMs = 30000): Promise<number> {
  const deadline = Date.now() + timeoutMs;
  let last = -1;
  let stable = 0;
  while (Date.now() < deadline) {
    const n = tocItemCount(frame);
    if (n === last && n > 0) {
      if (++stable >= 3) {
        return n;
      }
    } else {
      stable = 0;
      last = n;
    }
    await sleep(300);
  }
  return last;
}

// the document is up once the window title names the file we opened
async function waitForDocumentShown(frame: number, name: string, timeoutMs = 60000): Promise<boolean> {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (getWindowText(frame).startsWith(name)) {
      return true;
    }
    await sleep(100);
  }
  return false;
}

function launch(dir: string, file: string): Bun.Subprocess {
  return Bun.spawn([EXE, "-for-testing", join(dir, file)], { stdout: "ignore", stderr: "ignore" });
}

// the TOC is complete once the background build finished
async function testTocCompletes(dir: string): Promise<void> {
  const proc = launch(dir, "page-0000.html");
  try {
    const frame = await waitForFrame(proc.pid!);
    if (!(await waitForDocumentShown(frame, "page-0000.html"))) {
      throw new Error("issue-5918: document never opened");
    }
    sendCommand(frame, cmdId("CmdToggleBookmarks"));
    await sleep(1200);

    const first = tocItemCount(frame);
    if (first < nFiles) {
      throw new Error(`issue-5918: TOC has ${first} items, want at least ${nFiles} (one per file)`);
    }

    const settled = await waitForTocToSettle(frame);
    const wantFull = nFiles * (1 + headingsPerFile);
    if (settled !== wantFull) {
      throw new Error(`issue-5918: TOC settled at ${settled} items, want ${wantFull} (files plus their headings)`);
    }
    postMessage(frame, WM_CLOSE, 0, 0);
    await sleep(600);
  } finally {
    proc.kill();
  }
}

// closing the document while the background build is still running must not
// crash: the build has no model left to deliver its result to
async function testCloseDuringBuild(dir: string): Promise<void> {
  const proc = launch(dir, "page-0001.html");
  try {
    const frame = await waitForFrame(proc.pid!);
    // deliberately do not wait for the build to finish
    await sleep(700);
    sendCommand(frame, cmdId("CmdClose"));
    await sleep(1500);
    postMessage(frame, WM_CLOSE, 0, 0);
    const code = await Promise.race([proc.exited, sleep(6000).then(() => -1)]);
    if (code !== 0 && code !== -1) {
      throw new Error(`issue-5918: closing during the TOC build exited with ${code}`);
    }
  } finally {
    proc.kill();
  }
}

export async function testit(): Promise<void> {
  const dir = makeFolder();
  await testTocCompletes(dir);
  await testCloseDuringBuild(dir);
  console.log("issue-5918: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
