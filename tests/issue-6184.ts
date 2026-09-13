// #6184: a keyboard shortcut bound to one of the "open with <known viewer>"
// commands did nothing. Every Shortcuts entry gets its own command id (a clone
// whose origId is the real command), but FrameOnCommand checked for an
// "open with <known viewer>" command *before* mapping the clone back to its
// origId, so the accelerator fell through to the switch, which has no case for
// those commands.
//
// The test stands in for an installed viewer: it points the HKCU "App Paths"
// entry PDF-XChange is detected by at a .cmd that writes a marker file, binds
// F4 to CmdOpenWithPdfXchange and presses it.
import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone, tmpPath } from "./util";
import { findCanvas, killAndWait, launchControlled, pressKey } from "./win-automation";
import { sleep, VK_F4 } from "./winapi";

const APP_PATHS = String.raw`Software\Microsoft\Windows\CurrentVersion\App Paths\PXCEditor.exe`;

function reg(args: string[]): { ok: boolean; out: string } {
  const res = Bun.spawnSync(["reg", ...args], { stdout: "pipe", stderr: "pipe" });
  return { ok: res.exitCode === 0, out: String(res.stdout) };
}

// a real PDF-XChange install is found before our HKCU stand-in, which would
// launch it for real; skip rather than do that
function realPdfXchangeInstalled(): boolean {
  const keys = [
    String.raw`HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\PXCEditor.exe`,
    String.raw`HKLM\Software\Microsoft\Windows\CurrentVersion\App Paths\PDFXEdit.exe`,
    String.raw`HKCR\PXCEditor.PDF\shell\open\command`,
    String.raw`HKCR\PDFXEdit.PDF\shell\open\command`,
  ];
  return keys.some((k) => reg(["query", k]).ok);
}

function settings(): string {
  return `UiLanguage = en
CheckForUpdates = false
RestoreSession = false
RememberOpenedFiles = false
Shortcuts [
\t[
\t\tCmd = CmdOpenWithPdfXchange
\t\tKey = F4
\t]
]
`;
}

export async function testit(): Promise<void> {
  if (realPdfXchangeInstalled()) {
    console.log("skipping: PDF-XChange is installed, the test would launch it for real");
    return;
  }

  const dir = tmpPath("issue-6184");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });

  const marker = join(dir, "launched.txt");
  const viewer = join(dir, "viewer.cmd");
  writeFileSync(viewer, `@echo off\r\necho launched> "${marker}"\r\n`);
  writeFileSync(join(dir, "SumatraPDF-settings.txt"), settings());

  if (!reg(["add", `HKCU\\${APP_PATHS}`, "/ve", "/t", "REG_SZ", "/d", viewer, "/f"]).ok) {
    throw new Error("failed to register the stand-in viewer in HKCU");
  }

  const pdf = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const { proc, client, frame } = await launchControlled(["-appdata", dir, pdf]);
  try {
    await client.waitForRenderIdle();
    await pressKey(findCanvas(frame), VK_F4);

    const deadline = Date.now() + 5000;
    while (!existsSync(marker) && Date.now() < deadline) {
      await sleep(100);
    }
    if (!existsSync(marker)) {
      throw new Error("F4 bound to CmdOpenWithPdfXchange didn't launch the external viewer");
    }
  } finally {
    client.close();
    await killAndWait(proc);
    reg(["delete", `HKCU\\${APP_PATHS}`, "/f"]);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
