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
import { ROOT, cmdId, runStandalone, tmpPath } from "./util";
import { postMessage, WM_CLOSE } from "./winapi";
import {
  launchControlled,
  pressKey,
  sendCommand,
  typeIntoInput,
  waitForExit,
  waitForFocusClass,
  killAndWait,
} from "./win-automation";

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

async function run(dir: string, act: (frame: number) => Promise<void>): Promise<void> {
  const pdf = join(ROOT, "tests", "issue-5597.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    await act(frame);
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
  await run(searchDir, async (frame) => {
    sendCommand(frame, cmdId("CmdFindFirst"));
    const edit = await waitForFocusClass(frame, "Edit");
    await typeIntoInput(edit, "CAF", false);
    await pressKey(edit, VK_RETURN);
  });
  const afterSearch = readFileStates(searchDir);
  if (afterSearch.includes("FilePath")) {
    throw new Error(`issue-5899: searching saved a FileState with RememberOpenedFiles = false:\n${afterSearch}`);
  }

  const favDir = makeAppDir("favorite");
  await run(favDir, async (frame) => {
    sendCommand(frame, cmdId("CmdFavoriteAdd"));
    await pressKey(await waitForFocusClass(frame, "Edit"), VK_RETURN);
  });
  const afterFav = readFileStates(favDir);
  if (!afterFav.includes("FilePath") || !afterFav.includes("PageNo")) {
    throw new Error(`issue-5899: an explicitly added favorite was not saved:\n${afterFav}`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
