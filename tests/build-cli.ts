import { join } from "node:path";
import { ROOT } from "./util.ts";
import { outDirName, resolveSources, type BuildFlags } from "../cmd/helper/ng-compile";
import { forPlatform, targets } from "../cmd/helper/ng-targets";
import type { Platform } from "../cmd/helper/ng-toolchain";
import { isShared, sharedFiles } from "../cmd/helper/ng-shared";
import { existsSync } from "node:fs";

function run(args: string[], script = "build.ts"): { code: number; stdout: string; stderr: string } {
  const proc = Bun.spawnSync(["bun", join(ROOT, "cmd", script), ...args], { cwd: ROOT });
  return {
    code: proc.exitCode,
    stdout: proc.stdout.toString(),
    stderr: proc.stderr.toString(),
  };
}

function check(condition: boolean, message: string): void {
  if (!condition) throw new Error(message);
}

export async function testit(): Promise<void> {
  const noArgs = run([]);
  check(noArgs.code === 0, `no-argument exit code: ${noArgs.code}`);
  check(noArgs.stdout.includes("Usage: bun cmd/build.ts"), "no-argument invocation did not print usage");

  const unknown = run(["-not-a-build-option"]);
  check(unknown.code !== 0, "unknown option succeeded");
  check(unknown.stderr.includes("unknown option: -not-a-build-option"), "unknown option did not print an error");
  check(unknown.stderr.includes("Usage: bun cmd/build.ts"), "unknown option did not print usage");

  const conflict = run(["-dbg", "-rel"]);
  check(conflict.code !== 0, "conflicting configurations succeeded");
  check(conflict.stderr.includes("cannot be used together"), "configuration conflict did not explain the error");

  for (const flag of ["-mingw", "-wine", "-win"]) {
    const removed = run([flag]);
    check(removed.code !== 0 && removed.stderr.includes("unknown option"), `${flag} is still supported`);
  }

  const ngHelp = run([], "ng-build.ts");
  check(ngHelp.code === 0 && ngHelp.stdout.includes("Usage: bun cmd/ng-build.ts"), "ng did not print usage");
  const ngUnknown = run(["-unknown"], "ng-build.ts");
  check(ngUnknown.code !== 0 && ngUnknown.stderr.includes("unknown option"), "ng accepted an unknown option");
  const ngConflict = run(["-dbg", "-rel"], "ng-build.ts");
  check(
    ngConflict.code !== 0 && ngConflict.stderr.includes("cannot be used together"),
    "ng accepted conflicting configs",
  );

  const flags: BuildFlags = { debug: true, asan: false, clang: false, clean: false, verbose: false };
  for (const plat of ["win", "mac", "linux", "wasm"] as Platform[]) {
    check(outDirName(plat, flags) === `${plat}/dbg`, `wrong ng debug directory for ${plat}`);
    check(outDirName(plat, { ...flags, debug: false }) === `${plat}/rel`, `wrong ng release directory for ${plat}`);
    check(outDirName(plat, { ...flags, asan: true }) === `${plat}/dbg-asan`, `wrong ng ASan directory for ${plat}`);
  }
  const fail = (msg: string): never => {
    throw new Error(msg);
  };
  for (const target of targets) {
    for (const plat of ["win", "mac", "linux", "wasm"] as Platform[]) {
      if (target.platforms && !target.platforms.includes(plat)) continue;
      for (const src of resolveSources(forPlatform(target, plat), plat, fail)) {
        check(
          !src.startsWith("src/") || src.startsWith("src/ng/") || isShared(src),
          `${target.name} uses unlisted originals`,
        );
      }
    }
  }
  for (const src of sharedFiles) {
    check(existsSync(join(ROOT, src)), `shared source missing: ${src}`);
    check(!existsSync(join(ROOT, src.replace(/^src\//, "src/ng/"))), `shared source still duplicated: ${src}`);
  }
  console.log("PASS: unified build CLI validation");
}

if (import.meta.main) {
  try {
    await testit();
  } catch (error) {
    console.error(`❌ ${error instanceof Error ? error.message : error}`);
    process.exitCode = 1;
  }
}
