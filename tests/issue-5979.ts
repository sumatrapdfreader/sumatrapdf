// #5979: UI created or retained on a high-DPI monitor must use the new
// monitor's fonts after the frame moves to a lower-DPI monitor.
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type DpiState = {
  frame: number;
  home: number;
  tocLabel: number;
  tocEdit: number;
  aiLabel: number;
  aiInput: number;
  aiCheckbox: number;
  find: number;
  raw: string;
};

async function dpiRequest(client: ControlClient, action: string): Promise<string> {
  const response = await client.request(ControlCommand.TestDpi, [action]);
  const code = Number(response[0] ?? -1);
  const raw = String(response[1] ?? "");
  if (code !== 0) {
    throw new Error(`issue-5979: TestDpi ${action} failed (${code}): ${raw.trim()}`);
  }
  return raw;
}

async function dpiState(client: ControlClient): Promise<DpiState> {
  const raw = await dpiRequest(client, "state");
  const values: Record<string, number> = {};
  for (const match of raw.matchAll(/(\w+)=(\d+)/g)) {
    values[match[1]] = Number(match[2]);
  }
  return {
    frame: values.frame ?? 0,
    home: values.home ?? 0,
    tocLabel: values.tocLabel ?? 0,
    tocEdit: values.tocEdit ?? 0,
    aiLabel: values.aiLabel ?? 0,
    aiInput: values.aiInput ?? 0,
    aiCheckbox: values.aiCheckbox ?? 0,
    find: values.find ?? 0,
    raw,
  };
}

async function waitForDpiState(
  client: ControlClient,
  expectedDpi: number,
  ready: (state: DpiState) => boolean,
): Promise<DpiState> {
  const deadline = Date.now() + 5000;
  let state = await dpiState(client);
  while ((state.frame !== expectedDpi || !ready(state)) && Date.now() < deadline) {
    await Bun.sleep(40);
    state = await dpiState(client);
  }
  if (state.frame !== expectedDpi || !ready(state)) {
    throw new Error(`issue-5979: DPI state did not settle at ${expectedDpi}: ${state.raw.trim()}`);
  }
  return state;
}

function requireShrank(name: string, high: number, low: number): void {
  // Bigger UI fonts have a 14px floor, so their 150% -> 75% ratio is about
  // 1.5 rather than 2.0. A stale font has ratio 1.0.
  if (high <= 0 || low <= 0 || high < low * 1.3) {
    throw new Error(`issue-5979: ${name} font did not follow 150% -> 75% DPI (${high} -> ${low})`);
  }
}

function toggleTo150(frame: number): void {
  sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // system -> 125%
  sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 125% -> 150%
}

function toggleTo75(frame: number): void {
  sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 150% -> 75%
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-5979");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowToc = true\nSearchUIFloating = true\n",
  );

  const home = await launchControlled(["-appdata", dir]);
  try {
    await dpiRequest(home.client, "hidden");
    toggleTo150(home.frame);
    const high = await waitForDpiState(home.client, 144, (s) => s.home > 0 && s.tocEdit > 0);
    toggleTo75(home.frame);
    const low = await waitForDpiState(home.client, 72, (s) => s.home > 0 && s.tocEdit > 0);

    requireShrank("Home search", high.home, low.home);
    requireShrank("Bookmarks label", high.tocLabel, low.tocLabel);
    requireShrank("Bookmarks search", high.tocEdit, low.tocEdit);
  } finally {
    home.client.close();
    await killAndWait(home.proc);
  }

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const find = await launchControlled(["-appdata", dir, pdf]);
  try {
    await find.client.waitForRenderIdle();
    toggleTo150(find.frame);
    sendCommandSync(find.frame, cmdId("CmdFindFirst"));
    // This creates the panel when Claude and WebView2 are available; the DPI
    // assertions below stay optional so hosted runners without them can pass.
    sendCommandSync(find.frame, cmdId("CmdAIChatWithClaudeCode"));
    const high = await waitForDpiState(find.client, 144, (s) => s.find > 0);
    toggleTo75(find.frame);
    const low = await waitForDpiState(find.client, 72, (s) => s.find > 0);
    requireShrank("floating Find", high.find, low.find);
    if (high.aiInput > 0 || low.aiInput > 0) {
      requireShrank("AI chat label", high.aiLabel, low.aiLabel);
      requireShrank("AI chat input", high.aiInput, low.aiInput);
      requireShrank("AI chat checkbox", high.aiCheckbox, low.aiCheckbox);
    }
  } finally {
    find.client.close();
    await killAndWait(find.proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
