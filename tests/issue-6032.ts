// #6032: Bookmarks/Favorites header close button must follow the frame DPI
// (it stayed at the previous monitor's size after a DPI change).
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type DpiState = {
  frame: number;
  tocClose: number;
  favClose: number;
  raw: string;
};

async function dpiRequest(client: ControlClient, action: string): Promise<string> {
  const response = await client.request(ControlCommand.TestDpi, [action]);
  const code = Number(response[0] ?? -1);
  const raw = String(response[1] ?? "");
  if (code !== 0) {
    throw new Error(`issue-6032: TestDpi ${action} failed (${code}): ${raw.trim()}`);
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
    tocClose: values.tocClose ?? 0,
    favClose: values.favClose ?? 0,
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
    throw new Error(`issue-6032: DPI state did not settle at ${expectedDpi}: ${state.raw.trim()}`);
  }
  return state;
}

function requireShrank(name: string, high: number, low: number): void {
  if (high <= 0 || low <= 0 || high < low * 1.3) {
    throw new Error(`issue-6032: ${name} did not follow 150% -> 75% DPI (${high} -> ${low})`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6032");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "SumatraPDF-settings.txt"),
    "UiLanguage = en\nCheckForUpdates = false\nRestoreSession = false\nShowToc = true\nShowFavorites = true\n",
  );

  const { proc, client, frame } = await launchControlled(["-appdata", dir]);
  try {
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // system -> 125%
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 125% -> 150%
    const high = await waitForDpiState(client, 144, (s) => s.tocClose > 0 && s.favClose > 0);
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 150% -> 75%
    const low = await waitForDpiState(client, 72, (s) => s.tocClose > 0 && s.favClose > 0);
    requireShrank("Bookmarks close", high.tocClose, low.tocClose);
    requireShrank("Favorites close", high.favClose, low.favClose);
    console.log("issue-6032: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
