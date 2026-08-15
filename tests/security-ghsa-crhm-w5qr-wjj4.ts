// Regression test for GHSA-crhm-w5qr-wjj4. If a restriction INI exists,
// policy initialization must start fully restricted and only add explicitly
// enabled permissions. Invalid policy files must also fail closed.

import { copyFileSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { cmdId, EXE, runStandalone, tmpPath } from "./util.ts";
import { enumWindows, getClassName, getWindowPid, sleep } from "./winapi.ts";
import { sendCommand, waitForFrame, killAndWait } from "./win-automation.ts";

const DLL = join(dirname(EXE), "libsumatrapdf.dll");

async function assertFileDialog(name: string, ini: string, expected: boolean): Promise<void> {
  const appDir = tmpPath(`ghsa-crhm-w5qr-wjj4-${name}`);
  rmSync(appDir, { recursive: true, force: true });
  mkdirSync(appDir, { recursive: true });

  const testExe = join(appDir, "SumatraPDF.exe");
  copyFileSync(EXE, testExe);
  copyFileSync(DLL, join(appDir, basename(DLL)));
  writeFileSync(join(appDir, "sumatrapdfrestrict.ini"), ini);

  const proc = Bun.spawn([testExe, "-for-testing"], {
    cwd: appDir,
    stdout: "ignore",
    stderr: "ignore",
  });
  try {
    const frame = await waitForFrame(proc.pid);
    if (!frame) {
      throw new Error(`${name}: SumatraPDF window did not appear`);
    }
    sendCommand(frame, cmdId("CmdOpenFile"));
    const deadline = Date.now() + (expected ? 3000 : 250);
    let foundFileDialog = false;
    while (Date.now() < deadline) {
      enumWindows((hwnd) => {
        if (getWindowPid(hwnd) === proc.pid && getClassName(hwnd) === "#32770") {
          foundFileDialog = true;
          return false;
        }
        return true;
      });
      if (foundFileDialog) {
        break;
      }
      await sleep(30);
    }
    if (foundFileDialog !== expected) {
      throw new Error(`${name}: expected file dialog=${expected}, got ${foundFileDialog}`);
    }
  } finally {
    if (proc.exitCode === null) {
      await killAndWait(proc);
    }
  }
}

export async function testit(): Promise<void> {
  await assertFileDialog("allow-disk", "[Policies]\nDiskAccess = 1\n", true);
  await assertFileDialog(
    "deny-all",
    [
      "[Policies]",
      "InternetAccess = 0",
      "DiskAccess = 0",
      "SavePreferences = 0",
      "RegistryAccess = 0",
      "PrinterAccess = 0",
      "CopySelection = 0",
      "FullscreenAccess = 0",
      "",
    ].join("\n"),
    false,
  );
  await assertFileDialog("malformed", "[Poilcies]\nDiskAccess = 0\n", false);
}

if (import.meta.main) {
  await runStandalone(testit);
}
