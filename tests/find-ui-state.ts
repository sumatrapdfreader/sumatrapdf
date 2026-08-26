// Switching the global find-UI mode must migrate every visible find UI, not
// leave compact and floating variants mixed across multiple windows.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type FindUiState = {
  windows: number;
  docs: number;
  pref: number;
  compact: number;
  floating: number;
  firstTextLen: number;
  raw: string;
};

async function findUiRequest(client: ControlClient, action: string): Promise<FindUiState> {
  const response = await client.request(ControlCommand.TestFindUiState, [action]);
  const code = Number(response[0] ?? -1);
  const raw = String(response[1] ?? "");
  if (code !== 0) {
    throw new Error(`find-ui-state: ${action} failed (${code}): ${raw.trim()}`);
  }
  const values: Record<string, number> = {};
  for (const match of raw.matchAll(/(\w+)=(\d+)/g)) {
    values[match[1]] = Number(match[2]);
  }
  return {
    windows: values.windows ?? 0,
    docs: values.docs ?? 0,
    pref: values.pref ?? -1,
    compact: values.compact ?? 0,
    floating: values.floating ?? 0,
    firstTextLen: values.firstTextLen ?? -1,
    raw,
  };
}

async function waitForTwoDocuments(client: ControlClient): Promise<void> {
  const deadline = Date.now() + 8000;
  let state = await findUiRequest(client, "state");
  while ((state.windows !== 2 || state.docs !== 2) && Date.now() < deadline) {
    await Bun.sleep(40);
    state = await findUiRequest(client, "state");
  }
  if (state.windows !== 2 || state.docs !== 2) {
    throw new Error(`find-ui-state: duplicate window did not load: ${state.raw.trim()}`);
  }
}

function expectState(state: FindUiState, pref: number, compact: number, floating: number): void {
  if (state.pref !== pref || state.compact !== compact || state.floating !== floating) {
    throw new Error(
      `find-ui-state: expected pref=${pref} compact=${compact} floating=${floating}, got ${state.raw.trim()}`,
    );
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("find-ui-state");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nSearchUIFloating = false\n",
  );

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommandSync(frame, cmdId("CmdDuplicateInNewWindow"));
    await waitForTwoDocuments(client);

    expectState(await findUiRequest(client, "show-all"), 0, 2, 0);
    expectState(await findUiRequest(client, "toggle-first"), 1, 0, 2);

    let state = await findUiRequest(client, "set-first-text");
    if (state.firstTextLen !== "stale-term".length) {
      throw new Error(`find-ui-state: failed to seed floating term: ${state.raw.trim()}`);
    }
    await findUiRequest(client, "toggle-first");
    state = await findUiRequest(client, "clear-first");
    if (state.firstTextLen !== 0) {
      throw new Error(`find-ui-state: failed to clear compact term: ${state.raw.trim()}`);
    }
    state = await findUiRequest(client, "toggle-first");
    if (state.firstTextLen !== 0) {
      throw new Error(`find-ui-state: empty term was replaced after switching: ${state.raw.trim()}`);
    }

    // Return to compact mode, hide the first bar, and recreate it through the
    // same path used by theme changes. Its hidden term still backs F3.
    expectState(await findUiRequest(client, "toggle-first"), 0, 2, 0);
    state = await findUiRequest(client, "set-first-text");
    if (state.firstTextLen !== "stale-term".length) {
      throw new Error(`find-ui-state: failed to seed hidden-term test: ${state.raw.trim()}`);
    }
    expectState(await findUiRequest(client, "hide-first"), 0, 1, 0);
    state = await findUiRequest(client, "theme-recreate-first");
    if (state.firstTextLen !== "stale-term".length) {
      throw new Error(`find-ui-state: hidden term was lost during theme recreation: ${state.raw.trim()}`);
    }
    console.log("find-ui-state: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
