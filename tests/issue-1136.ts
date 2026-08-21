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
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { sendMessage, setCursorPos, sleep } from "./winapi";
import {
  findCanvas,
  findChildByClass,
  launchControlled,
  sendCommand,
  waitForTitle,
  killAndWait,
} from "./win-automation";
import type { ControlClient, HomeSelection } from "./control.ts";

const WM_KEYDOWN = 0x0100;
const WM_CHAR = 0x0102;
const VK_RETURN = 0x0d;
const VK_UP = 0x26;
const VK_RIGHT = 0x27;
const VK_DOWN = 0x28;
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

// Target the control that owns the behavior and finish handling one key before
// checking its effect. Looking up focus and then posting left a race where focus
// changed before the queued key arrived, so the key was silently lost.
function key(target: number, vk: number): void {
  sendMessage(target, WM_KEYDOWN, vk, 0);
}

// Waits for the home page to report the state a key was supposed to produce.
// Polling the app beats sleeping after each key: a key posted while the focus
// is still moving lands on the wrong window and is silently lost, which is what
// made this test flaky ("focus did not move", or the wrong file opened).
async function waitForHome(
  client: ControlClient,
  pred: (h: HomeSelection) => boolean,
  what: string,
  timeoutMs = 8000,
): Promise<HomeSelection> {
  const deadline = Date.now() + timeoutMs;
  let last: HomeSelection | null = null;
  for (;;) {
    last = await client.homeSelection();
    if (last.ready && pred(last)) {
      return last;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-1136: ${what} (last: ${last.raw})`);
    }
    await sleep(50);
  }
}

async function withHomePage(
  name: string,
  fn: (frame: number, canvas: number, searchEdit: number, client: ControlClient) => Promise<void>,
): Promise<void> {
  // CI uses the whole work area, so a cursor left over the thumbnails changes
  // the keyboard selection through hover before the first test key.
  setCursorPos(0, 0);
  const { proc, client, frame } = await launchControlled(["-appdata", makeAppDir(name)]);
  try {
    const canvas = findCanvas(frame);
    if (!canvas) {
      throw new Error("issue-1136: home-page canvas not found");
    }
    // the first entry is selected once the home page has laid out; the search
    // box is created in the same pass and Up needs it to exist
    await waitForHome(
      client,
      (h) => h.entries === nFiles && h.searchBox && h.sel === 0 && !h.searchFocus,
      "home page never reached its initial selection",
    );
    const searchEdit = findChildByClass(canvas, "Edit");
    if (!searchEdit) {
      throw new Error("issue-1136: home-page search edit not found");
    }
    await fn(frame, canvas, searchEdit, client);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  // arrows move the selection and Enter opens it. The first entry is selected
  // at startup, so two Rights land on the third document
  await withHomePage("enter", async (frame, canvas, _searchEdit, client) => {
    key(canvas, VK_RIGHT);
    await waitForHome(client, (h) => h.sel === 1, "Right did not move the selection to the second entry");
    key(canvas, VK_RIGHT);
    await waitForHome(client, (h) => h.sel === 2, "Right did not move the selection to the third entry");
    key(canvas, VK_RETURN);
    const title = await waitForTitle(frame, (t) => t.includes("doc-02.pdf"));
    if (!title.includes("doc-02.pdf")) {
      throw new Error(`Enter did not open the selected file, title: '${title}'`);
    }
  });

  // Up from the first row goes to the search box, Down there comes back to the
  // list; then filtering re-selects the first (only) match, so Enter opens it
  await withHomePage("search", async (frame, canvas, searchEdit, client) => {
    key(canvas, VK_UP);
    await waitForHome(client, (h) => h.searchFocus, "Up from the first row did not focus the search box");
    key(searchEdit, VK_DOWN);
    await waitForHome(client, (h) => !h.searchFocus, "Down did not move the focus back to the list");

    // move off the first entry, then filter down to a single different file:
    // the selection must reset to it
    key(canvas, VK_RIGHT);
    await waitForHome(client, (h) => h.sel === 1, "Right did not move the selection off the first entry");
    sendCommand(frame, cmdId("CmdFindFirst")); // focuses the home search box
    await waitForHome(client, (h) => h.searchFocus, "CmdFindFirst did not focus the search box");
    for (const ch of "doc-04") {
      sendMessage(searchEdit, WM_CHAR, ch.charCodeAt(0), 0);
    }
    await waitForHome(
      client,
      (h) => h.entries === 1 && h.path.includes("doc-04.pdf"),
      "typing in the search box did not filter down to doc-04",
    );
    key(searchEdit, VK_DOWN); // search box -> the filtered list
    await waitForHome(client, (h) => !h.searchFocus, "Down did not move the focus to the filtered list");
    key(canvas, VK_RETURN);
    const title = await waitForTitle(frame, (t) => t.includes("doc-04.pdf"));
    if (!title.includes("doc-04.pdf")) {
      throw new Error(`filtering should re-select the first match, opened title: '${title}'`);
    }
  });
}

if (import.meta.main) {
  await runStandalone(testit);
}
