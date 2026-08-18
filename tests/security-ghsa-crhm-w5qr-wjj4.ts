// Regression test for GHSA-crhm-w5qr-wjj4. If a restriction INI exists,
// policy initialization must start fully restricted and only add explicitly
// enabled permissions. Invalid policy files must also fail closed.
//
// Reads the granted bits over -dbg-control. The old check watched for an
// IFileOpenDialog after CmdOpenFile; that dialog is often hosted out of
// process (or slower than a 3s poll on ASan), so CI saw "no dialog" even
// when DiskAccess was on.

import { copyFileSync, existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

// What the exe needs beside it once copied out of the build directory. The
// regular build loads the engine from libsumatrapdf.dll; the static (ASan) one
// has it linked in but needs the ASan runtime, so copy whichever is there.
const SIDE_BY_SIDE_DLLS = ["libsumatrapdf.dll", "clang_rt.asan_dynamic-x86_64.dll"].map((name) =>
  join(dirname(EXE), name),
);

type PolicyBits = {
  restricted: number;
  internet: number;
  disk: number;
  prefs: number;
  registry: number;
  printer: number;
  copy: number;
  fullscreen: number;
};

function parsePolicies(raw: string): PolicyBits {
  const n = (key: string): number => {
    const m = new RegExp(`^${key}=(\\d+)`, "m").exec(raw);
    if (!m) {
      throw new Error(`missing ${key} in policy dump:\n${raw}`);
    }
    return parseInt(m[1], 10);
  };
  return {
    restricted: n("restricted"),
    internet: n("internet"),
    disk: n("disk"),
    prefs: n("prefs"),
    registry: n("registry"),
    printer: n("printer"),
    copy: n("copy"),
    fullscreen: n("fullscreen"),
  };
}

async function assertPolicies(name: string, ini: string, want: Partial<PolicyBits>): Promise<void> {
  const appDir = tmpPath(`ghsa-crhm-w5qr-wjj4-${name}`);
  rmSync(appDir, { recursive: true, force: true });
  mkdirSync(appDir, { recursive: true });

  const testExe = join(appDir, "SumatraPDF.exe");
  copyFileSync(EXE, testExe);
  for (const dll of SIDE_BY_SIDE_DLLS) {
    if (existsSync(dll)) {
      copyFileSync(dll, join(appDir, basename(dll)));
    }
  }
  writeFileSync(join(appDir, "sumatrapdfrestrict.ini"), ini);

  await withControlledSumatra(
    testExe,
    async (client) => {
      const res = await client.request(ControlCommand.TestGetPolicies);
      const raw = String(res[1] ?? "");
      if (res[0] !== 0) {
        throw new Error(`${name}: TestGetPolicies failed: ${raw.trim()}`);
      }
      const got = parsePolicies(raw);
      for (const [key, expected] of Object.entries(want) as [keyof PolicyBits, number][]) {
        if (got[key] !== expected) {
          throw new Error(`${name}: expected ${key}=${expected}, got ${got[key]}\n${raw}`);
        }
      }
      console.log(`  ${name}: ${raw.trim().replace(/\n/g, " ")} ✓`);
    },
    [],
    { cwd: appDir },
  );
}

const kFullyClosed: Partial<PolicyBits> = {
  restricted: 1,
  internet: 0,
  disk: 0,
  prefs: 0,
  registry: 0,
  printer: 0,
  copy: 0,
  fullscreen: 0,
};

export async function testit(): Promise<void> {
  await assertPolicies("allow-disk", "[Policies]\nDiskAccess = 1\n", { ...kFullyClosed, disk: 1 });
  await assertPolicies(
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
    kFullyClosed,
  );
  await assertPolicies("malformed", "[Poilcies]\nDiskAccess = 0\n", kFullyClosed);
}

if (import.meta.main) {
  await runStandalone(testit);
}
