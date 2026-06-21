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
// Run:  bun tests/issue-5702.ts [--no-build]   (or via tests/all.ts)

import { existsSync, mkdirSync, rmSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, runStandalone } from "./util.ts";
import { ControlClient, ControlCommand, withControlledSumatra } from "../cmd/control.ts";

const DATA = join(import.meta.dir, "issue-5702-data");
const WORK = join(DATA, ".work");

const TEX_NAME = "test.tex";
const TEX_SRC = join(DATA, TEX_NAME);
const PDF_NAME = TEX_NAME.replace(/\.tex$/, ".pdf");
const SYNCTEX_NAME = TEX_NAME.replace(/\.tex$/, ".synctex.gz");

const WSL_DISTRO = "Ubuntu";
const WSL_UNC_ROOT = `\\\\wsl.localhost\\${WSL_DISTRO}`;
const WSL_TEST_DIR_UNIX = "/tmp/sumatra-test-5702";
const WSL_TEST_DIR_UNC = join(WSL_UNC_ROOT, "tmp", "sumatra-test-5702");

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
  const p = Bun.spawnSync({ cmd, stdout: "pipe", stderr: "pipe" });
  return {
    ok: p.exitCode === 0,
    stdout: p.stdout.toString(),
    stderr: p.stderr.toString(),
  };
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

// converts a Windows absolute path (e.g. "C:\foo\bar.tex") to its WSL
// mount-path equivalent (e.g. "/mnt/c/foo/bar.tex"), for invoking tectonic
// from inside WSL and for computing expected DocToSource results.
function windowsPathToWslMountPath(winPath: string): string {
  const m = winPath.match(/^([A-Za-z]):[\\/](.*)$/);
  if (!m) {
    throw new Error(`not a Windows absolute path: ${winPath}`);
  }
  const drive = m[1].toLowerCase();
  const rest = m[2].replace(/\\/g, "/");
  return `/mnt/${drive}/${rest}`;
}

