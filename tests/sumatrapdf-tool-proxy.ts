// sumatrapdf-tool.exe forwards its args to SumatraPDF.exe over a named pipe and
// relays SumatraPDF.exe's stdout / stderr and exit code (src/ToolPipe.h): text
// and binary output, errors, non-ASCII args, `cli`, piped stdin, rejecting what
// would open a window, and no SumatraPDF.exe left.
//
// Run: bun tests/sumatrapdf-tool-proxy.ts [--no-build]

import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const PNG_SIGNATURE = [0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a];
const TIMEOUT_MS = 60_000;

function run(tool: string, args: string[]) {
  const p = Bun.spawnSync([tool, ...args], { timeout: TIMEOUT_MS });
  return { exitCode: p.exitCode, stdout: p.stdout, stderr: p.stderr.toString() };
}

function sumatraRunning(): boolean {
  const p = Bun.spawnSync(["tasklist", "/FI", `IMAGENAME eq SumatraPDF.exe`, "/NH"]);
  return p.stdout.toString().includes("SumatraPDF.exe");
}

export async function testit(): Promise<void> {
  const tool = join(dirname(EXE), "sumatrapdf-tool.exe");
  if (!existsSync(tool)) {
    console.log("  skipping sumatrapdf-tool-proxy: no sumatrapdf-tool.exe from the tested build");
    return;
  }
  const dir = tmpPath("sumatrapdf-tool-proxy");
  mkdirSync(join(dir, "zażółć"), { recursive: true });
  const pdf = join(dir, "zażółć", "plik ąę.pdf");
  copyFileSync(join(ROOT, "ext", "a-zlib", "zlib.3.pdf"), pdf);

  // text to stdout, a non-ASCII path in the args
  const info = run(tool, ["info", pdf]);
  const infoOut = info.stdout.toString();
  if (info.exitCode !== 0 || !infoOut.includes("plik ąę.pdf") || !infoOut.includes("Info object")) {
    throw new Error(`sumatrapdf-tool-proxy: info: exit ${info.exitCode}, stdout: ${infoOut}, stderr: ${info.stderr}`);
  }

  // an error: stderr and the exit code come back
  const missing = run(tool, ["info", join(dir, "missing.pdf")]);
  if (missing.exitCode === 0 || !missing.stderr.includes("cannot open")) {
    throw new Error(`sumatrapdf-tool-proxy: missing file: exit ${missing.exitCode}, stderr: ${missing.stderr}`);
  }

  // binary to stdout arrives unchanged
  const draw = run(tool, ["draw", "-F", "png", "-o", "-", pdf, "1"]);
  const sig = Array.from(draw.stdout.subarray(0, PNG_SIGNATURE.length));
  if (draw.exitCode !== 0 || sig.join(",") !== PNG_SIGNATURE.join(",") || draw.stdout.length < 1000) {
    throw new Error(`sumatrapdf-tool-proxy: draw: exit ${draw.exitCode}, ${draw.stdout.length} bytes, ${sig}`);
  }

  // not only the mupdf tools: SumatraPDF.exe's own `cli`
  const cli = run(tool, ["cli", "info", pdf]);
  if (cli.exitCode !== 0 || !cli.stdout.toString().includes("pages")) {
    throw new Error(`sumatrapdf-tool-proxy: cli info: exit ${cli.exitCode}, stdout: ${cli.stdout}, ${cli.stderr}`);
  }

  // stdin from a pipe reaches the child: `run` reads its script from it
  const script = Buffer.from('print("from stdin", 1 + 2)\n');
  const runP = Bun.spawnSync([tool, "run"], { stdin: script, timeout: TIMEOUT_MS });
  const runOut = runP.stdout.toString();
  if (runP.exitCode !== 0 || !runOut.includes("from stdin 3")) {
    throw new Error(`sumatrapdf-tool-proxy: run < script: exit ${runP.exitCode}, stdout: ${runOut}`);
  }

  // what would open a window is not a command-line operation: rejected
  const view = run(tool, [pdf]);
  if (view.exitCode === 0 || !view.stderr.includes("not a command-line operation")) {
    throw new Error(`sumatrapdf-tool-proxy: viewing a file wasn't rejected: exit ${view.exitCode}, ${view.stderr}`);
  }

  if (sumatraRunning()) {
    throw new Error("sumatrapdf-tool-proxy: a SumatraPDF.exe was left running");
  }
  console.log("sumatrapdf-tool-proxy: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
