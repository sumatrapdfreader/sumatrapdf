// #1136: keyboard navigation of the home page file list.
//  - arrows move a selection, drawn with a light blue outline
//  - Enter opens the selected file
//  - the first entry is selected at startup
//  - Up from the first row moves focus to the search box, Down there comes back
//  - filtering re-selects the first entry
//
// Everything is asserted through observable effects (which document opens, which
// control has focus) rather than pixels, so it doesn't depend on the drawing.
import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { getClassName, getFocusedHwnd, postMessage, sleep, waitForTopWindow, waitForWindowIdle } from "./winapi";
import { findCanvas, FRAME_CLASS, sendCommand, waitForFocusClass, waitForTitle, windowPosArgs } from "./win-automation";

const WM_KEYDOWN = 0x0100;
const WM_CHAR = 0x0102;
const VK_RETURN = 0x0d;
const VK_UP = 0x26;
const VK_RIGHT = 0x27;
const VK_DOWN = 0x28;
const CANVAS_CLASS = "SUMATRA_PDF_CANVAS";
const nFiles = 6;

function makeAppDir(name: string): string {
  const dir = tmpPath(`issue-1136-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(join(dir, "sub"), { recursive: true });
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const states: string[] = [];
  for (let i = 0; i < nFiles; i++) {
    const p = join(dir, "sub", `doc-${String(i).padStart(2, "0")}.pdf`);
    copyFileSync(src, p);
    states.push(`\t[\n\t\tFilePath = ${p}\n\t\tOpenCount = ${nFiles - i}\n\t]`);
  }
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nRememberOpenedFiles = true\n` +
      `HomePageViewMode = thumbnails\nFileStates [\n${states.join("\n")}\n]\n`,
  );
  return dir;
}

// send a key to whatever has focus, the way Windows delivers it
async function key(frame: number, vk: number): Promise<void> {
  postMessage(getFocusedHwnd(frame) || frame, WM_KEYDOWN, vk, 0);
  await sleep(500);
}

async function withHomePage(name: string, fn: (frame: number) => Promise<void>): Promise<void> {
  const proc = Bun.spawn([EXE, "-for-testing", ...windowPosArgs(), "-appdata", makeAppDir(name)], {
    stdout: "ignore",
    stderr: "ignore",
  });
  try {
    const frame = await waitForTopWindow(proc.pid, FRAME_CLASS);
    if (!frame) {
      throw new Error("no frame window");
    }
    const canvas = findCanvas(frame);
    await waitForWindowIdle(canvas || frame, 8000, 200);
    await fn(frame);
  } finally {
    proc.kill();
    await proc.exited;
  }
}

export async function testit(): Promise<void> {
  // arrows move the selection and Enter opens it. The first entry is selected
  // at startup, so two Rights land on the third document
  await withHomePage("enter", async (frame) => {
    await key(frame, VK_RIGHT);
    await key(frame, VK_RIGHT);
    await key(frame, VK_RETURN);
    const title = await waitForTitle(frame, (t) => t.includes("doc-02.pdf"));
    if (!title.includes("doc-02.pdf")) {
      throw new Error(`Enter did not open the selected file, title: '${title}'`);
    }
  });

  // Up from the first row goes to the search box, Down there comes back to the
  // list; then filtering re-selects the first (only) match, so Enter opens it
  await withHomePage("search", async (frame) => {
    await key(frame, VK_UP);
    let focus = getFocusedHwnd(frame);
    let cls = focus ? getClassName(focus) : "?";
    if (cls !== "Edit") {
      throw new Error(`Up on the first row should focus the search box, focus is '${cls}'`);
    }
    await key(frame, VK_DOWN);
    focus = getFocusedHwnd(frame);
    cls = focus ? getClassName(focus) : "?";
    if (cls !== CANVAS_CLASS) {
      throw new Error(`Down in the search box should focus the list, focus is '${cls}'`);
    }

    // move off the first entry, then filter down to a single different file:
    // the selection must reset to it
    await key(frame, VK_RIGHT);
    sendCommand(frame, cmdId("CmdFindFirst")); // focuses the home search box
    const edit = await waitForFocusClass(frame, "Edit");
    for (const ch of "doc-04") {
      postMessage(edit, WM_CHAR, ch.charCodeAt(0), 0);
      await sleep(40);
    }
    await sleep(200);
    await key(frame, VK_DOWN); // search box -> the filtered list
    await key(frame, VK_RETURN);
    const title = await waitForTitle(frame, (t) => t.includes("doc-04.pdf"));
    if (!title.includes("doc-04.pdf")) {
      throw new Error(`filtering should re-select the first match, opened title: '${title}'`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