// runs the app's control-pipe synctex forward-search for the given pdf and
// returns the parsed result (ret is a PDFSYNCERR_* code; 0 == PDFSYNCERR_SUCCESS),
// including the first rect's coordinates.
async function forwardSearch(
  client: ControlClient,
  pdfPath: string,
  srcPath: string,
): Promise<FwdSearchResult> {
  const [, rawArg] = await client.request(ControlCommand.TestSynctex, [pdfPath, srcPath, TARGET_LINE]);
  const raw = String(rawArg).trim();

  const m = raw.match(/ret=(-?\d+)\s+page=(-?\d+)\s+nrects=(-?\d+)/);
  if (!m) throw Error(`(forwardSearch: parse error) raw: ${raw}`);
  const result: FwdSearchResult = {
    ret: parseInt(m[1]),
    page: parseInt(m[2]),
    nrects: parseInt(m[3]),
    raw,
  };
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
  client: ControlClient,
  pdfPath: string,
  page: number,
  x: number,
  y: number,
): Promise<InvSearchResult> {
  const [, rawArg] = await client.request(ControlCommand.TestInverseSearch, [pdfPath, page, x, y]);
  const raw = String(rawArg).trim();

  const m = raw.match(/ret=(-?\d+)\s+srcfile=(.*)\s+line=(-?\d+)\s+col=(-?\d+)/);
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

// verifies a tectonic compile produced both the pdf and synctex.gz; throws
// with captured stdout/stderr otherwise.
function verifyCompileOutput(
  compile: { ok: boolean; stdout: string; stderr: string },
  pdfPath: string,
  synctexPath: string,
  label: "Windows file" | "WSL file",
): void {
  if (!compile.ok || !existsSync(pdfPath) || !existsSync(synctexPath)) {
    console.error(compile.stdout);
    console.error(compile.stderr);
    throw new Error(`tectonic did not produce ${pdfPath} + ${synctexPath} (${label})`);
  }
}

// compiles issue-5702-data/test.tex on the Windows filesystem via
// `wsl.exe -d Ubuntu -- tectonic ...` (workflow: Windows files, WSL compile).
// Returns the Windows paths to the resulting pdf and source file.
function compileWinFiles(): { pdfPath: string; srcPath: string } {
  rmSync(WORK, { recursive: true, force: true });
  mkdirSync(WORK, { recursive: true });

  const wslTexPath = windowsPathToWslMountPath(TEX_SRC);
  const wslOutDir = windowsPathToWslMountPath(WORK);

  console.log(`• compiling ${TEX_SRC} via wsl -d ${WSL_DISTRO} -- tectonic ...`);
  const compile = run([
    "wsl.exe", "-d", WSL_DISTRO, "--",
    "tectonic", "-X", "compile", wslTexPath, "--synctex", "--outdir", wslOutDir,
  ]);

  const pdfPath = join(WORK, PDF_NAME);
  const synctexPath = join(WORK, SYNCTEX_NAME);
  verifyCompileOutput(compile, pdfPath, synctexPath, "Windows file");

  return { pdfPath, srcPath: TEX_SRC };
}

// copies issue-5702-data/test.tex onto the WSL filesystem (via its UNC path)
// and compiles it there (workflow: WSL files, WSL compile). Returns the WSL
// UNC paths to the resulting pdf and source file.
function compileWslFiles(): { pdfPath: string; srcPath: string } {
  rmSync(WSL_TEST_DIR_UNC, { recursive: true, force: true });
  mkdirSync(WSL_TEST_DIR_UNC, { recursive: true });

  const texContent = readFileSync(TEX_SRC);
  const srcPath = join(WSL_TEST_DIR_UNC, TEX_NAME);
  writeFileSync(srcPath, texContent);

  console.log(`• compiling ${WSL_TEST_DIR_UNIX}/${TEX_NAME} via wsl -d ${WSL_DISTRO} -- tectonic ...`);
  const compile = run([
    "wsl.exe", "-d", WSL_DISTRO, "--",
    "tectonic", "-X", "compile", `${WSL_TEST_DIR_UNIX}/${TEX_NAME}`, "--synctex",
  ]);

  const pdfPath = join(WSL_TEST_DIR_UNC, PDF_NAME);
  const synctexPath = join(WSL_TEST_DIR_UNC, SYNCTEX_NAME);
  verifyCompileOutput(compile, pdfPath, synctexPath, "WSL file");

  return { pdfPath, srcPath };
}

async function testForwardSearch(
  client: ControlClient,
  pdfPath: string,
  srcPath: string,
  label: "Windows file" | "WSL file",
): Promise<{ ok: boolean, result: FwdSearchResult }> {
    const res = await forwardSearch(client, pdfPath, srcPath);
    const pass = res.ret === 0 && res.page >= 1 && res.nrects >= 1;
    console.log(`${pass ? "PASS" : "FAIL"} forward search (${label}) -> ${res.raw}`);
    return { ok: pass, result: res };
}

async function testInverseSearch(
  client: ControlClient,
  pdfPath: string,
  srcPath: string,
  fwdResult: FwdSearchResult,
  label: "Windows file" | "WSL file",
): Promise<{ ok: boolean }> {
  const pt = pointFromFwdSearchResult(fwdResult);
  if (!pt) {
    console.log(`FAIL inverse search (${label}) -> could not discover point: ${fwdResult.raw}`);
    return { ok: false };
  }
  const expectedSrcFile = label === "Windows file" ? windowsPathToWslMountPath(srcPath) : `${WSL_TEST_DIR_UNIX}/${TEX_NAME}`;
  const res = await inverseSearch(client, pdfPath, pt.page, pt.x, pt.y);
  const pass = res.ret === 0 && res.srcfile === expectedSrcFile && res.line === TARGET_LINE;
  console.log(`${pass ? "PASS" : "FAIL"} inverse search (${label}) -> ${res.raw}`);
  return { ok: pass };
}

// ---------------------------------------------------------------------------

export async function testit(): Promise<void> {
  if (!existsSync(EXE)) {
    throw new Error(`app not found: ${EXE} (build first)`);
  }
  if (!existsSync(TEX_SRC)) {
    throw new Error(`missing test fixture: ${TEX_SRC}`);
  }
  if (!findWslTectonic()) {
    return;
  }

  // compile the fixture for Windows and WSL before running any test
  const winFiles = compileWinFiles();
  const wslFiles = compileWslFiles();

  const results = await withControlledSumatra(EXE, async (client) => {
    const fwdWin = await testForwardSearch(client, winFiles.pdfPath, winFiles.srcPath, "Windows file");
    const fwdWsl = await testForwardSearch(client, wslFiles.pdfPath, wslFiles.srcPath, "WSL file");
    const invWin = await testInverseSearch(client, winFiles.pdfPath, winFiles.srcPath, fwdWin.result, "Windows file");
    const invWsl = await testInverseSearch(client, wslFiles.pdfPath, wslFiles.srcPath, fwdWsl.result, "WSL file");
    return [
      { name: "forward search(win files)", ok: fwdWin.ok },
      { name: "forward search(wsl files)", ok: fwdWsl.ok },
      { name: "inverse search(win files)", ok: invWin.ok },
      { name: "inverse search(wsl files)", ok: invWsl.ok },
    ];
  });

  const failed = results.filter((r) => !r.ok);
  console.log("");
  if (failed.length > 0) {
    throw new Error(`${failed.length}/${results.length} tests failed: ${failed.map((f) => f.name).join(", ")}`);
  }

  console.log(`PASS issue-5702: all ${results.length} WSL synctex scenarios resolved correctly`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
