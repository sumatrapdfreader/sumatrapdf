// Switching the theme while the command palette is open repaints the palette
// in the new theme: its background goes dark and the bottom hint row keeps its
// text ("Enter run command", "Esc close").
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { cmdId, runStandalone, tmpPath } from "./util.ts";
import { captureWindowPixels, getClassName, getFocusedHwnd, getParentWindow, getRootWindow, sleep } from "./winapi.ts";
import { killAndWait, launchControlled, sendCommand, sendCommandSync } from "./win-automation.ts";

const SETTINGS = `UiLanguage = en
Theme = Light
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
`;

// height of the bottom strip holding the hint row
const kHintRowDy = 28;

type Look = { bgLuma: number; hintInk: number };

function luma(d: Uint8Array, i: number): number {
  return (d[i + 2]! * 299 + d[i + 1]! * 587 + d[i]! * 114) / 1000;
}

// bgLuma: brightness of the left padding column (window background)
// hintInk: pixels in the hint row that stand out from its background
function paletteLook(hwnd: number): Look | null {
  const px = captureWindowPixels(hwnd);
  if (!px) {
    return null;
  }
  const { w, h, data } = px;
  let sum = 0;
  for (let y = 0; y < h; y++) {
    sum += luma(data, (y * w + 3) * 4);
  }
  const bgLuma = sum / h;

  const y0 = Math.max(0, h - kHintRowDy);
  const bgHint = luma(data, ((h - 2) * w + 3) * 4);
  let hintInk = 0;
  for (let y = y0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      if (Math.abs(luma(data, (y * w + x) * 4) - bgHint) > 80) {
        hintInk++;
      }
    }
  }
  return { bgLuma, hintInk };
}

async function openPalette(frame: number): Promise<number> {
  sendCommand(frame, cmdId("CmdCommandPalette"));
  const deadline = Date.now() + 8_000;
  while (Date.now() < deadline) {
    const edit = getFocusedHwnd(frame);
    if (edit && getClassName(edit) === "Edit" && getRootWindow(edit) !== frame) {
      return getParentWindow(edit);
    }
    await sleep(50);
  }
  throw new Error("command-palette-theme: palette did not open");
}

async function waitForLook(hwnd: number, what: string, pred: (l: Look) => boolean): Promise<Look> {
  const deadline = Date.now() + 8_000;
  let last: Look | null = null;
  while (Date.now() < deadline) {
    last = paletteLook(hwnd);
    if (last && pred(last)) {
      return last;
    }
    await sleep(100);
  }
  throw new Error(`command-palette-theme: ${what}; palette look is ${JSON.stringify(last)}`);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("command-palette-theme");
  rmSync(dir, { recursive: true, force: true });
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), SETTINGS);

  const { proc, client, frame } = await launchControlled(["-appdata", appdata]);
  try {
    const palette = await openPalette(frame);
    const light = await waitForLook(palette, "palette is not light", (l) => l.bgLuma > 160 && l.hintInk > 20);

    sendCommandSync(frame, cmdId("CmdToggleLightDarkTheme"));
    const dark = await waitForLook(
      palette,
      "palette did not switch to the dark theme",
      (l) => l.bgLuma < 90 && l.hintInk > 20,
    );
    console.log(`command-palette-theme: OK light=${JSON.stringify(light)} dark=${JSON.stringify(dark)}`);
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
