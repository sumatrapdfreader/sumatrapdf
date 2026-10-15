// Typing into the Command Palette filters it. Keys are posted as WM_KEYDOWN, so
// the message loop translates them to WM_CHAR like real typing (WM_SETTEXT,
// which other palette tests use, skips that path).
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient } from "./control.ts";
import { ROOT, cmdId, runStandalone, tmpPath } from "./util.ts";
import { postMessage, sleep, WM_KEYDOWN, WM_KEYUP } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand, waitForPalette, paletteState } from "./win-automation.ts";

const VK_BACK = 0x08;
// repeat count 1; key up also sets the previous-state and transition bits, or
// TranslateMessage makes a second WM_CHAR from it
const KEYDOWN_LPARAM = 1;
const KEYUP_LPARAM = 0xc0000001;

function pressKey(hwnd: number, vk: number): void {
  postMessage(hwnd, WM_KEYDOWN, vk, KEYDOWN_LPARAM);
  postMessage(hwnd, WM_KEYUP, vk, KEYUP_LPARAM);
}

const SETTINGS = `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

async function queryLen(client: ControlClient): Promise<number> {
  const s = await paletteState(client);
  return s.open ? s.queryLen : -1;
}

async function waitQueryLen(client: ControlClient, want: number): Promise<number> {
  const deadline = Date.now() + 3_000;
  let got = -1;
  while (Date.now() < deadline) {
    got = await queryLen(client);
    if (got === want) {
      break;
    }
    await sleep(40);
  }
  return got;
}

export async function testit(): Promise<void> {
  const appdata = tmpPath("command-palette-typing");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    sendCommand(frame, cmdId("CmdCommandPalette"));

    const { edit } = await waitForPalette(client, frame);
    if (!edit) {
      throw new Error("command-palette-typing: palette did not open");
    }
    const start = await queryLen(client);

    for (const ch of "ZOOM") {
      pressKey(edit, ch.charCodeAt(0));
    }
    let got = await waitQueryLen(client, start + 4);
    if (got !== start + 4) {
      throw new Error(`command-palette-typing: typed 4 chars, queryLen ${start} -> ${got}`);
    }

    pressKey(edit, VK_BACK);
    got = await waitQueryLen(client, start + 3);
    if (got !== start + 3) {
      throw new Error(`command-palette-typing: Backspace left queryLen ${got}, want ${start + 3}`);
    }
    console.log("command-palette-typing: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
