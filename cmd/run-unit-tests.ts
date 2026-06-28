import { copyFileSync, existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

const asanDllName = "clang_rt.asan_dynamic-x86_64.dll";

type BuildKind = "dbg" | "rel" | "asan";

function usage(): never {
  throw new Error("usage: bun cmd/run-unit-tests.ts [-dbg | -rel | -asan]");
}

function parseArgs(): BuildKind {
  let kind: BuildKind = "dbg";
  let seen = false;
  for (const arg of process.argv.slice(2)) {
    if (arg === "-dbg" || arg === "-debug") {
      if (seen) usage();
      kind = "dbg";
      seen = true;
    } else if (arg === "-rel" || arg === "-release") {
      if (seen) usage();
      kind = "rel";
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

function configFor(kind: BuildKind): { config: string; platform: string; outDir: string; logName: string } {
  if (kind === "rel") {
    return { config: "Release", platform: "x64", outDir: join("out", "rel64"), logName: "unit-tests-rel.txt" };
  }
  if (kind === "asan") {
    return { config: "Debug", platform: "x64_asan", outDir: join("out", "dbg64_asan"), logName: "unit-tests-asan.txt" };
  }
  return { config: "Debug", platform: "x64", outDir: join("out", "dbg64"), logName: "unit-tests-dbg.txt" };
}

function tail(s: string, maxLines: number): string {
  const lines = s.trimEnd().split(/\r?\n/);
  return lines.slice(Math.max(0, lines.length - maxLines)).join("\n");
}

async function runAndCapture(exe: string, cwd: string, logPath: string): Promise<{ exitCode: number; output: string }> {
  const proc = Bun.spawn([exe, "-for-ai"], {
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
  const { config, platform, outDir, logName } = configFor(kind);
  const { msbuildPath, vsRoot } = detectVisualStudio2026();

  await runLogged(join("bin", "premake5.exe"), ["vs2022"]);
  await runLogged(msbuildPath, [
    String.raw`vs2022\SumatraPDF.sln`,
    "/t:test_util:Rebuild",
    `/p:Configuration=${config};Platform=${platform}`,
    "/m",
  ]);
  if (kind === "asan") {
    copyAsanRuntime(vsRoot, outDir);
  }

  mkdirSync(outDir, { recursive: true });
  const exe = join(process.cwd(), outDir, "test_util.exe");
  const logPath = join(process.cwd(), outDir, logName);
  const { exitCode, output } = await runAndCapture(exe, outDir, logPath);

  console.log(`test_util exit code: ${exitCode}`);
  console.log(`output: ${logPath}`);
  if (exitCode === 0 && output.includes("Passed all ")) {
    console.log("unit tests passed");
    return;
  }
  if (exitCode === 7 || output.includes("test_util crash") || output.includes("AddressSanitizer")) {
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
