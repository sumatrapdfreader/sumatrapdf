import { copyFileSync, existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

const asanDllName = "clang_rt.asan_dynamic-x86_64.dll";

type BuildKind = "dbg" | "dbg32" | "asan";

function usage(): never {
  throw new Error("usage: bun cmd/run-unit-tests.ts [-dbg | -32 | -asan]");
}

function parseArgs(): BuildKind {
  let kind: BuildKind = "dbg";
  let seen = false;
  for (const arg of process.argv.slice(2)) {
    if (arg === "-dbg") {
      if (seen) usage();
      kind = "dbg";
      seen = true;
    } else if (arg === "-32") {
      if (seen) usage();
      kind = "dbg32";
      seen = true;
    } else if (arg === "-asan") {
      if (seen) usage();
      kind = "asan";
      seen = true;
    } else {
      usage();
    }
  }
  return kind;
}

function findAsanDll(vsRoot: string): string {
  const candidates = [join(vsRoot, String.raw`VC\Tools\MSVC`), join(vsRoot, String.raw`VC\Tools\Llvm\x64\lib\clang`)];
  const walk = (dir: string): string | undefined => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = join(dir, entry.name);
      if (entry.isDirectory()) {
        const hit = walk(path);
        if (hit) return hit;
      } else if (entry.name === asanDllName) {
        return path;
      }
    }
    return;
  };
  for (const base of candidates) {
    if (!existsSync(base)) continue;
    const hit = walk(base);
    if (hit) return hit;
  }
  throw new Error(`could not find ${asanDllName} under ${vsRoot}`);
}

function copyAsanRuntime(vsRoot: string, outDir: string): void {
  const src = findAsanDll(vsRoot);
  const dst = join(outDir, asanDllName);
  copyFileSync(src, dst);
}

interface UnitTestConfig {
  platform: string;
  target: string;
  exeName: string;
  outDir: string;
  logName: string;
}

// unit tests are compiled into SumatraPDF only in Debug builds, so every
// config here is Debug
function configFor(kind: BuildKind): UnitTestConfig {
  if (kind === "dbg32") {
    return {
      platform: "Win32",
      target: "SumatraPDF",
      exeName: "SumatraPDF.exe",
      outDir: join("out", "dbg32"),
      logName: "unit-tests-dbg32.txt",
    };
  }
  if (kind === "asan") {
    return {
      platform: "x64_asan",
      target: "SumatraPDF-static",
      exeName: "SumatraPDF-static.exe",
      outDir: join("out", "dbg64_asan"),
      logName: "unit-tests-asan.txt",
    };
  }
  return {
    platform: "x64",
    target: "SumatraPDF",
    exeName: "SumatraPDF.exe",
    outDir: join("out", "dbg64"),
    logName: "unit-tests-dbg.txt",
  };
}

function tail(s: string, maxLines: number): string {
  const lines = s.trimEnd().split(/\r?\n/);
  return lines.slice(Math.max(0, lines.length - maxLines)).join("\n");
}

async function runAndCapture(exe: string, cwd: string, logPath: string): Promise<{ exitCode: number; output: string }> {
  const proc = Bun.spawn([exe, "-unit-tests", "-for-ai"], {
    cwd,
    stdout: "pipe",
    stderr: "pipe",
    stdin: "ignore",
  });
  const [stdout, stderr, exitCode] = await Promise.all([
    new Response(proc.stdout).text(),
    new Response(proc.stderr).text(),
    proc.exited,
  ]);
  const output = stdout + stderr;
  writeFileSync(logPath, output);
  return { exitCode, output };
}

async function main() {
  const kind = parseArgs();
  const { platform, target, exeName, outDir, logName } = configFor(kind);
  const { msbuildPath, vsRoot } = detectVisualStudio2026();

  await runLogged(join("bin", "premake5.exe"), ["vs2022"]);
  await runLogged(msbuildPath, [
    String.raw`vs2022\SumatraPDF.sln`,
    `/t:${target}`,
    `/p:Configuration=Debug;Platform=${platform}`,
    "/m",
  ]);
  if (kind === "asan") {
    copyAsanRuntime(vsRoot, outDir);
  }

  mkdirSync(outDir, { recursive: true });
  const exe = join(process.cwd(), outDir, exeName);
  const logPath = join(process.cwd(), outDir, logName);
  const { exitCode, output } = await runAndCapture(exe, outDir, logPath);

  console.log(`${exeName} -unit-tests exit code: ${exitCode}`);
  console.log(`output: ${logPath}`);
  if (exitCode === 0 && output.includes("Passed all ")) {
    console.log("unit tests passed");
    return;
  }
  if (exitCode === 7 || output.includes("unit tests crash") || output.includes("AddressSanitizer")) {
    console.log("unit tests crashed");
  } else if (output.includes("Assertion failed:") || output.includes("Failed ")) {
    console.log("unit tests failed assertions");
  } else {
    console.log("unit tests failed");
  }
  console.log("--- tail ---");
  console.log(tail(output, 80));
  process.exit(exitCode || 1);
}

await main();
