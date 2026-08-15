// #5907: with RememberStatePerDocument = false the home page's list of recently
// opened documents came up empty after a restart. The two settings mean
// different things: RememberStatePerDocument = false only drops the per-document
// state (page, zoom, scroll, window placement), while the list of files is what
// RememberOpenedFiles controls. The #5899 fix conflated them and dropped every
// FileState without a favorite in both cases.
//
// Asserted against the saved settings file: opening a document with
// RememberStatePerDocument = false still writes its FilePath, and does not write
// the per-document state fields.
import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone, tmpPath } from "./util";
import { postMessage, WM_CLOSE } from "./winapi";
import { launchControlled, waitForExit, killAndWait } from "./win-automation";

function makeAppDir(name: string, settings: string): string {
  const dir = tmpPath(`issue-5907-${name}`);
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    `UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\n${settings}`,
  );
  return dir;
}

// the FileStates [ ... ] block of the saved settings
function readFileStates(dir: string): string {
  const s = readFileSync(join(dir, "SumatraPDF-settings.txt"), "utf8");
  const start = s.indexOf("\nFileStates [");
  if (start < 0) {
    throw new Error("issue-5907: no FileStates section in saved settings");
  }
  const end = s.indexOf("\n]", start);
  return s.slice(start + 1, end + 2);
}

async function openAndClose(dir: string): Promise<void> {
  const pdf = join(ROOT, "tests", "issue-5597.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf], { saveSettings: true });
  try {
    await client.waitForRenderIdle();
    postMessage(frame, WM_CLOSE, 0, 0);
    if (!(await waitForExit(proc))) {
      throw new Error("issue-5907: SumatraPDF didn't exit after WM_CLOSE");
    }
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

export async function testit(): Promise<void> {
  // the reported case: per-document state off, file history on
  const dir = makeAppDir("no-per-doc-state", `RememberOpenedFiles = true\nRememberStatePerDocument = false\n`);
  await openAndClose(dir);
  const states = readFileStates(dir);
  if (!states.includes("issue-5597.pdf")) {
    throw new Error(`issue-5907: opened file missing from history with RememberStatePerDocument = false:\n${states}`);
  }
  // ... but the per-document state itself must still be dropped
  if (states.includes("ScrollPos") || states.includes("PageNo")) {
    throw new Error(`issue-5907: per-document state saved with RememberStatePerDocument = false:\n${states}`);
  }

  // the #5899 case must keep working: no history at all
  const dir2 = makeAppDir("no-history", `RememberOpenedFiles = false\nRememberStatePerDocument = false\n`);
  await openAndClose(dir2);
  const states2 = readFileStates(dir2);
  if (states2.includes("FilePath")) {
    throw new Error(`issue-5907: file saved with RememberOpenedFiles = false:\n${states2}`);
  }

  console.log("issue-5907: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
