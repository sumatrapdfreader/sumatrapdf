// Mouse side buttons (XButton1 / XButton2) can be bound in Shortcuts, also with
// modifiers. An unbound side button still goes back / forward.
//
// Run: bun tests/mouse-button-shortcuts.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import type { ControlClient } from "./control.ts";
import { assemblePdf, runStandalone, tmpPath } from "./util.ts";
import { packCoords, postMessage, sleep } from "./winapi.ts";
import { findCanvas, killAndWait, launchControlled } from "./win-automation.ts";

const kPages = 10;
const kWaitMs = 5000;
const WM_XBUTTONUP = 0x020c;
const WM_APPCOMMAND = 0x0319;
const XBUTTON2 = 2;
const MK_CONTROL = 0x8;
const APPCOMMAND_BROWSER_BACKWARD = 1;
const FAPPCOMMAND_MOUSE = 0x8000;

const kSettings = `RestoreSession = false
CheckForUpdates = false
Shortcuts [
    [
        Cmd = CmdGoToNextPage
        Key = XButton2
    ]
    [
        Cmd = CmdGoToLastPage
        Key = Ctrl + XButton1
    ]
]
`;

function makePdf(): string {
  const objs: string[] = ["<< /Type /Catalog /Pages 2 0 R >>"];
  const kids = Array.from({ length: kPages }, (_, i) => `${i + 3} 0 R`).join(" ");
  objs.push(`<< /Type /Pages /Count ${kPages} /Kids [${kids}] >>`);
  for (let i = 0; i < kPages; i++) {
    objs.push("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>");
  }
  return assemblePdf(objs);
}

async function waitForPage(client: ControlClient, want: number, what: string): Promise<void> {
  const deadline = Date.now() + kWaitMs;
  let page = 0;
  while (Date.now() < deadline) {
    page = (await client.chapterInfo()).page;
    if (page === want) {
      return;
    }
    await sleep(50);
  }
  throw new Error(`mouse-button-shortcuts: ${what}: on page ${page}, want ${want}`);
}

// WM_APPCOMMAND the way DefWindowProc makes it from a side button release
function sideButtonAppCommand(frame: number, keys: number): void {
  const lp = ((APPCOMMAND_BROWSER_BACKWARD | FAPPCOMMAND_MOUSE) << 16) | keys;
  postMessage(frame, WM_APPCOMMAND, frame, lp);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("mouse-button-shortcuts");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const pdf = join(dir, "pages.pdf");
  writeFileSync(pdf, makePdf(), "latin1");
  const appdata = join(dir, "appdata");
  mkdirSync(appdata, { recursive: true });
  writeFileSync(join(appdata, "SumatraPDF-settings.txt"), kSettings);

  const { proc, client, frame } = await launchControlled(["-appdata", appdata, pdf]);
  try {
    await client.waitForRenderIdle();
    await waitForPage(client, 1, "start");

    // a real release of XButton2 over the page: DefWindowProc turns it into WM_APPCOMMAND
    postMessage(findCanvas(frame), WM_XBUTTONUP, XBUTTON2 << 16, packCoords(100, 100));
    await waitForPage(client, 2, "XButton2 bound to CmdGoToNextPage");

    sideButtonAppCommand(frame, MK_CONTROL);
    await waitForPage(client, kPages, "Ctrl + XButton1 bound to CmdGoToLastPage");

    // plain XButton1 isn't bound: still Back
    sideButtonAppCommand(frame, 0);
    await waitForPage(client, 2, "unbound XButton1 goes back");
    console.log("mouse-button-shortcuts: OK");
  } finally {
    client.close();
    await killAndWait(proc);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
