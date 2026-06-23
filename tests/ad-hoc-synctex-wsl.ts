// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/5702
//
// Verifies that SyncTeX forward and inverse search work across two
// previously-unsupported WSL workflows:
//   1. files on the WSL filesystem, compiled from inside WSL
//   2. files on the Windows filesystem, compiled from inside WSL
//      (e.g. via `wsl.exe -d Ubuntu -- tectonic ...` against /mnt/c/...)
//
// Uses Tectonic (run via WSL) to produce a real PDF + .synctex.gz pair for
// each workflow, then runs the control pipe TestSynctex/TestInverseSearch
// commands, which query SourceToDoc/DocToSource directly and check the
// results.
//
// Run:  bun tests/ad-hoc-synctex-wsl.ts [--no-build]   (or via tests/latex.ts)

import { existsSync, mkdirSync, rmSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";

const TEX_NAME = "test.tex";
const PDF_NAME = TEX_NAME.replace(/\.tex$/, ".pdf");

const WIN_TEST_DIR = join(import.meta.dir, "issue-5702-data", ".work");
const WIN_TEX_SRC = join(import.meta.dir, "issue-5702-data", TEX_NAME);

const WSL_DISTRO = "Ubuntu";
const WSL_TEST_DIR = `\\\\wsl.localhost\\${WSL_DISTRO}\\tmp\\sumatra-test-5702`;
const WSL_TEX_SRC = join(WSL_TEST_DIR, TEX_NAME)

// line 4 of issue-5702-data/test.tex is a body paragraph that synctex maps to a position
const TARGET_LINE = 4;

type Rect = {
  x: number,
  y: number,
  dx: number,
  dy: number,
}

type FwdSearchResult = {
  ret: number;
  page: number;
  nrects: number;
  rect?: Rect,
  raw: string;
};

type FwdSearchPoint = { page: number; x: number; y: number };

type InvSearchResult = {
  ret: number;
  srcfile: string;
  line: number;
  col: number;
  raw: string;
};


function run(cmd: string[]): { ok: boolean; stdout: string; stderr: string } {
  try {
    const p = Bun.spawnSync({ cmd, stdout: "pipe", stderr: "pipe" });
    return {
      ok: p.exitCode === 0,
      stdout: p.stdout.toString(),
      stderr: p.stderr.toString(),
    };
  } catch (e: any) {
    return {
      ok: false,
      stdout: "",
      stderr: String(e),
    };
  }
}

function findWslTectonic(): boolean {
  const r = run(["wsl.exe", "-d", WSL_DISTRO, "--", "tectonic", "--version"]);
  if (!r.ok) {
    console.log(
      `\nSKIP issue-5702: WSL distro '${WSL_DISTRO}' with tectonic not available, skipping.\n` +
      `To run this test, install a WSL distro and tectonic inside it, e.g.:\n` +
      `    wsl --install -d ${WSL_DISTRO}\n` +
      `    wsl -d ${WSL_DISTRO} -- bash -lc "curl --proto '=https' --tlsv1.2 -fsSL https://drop-sh.fullyjustified.net |sh"`,
    );
  }
  return r.ok;
}

function findWindowsTectonic(): boolean {
  const r = run(["tectonic", "--version"]);
  if (!r.ok) {
    console.log(
      `\nSKIP issue-5702: tectonic not found on Windows, skipping.\n` +
      `To install it, run this in PowerShell:\n` +
      `    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072` +
      `    iex ((New-Object System.Net.WebClient).DownloadString('https://drop-ps1.fullyjustified.net'))`,
    );
  }
  return r.ok;
}

// converts a Windows absolute path (e.g. "C:\foo\bar.tex") to its WSL
// mount-path equivalent (e.g. "/mnt/c/foo/bar.tex"), for invoking tectonic
// from inside WSL and for computing expected DocToSource results.
function windowsPathToWslMountPath(winPath: string): string | null {
  const m = winPath.match(/^([A-Za-z]):[\\/](.*)$/);
  if (!m) {
    return null;
  }
  const drive = m[1].toLowerCase();
  const rest = m[2].replace(/\\/g, "/");
  return `/mnt/${drive}/${rest}`;
}

// converts a WSL UNC path (e.g. "\\wsl.localhost\Ubuntu\home\user\file.tex"
// or "\\wsl$\Ubuntu\...") to its plain Unix-path equivalent (e.g.
// "/home/user/file.tex"), for invoking tectonic from inside WSL and for
// computing expected DocToSource results.
function wslUncPathToUnixPath(wslUncpath: string): string | null {
  const match = wslUncpath.match(
    /^\\\\(?:wsl\$|wsl\.localhost)\\[^\\]+\\(.*)$/
  );

  if (!match) {
    return null;
  }

  return "/" + match[1].replaceAll("\\", "/");
}

// runs the app's control-pipe synctex forward-search for the given pdf and
// returns the parsed result (ret is a PDFSYNCERR_* code; 0 == PDFSYNCERR_SUCCESS),
// including the first rect's coordinates.
async function forwardSearch(
  pdfPath: string,
  srcPath: string,
): Promise<FwdSearchResult> {
  const [, rawArg] = await runControlCommand(EXE, ControlCommand.TestSynctex, [pdfPath, srcPath, TARGET_LINE]);
  const raw = String(rawArg).trim();

  const m = raw.match(/ret=(-?\d+)\s+page=(-?\d+)\s+nrects=(-?\d+)/);
  if (!m) throw Error(`(forwardSearch: parse error) raw: ${raw}`);
  const result: FwdSearchResult = {
    ret: parseInt(m[1]),
    page: parseInt(m[2]),
    nrects: parseInt(m[3]),
    raw,
  };
  // parse rect separately, since it's only present when nrects >= 1
  const n = raw.match(
    /rect_x=(-?\d+)\s+rect_y=(-?\d+)\s+rect_dx=(-?\d+)\s+rect_dy=(-?\d+)/,
  );
  if (n) {
    result["rect"] = {
      x: parseInt(n[1]),
      y: parseInt(n[2]),
      dx: parseInt(n[3]),
      dy: parseInt(n[4]),
    };
  }

  return result;
}

// runs the app's control-pipe synctex inverse-search for the given pdf and
// returns the parsed result (ret is a PDFSYNCERR_* code; 0 == PDFSYNCERR_SUCCESS)
async function inverseSearch(
  pdfPath: string,
  page: number,
  x: number,
  y: number,
): Promise<InvSearchResult> {
  const [, rawArg] = await runControlCommand(EXE, ControlCommand.TestInverseSearch, [pdfPath, page, x, y]);
  const raw = String(rawArg).trim();

  const m = raw.match(/^ret=(-?\d+)\s+srcfile=(.*?)\s+line=(-?\d+)\s+col=(-?\d+)$/);
  if (!m) {
    throw Error(`(inverseSearch: parse error) raw: ${raw}`);
  }
  return { ret: parseInt(m[1]), srcfile: m[2], line: parseInt(m[3]), col: parseInt(m[4]), raw };
}

// derives a (page, x, y) point from a forward-search result, taking the
// center of the rect TARGET_LINE mapped to. Returns null if the result
// wasn't a usable match (e.g. the forward search itself failed).
function pointFromFwdSearchResult(res: FwdSearchResult): FwdSearchPoint | null {
  if (res.ret !== 0 || res.page < 1 || res.nrects < 1 || !res.rect) {
    return null;
  }
  return {
    page: res.page,
    x: Math.round(res.rect.x + res.rect.dx / 2),
    y: Math.round(res.rect.y + res.rect.dy / 2),
  };
}

// compiles srcPath (a Windows path or a WSL UNC path) into workDir
// using tectonic run inside WSL.
// Returns back srcpath along with the path of the generated pdf
function compileFiles(
  workDir: string,
  srcPath: string,
  { useWsl = true }: { useWsl?: boolean; } = {}
): { pdfPath: string; srcPath: string } {
  // clean up working directory
  rmSync(workDir, { recursive: true, force: true });
  mkdirSync(workDir, { recursive: true });

  if (!existsSync(srcPath)) {
    writeFileSync(srcPath, readFileSync(WIN_TEX_SRC));
  }

  let srcPathForTectonic = srcPath;
  if (useWsl) {
    // convert srcPath to a format that can be used by tectonic in wsl
    const WslCompatibleSrcPath = windowsPathToWslMountPath(srcPath) ?? wslUncPathToUnixPath(srcPath);
    if (!WslCompatibleSrcPath) {
      throw new Error(`Source path is neither a Windows path nor a WSL UNC path: ${srcPath}`);
    }
    srcPathForTectonic  = WslCompatibleSrcPath;
  }

  let workDirForTectonic = workDir;
  if (useWsl) {
    // convert workDir to a format that can be used by tectonic in wsl
    const WslCompatibleWorkDir = windowsPathToWslMountPath(workDir) ?? wslUncPathToUnixPath(workDir);
    if (!WslCompatibleWorkDir) {
      throw new Error(`Work dir is neither a Windows path nor a WSL UNC path: ${workDir}`);
    }
    workDirForTectonic = WslCompatibleWorkDir;
  }

  console.log(`• compiling ${srcPathForTectonic} in ${useWsl ? "WSL" : "Windows"}`);
  const cmd = useWsl ? ["wsl.exe", "-d", WSL_DISTRO, "--"] : [];

  cmd.push(
    "tectonic", "-X", "compile", srcPathForTectonic, "--synctex", "--outdir", workDirForTectonic,
  );

  const compile = run(cmd);
  if (!compile.ok) {
    console.error(compile.stdout);
    console.error(compile.stderr);
    throw new Error(`Error running tectonic`);
  }

  return { pdfPath: join(workDir, PDF_NAME), srcPath };
}

async function testForwardSearch(
  pdfPath: string,
  srcPath: string,
  label: string,
): Promise<{ ok: boolean, result: FwdSearchResult }> {
    const res = await forwardSearch(pdfPath, srcPath);
    const pass = res.ret === 0 && res.page >= 1 && res.nrects >= 1;
    console.log(`${pass ? "PASS" : "FAIL"} forward search (${label}) -> nrects: ${res.nrects}`);
    return { ok: pass, result: res };
}

async function testInverseSearch(
  pdfPath: string,
  srcPath: string,
  fwdResult: FwdSearchResult,
  label: string,
  { expectAsIs = false }: { expectAsIs?: boolean; } = {},
): Promise<{ ok: boolean }> {
  const pt = pointFromFwdSearchResult(fwdResult);
  if (!pt) {
    console.log(`FAIL inverse search (${label}) -> could not discover point: ${fwdResult.raw}`);
    return { ok: false };
  }
  const expectedSrcFile = expectAsIs ? srcPath : windowsPathToWslMountPath(srcPath) ?? wslUncPathToUnixPath(srcPath);
  const res = await inverseSearch(pdfPath, pt.page, pt.x, pt.y);
  const pass = res.ret === 0 && res.srcfile === expectedSrcFile && res.line === TARGET_LINE;
  console.log(`${pass ? "PASS" : "FAIL"} inverse search (${label}) -> srcfile: ${res.srcfile}`);
  return { ok: pass };
}

// ---------------------------------------------------------------------------

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }
  if (!existsSync(WIN_TEX_SRC)) {
    throw new Error(`missing test fixture: ${WIN_TEX_SRC}`);
  }
  const haveWslTectonic = findWslTectonic();
  const haveWinTectonic = findWindowsTectonic();
  if (!haveWslTectonic && !haveWinTectonic) {
    return;
  }

  type Scenario = {
    name: string;
    fileLocation: "Windows" | "WSL";
    toolchain: "wsl" | "windows";
  };

  const scenarios: Scenario[] = [
    { name: "win files, wsl tectonic", fileLocation: "Windows", toolchain: "wsl" },
    { name: "wsl files, wsl tectonic", fileLocation: "WSL", toolchain: "wsl" },
    { name: "win files, win tectonic", fileLocation: "Windows", toolchain: "windows" },
    { name: "wsl files, win tectonic", fileLocation: "WSL", toolchain: "windows" },
  ];

  const results: { name: string; ok: boolean }[] = [];
  for (const scenario of scenarios) {
    const haveToolchain = scenario.toolchain === "wsl" ? haveWslTectonic : haveWinTectonic;
    if (!haveToolchain) {
      console.log(`SKIP ${scenario.name}: ${scenario.toolchain} tectonic not available`);
      continue;
    }

    const workDir = scenario.fileLocation === "Windows" ? WIN_TEST_DIR : WSL_TEST_DIR;
    const srcPath = scenario.fileLocation === "Windows" ? WIN_TEX_SRC : WSL_TEX_SRC;
    const files = compileFiles(workDir, srcPath, { useWsl: scenario.toolchain === "wsl" });

    const fwd = await testForwardSearch(files.pdfPath, files.srcPath, scenario.name);
    const inv = await testInverseSearch(files.pdfPath, files.srcPath, fwd.result, scenario.name, {
        // Windows-tectonic-compiled files: DocToSource returns the path
        // as-is (it's already a normal Windows/WSL-UNC path the compiler
        // itself recorded), not converted to /mnt/... or Unix form.
        expectAsIs: scenario.toolchain === "windows",
      });

    results.push({ name: `forward search (${scenario.name})`, ok: fwd.ok });
    results.push({ name: `inverse search (${scenario.name})`, ok: inv.ok });

    if (scenario.fileLocation == "Windows") {
      // forward search on a Windows source path using forward slashes
      // (C:/foo/bar.tex), to make sure that variant is handled too
      const label = `${scenario.name} with forward-slash win path`
      const fwdSlash = await testForwardSearch(files.pdfPath, files.srcPath.replace(/\\/g, "/"), label);
      results.push({ name: `forward search (${label})`, ok: fwdSlash.ok });
    }
  }

  const failed = results.filter((r) => !r.ok);
  console.log("");
  if (failed.length > 0) {
    throw new Error(`${failed.length}/${results.length} tests failed: ${failed.map((f) => f.name).join(", ")}`);
  }

  console.log(`PASS issue-5702: all ${results.length} scenarios resolved correctly`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
