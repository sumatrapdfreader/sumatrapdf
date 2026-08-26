// Compact find bar must follow DPI in place. RecreateFindBar on WM_DPICHANGED
// heap-corrupted when a nested 96/120 change arrived during DestroyWindow
// (DameWare / RDP).
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, type ControlClient } from "./control.ts";
import { cmdId, ROOT, runStandalone, tmpPath } from "./util.ts";
import { killAndWait, launchControlled, sendCommandSync } from "./win-automation.ts";

type DpiState = {
  frame: number;
  find: number;
  raw: string;
};

async function dpiRequest(client: ControlClient, action: string): Promise<string> {
  const response = await client.request(ControlCommand.TestDpi, [action]);
  const code = Number(response[0] ?? -1);
  const raw = String(response[1] ?? "");
  if (code !== 0) {
    throw new Error(`find-bar-dpi: TestDpi ${action} failed (${code}): ${raw.trim()}`);
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
    throw new Error(`find-bar-dpi: DPI state did not settle at ${expectedDpi}: ${state.raw.trim()}`);
  }
  return state;
}

function requireShrank(name: string, high: number, low: number): void {
  if (high <= 0 || low <= 0 || high < low * 1.3) {
    throw new Error(`find-bar-dpi: ${name} did not follow 150% -> 75% DPI (${high} -> ${low})`);
  }
}

export async function testit(): Promise<void> {
  const dir = tmpPath("find-bar-dpi");
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
    sendCommandSync(frame, cmdId("CmdFindFirst"));

    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // system -> 125%
    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 125% -> 150%
    const high = await waitForDpiState(client, 144, (s) => s.find > 0);

    sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride")); // 150% -> 75%
    const low = await waitForDpiState(client, 72, (s) => s.find > 0);
    requireShrank("compact Find bar", high.find, low.find);

    // Oscillate the way remote-desktop DPI storms do: many WM_DPICHANGED
    // deliveries, including nested ones from the find bar's own popup.
    for (let i = 0; i < 12; i++) {
      sendCommandSync(frame, cmdId("CmdDebugToggleDpiOverride"));
    }
    const after = await dpiState(client);
    if (after.find <= 0) {
      throw new Error(`find-bar-dpi: compact find bar gone after DPI oscillation: ${after.raw.trim()}`);
    }
    console.log("find-bar-dpi: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
