// A command-line search for a PDF already being restored used to start a find
// worker before the restored tab had a document controller (crash 8cdef464).
// This test must not use -for-testing: that disables session save/restore.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommand, waitForExit } from "./win-automation.ts";

const SEARCH_TERM = "zlib";

async function exitCleanly(proc: Bun.Subprocess, frame: number, label: string): Promise<void> {
  sendCommand(frame, cmdId("CmdExit"));
  if (!(await waitForExit(proc, 8000))) {
    throw new Error(`session-restore-search: ${label} did not exit`);
  }
}

async function findWindowContents(client: ControlClient): Promise<string> {
  const deadline = Date.now() + 20_000;
  for (;;) {
    const res = await client.request(ControlCommand.TestFindWindowContents);
    const exitCode = res[0] as number;
    const text = ((res[1] as string) ?? "").trim();
    if (!text.includes("NOTREADY")) {
      if (exitCode !== 0) {
        throw new Error(`session-restore-search: ${text}`);
      }
      return text;
    }
    if (Date.now() > deadline) {
      throw new Error(`session-restore-search: deferred search never finished: ${text}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

export async function testit(): Promise<void> {
  const appData = tmpPath("session-restore-search-appdata");
  rmSync(appData, { recursive: true, force: true });
  mkdirSync(appData, { recursive: true });
  writeFileSync(
    join(appData, "SumatraPDF-settings.txt"),
    [
      "UiLanguage = en",
      "CheckForUpdates = false",
      "RestoreSession = true",
      "RememberOpenedFiles = true",
      "RememberStatePerDocument = true",
      "LazyLoading = false",
      "UseTabs = true",
      "ReuseInstance = false",
      "ShowStartPage = true",
      "",
    ].join("\n"),
  );

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const first = await launchControlled(["-appdata", appData, pdf], { saveSettings: true });
  try {
    await first.client.waitForRenderIdle(15_000);
    await exitCleanly(first.proc, first.frame, "initial session-saving run");
  } finally {
    first.client.close();
    await killAndWait(first.proc);
  }

  const restored = await launchControlled(["-appdata", appData, "-new-window", "-search", SEARCH_TERM, pdf], {
    saveSettings: true,
  });
  try {
    const result = await findWindowContents(restored.client);
    const match = /OK term=(\S+) n=(\d+) snippets=(\d+)/.exec(result);
    if (!match) {
      throw new Error(`session-restore-search: unexpected result: ${result}`);
    }
    if (match[1] !== SEARCH_TERM) {
      throw new Error(`session-restore-search: term=${match[1]} want ${SEARCH_TERM}`);
    }
    const count = parseInt(match[2]!, 10);
    const snippets = parseInt(match[3]!, 10);
    if (count < 1 || snippets !== count) {
      throw new Error(`session-restore-search: matches=${count} snippets=${snippets}`);
    }
    await exitCleanly(restored.proc, restored.frame, "restored search run");
  } finally {
    restored.client.close();
    await killAndWait(restored.proc);
    rmSync(appData, { recursive: true, force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
