// #5899: with RememberOpenedFiles = false, searching a document left a FileState
// entry in the settings file. Searching calls SetSearchStartFavorite(), which
// creates a FileState just to hang a session-only "jump back here" favorite on;
// the favorite itself was already skipped when serializing (#5862) but the empty
// FileState around it was still written.
//
// Two halves, both asserted against the saved settings file:
//  - searching writes no FileState
//  - a favorite the user added on purpose is still saved
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand } from "./control.ts";
import { ROOT, SLOW_BUILD_FACTOR, USE_NG, cmdId, runStandalone, tmpPath } from "./util";
import { postMessage, sendText, sleep, WM_CLOSE } from "./winapi";
import { launchControlled, pressKey, sendCommand, waitForExit, waitForFocusClass, killAndWait } from "./win-automation";
import type { ControlClient } from "./control.ts";

const VK_RETURN = 0x0d;

function makeAppDir(name: string): string {
  const dir = tmpPath(`issue-5899-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n` +
      `RememberOpenedFiles = false\nRememberStatePerDocument = false\n`,
  );
  return dir;
}

// the FileStates [ ... ] block of the saved settings
function readFileStates(dir: string): string {
  const s = readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
  const start = s.indexOf("\nFileStates [");
  if (start < 0) {
    throw new Error("issue-5899: no FileStates section in saved settings");
  }
  const end = s.indexOf("\n]", start);
  return s.slice(start + 1, end + 2);
}

async function ngFind(client: ControlClient, action: string, arg = ""): Promise<void> {
  const res = await client.request(ControlCommand.TestFindUiState, arg ? [action, arg] : [action]);
  if (res[0] !== 0) {
    throw new Error(`issue-5899: ${action} failed: ${String(res[1] ?? "")}`);
  }
}

async function waitFindOpen(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let last = "";
  for (;;) {
    last = String((await client.request(ControlCommand.TestFindUiState, ["state"]))[1] ?? "");
    if (/compact=1/.test(last) || /floating=1/.test(last)) {
      return;
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5899: find bar did not open (${last})`);
    }
    await sleep(40);
  }
}

// the name field is drawn in the dialog, so Enter goes to that window
async function confirmAddFavorite(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 4000 * SLOW_BUILD_FACTOR;
  let last = "";
  for (;;) {
    last = String((await client.request(ControlCommand.TestUiState))[1] ?? "");
    if (/dialog=1/.test(last)) {
      await client.request(ControlCommand.TestInput, ["key", VK_RETURN, 0]);
      await sleep(50);
      const after = String((await client.request(ControlCommand.TestUiState))[1] ?? "");
      if (!/dialog=1/.test(after)) {
        return;
      }
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-5899: Add Favorite dialog did not accept Enter (${last})`);
    }
    await sleep(40);
  }
}

async function run(dir: string, act: (frame: number, client: ControlClient) => Promise<void>): Promise<void> {
  const pdf = join(ROOT, "tests", "issue-5597.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await act(frame, client);
    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5899: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  const searchDir = makeAppDir("search");
  await run(searchDir, async (frame, client) => {
    sendCommand(frame, cmdId("CmdFindFirst"));
    if (USE_NG) {
      await waitFindOpen(client);
      await ngFind(client, "set", "CAF");
      await ngFind(client, "enter");
      return;
    }
    const edit = await waitForFocusClass(frame, "Edit");
    sendText(edit, "CAF");
    await pressKey(edit, VK_RETURN, 0);
  });
  const afterSearch = readFileStates(searchDir);
  if (afterSearch.includes("FilePath")) {
    throw new Error(`issue-5899: searching saved a FileState with RememberOpenedFiles = false:\n${afterSearch}`);
  }

  const favDir = makeAppDir("favorite");
  await run(favDir, async (frame, client) => {
    sendCommand(frame, cmdId("CmdFavoriteAdd"));
    if (USE_NG) {
      await confirmAddFavorite(client);
      return;
    }
    await pressKey(await waitForFocusClass(frame, "Edit"), VK_RETURN, 0);
  });
  const afterFav = readFileStates(favDir);
  if (!afterFav.includes("FilePath") || !afterFav.includes("PageNo")) {
    throw new Error(`issue-5899: an explicitly added favorite was not saved:\n${afterFav}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
