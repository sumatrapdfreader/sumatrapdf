// Shared helpers for print-to-PDF tests (Microsoft Print to PDF + output= path).
import { existsSync, readFileSync, rmSync, statSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { EXE } from "./util.ts";

export const PRINT_TO_PDF = "Microsoft Print to PDF";

export function printOutputPath(path: string): string {
  return path.replace(/\\/g, "/");
}

function waitForFile(path: string, timeoutMs = 15000): boolean {
  const start = Date.now();
  let lastSize = -1;
  let stableReads = 0;
  while (Date.now() - start < timeoutMs) {
    if (existsSync(path)) {
      const size = statSync(path).size;
      if (size > 0 && size === lastSize) {
        stableReads++;
        if (stableReads >= 3) {
          return true;
        }
      } else {
        stableReads = 0;
        lastSize = size;
      }
    }
    Bun.sleepSync(200);
  }
  return existsSync(path) && statSync(path).size > 0;
}

export function runPrintToPdf(
  inputPath: string,
  outputPath: string,
  printSettings: string,
): { ok: boolean; stdout: string; stderr: string; exitCode: number } {
  rmSync(outputPath, { force: true });
  const out = printOutputPath(outputPath);
  let settings = printSettings;
  if (!settings.includes("output=")) {
    settings = settings ? `output=${out},${settings}` : `output=${out}`;
  }
  const p = Bun.spawnSync({
    cmd: [EXE, "-for-testing", "-silent", "-exit-when-done", "-print-to", PRINT_TO_PDF, "-print-settings", settings, inputPath],
    stdout: "pipe",
    stderr: "pipe",
  });
  return {
    ok: p.exitCode === 0 && waitForFile(outputPath),
    stdout: p.stdout.toString(),
    stderr: p.stderr.toString(),
    exitCode: p.exitCode ?? -1,
  };
}

export function tempPrintOutput(name: string): string {
  return join(tmpdir(), `sumatra-print-test-${name}-${Date.now()}.pdf`);
}

export function requirePrintToPdf(): void {
  const p = Bun.spawnSync({
    cmd: ["powershell", "-NoProfile", "-Command", `(Get-Printer -Name '${PRINT_TO_PDF}' -ErrorAction SilentlyContinue) -ne $null`],
    stdout: "pipe",
  });
  if (p.stdout.toString().trim() !== "True") {
    throw new Error(
      `"${PRINT_TO_PDF}" printer not found. Enable it in Windows Settings > Bluetooth & devices > Printers & scanners.`,
    );
  }
}

export function pdfMediaBox(path: string): { w: number; h: number } {
  const text = readFileSync(path).toString("latin1");
  const m = text.match(/\/MediaBox\s*\[\s*([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s*\]/);
  if (!m) {
    throw new Error(`no /MediaBox in ${path}`);
  }
  return { w: parseFloat(m[3]), h: parseFloat(m[4]) };
}