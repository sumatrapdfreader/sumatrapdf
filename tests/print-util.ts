// Shared helpers for print-to-PDF tests (Microsoft Print to PDF + output= path).
import { existsSync, mkdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { tmpdir } from "node:os";
import { EXE } from "./util.ts";

export const PRINT_TO_PDF = "Microsoft Print to PDF";

export function printOutputPath(path: string): string {
  return path.replace(/\\/g, "/");
}

export function pdfPageCount(path: string): number {
  const data = readFileSync(path);
  const text = data.toString("latin1");
  const patterns = [
    /\/Type\s*\/Pages[\s\S]*?\/Count\s+(\d+)/,
    /\/Count\s+(\d+)[\s\S]*?\/Type\s*\/Pages/,
  ];
  for (const re of patterns) {
    const m = text.match(re);
    if (m) {
      return parseInt(m[1], 10);
    }
  }
  const pages = text.match(/\/Type\s*\/Page\b/g);
  if (pages && pages.length > 0) {
    return pages.length;
  }
  throw new Error(`could not determine page count for ${path}`);
}

export function writeMultiPagePdf(path: string, labels: string[]): void {
  mkdirSync(dirname(path), { recursive: true });
  const objs: string[] = [];
  const kids: number[] = [];
  let nextId = 3;
  for (const label of labels) {
    const pageId = nextId++;
    const contentId = nextId++;
    kids.push(pageId);
    const stream = `BT /F1 24 Tf 72 720 Td (${label}) Tj ET`;
    objs.push(
      `${pageId} 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents ${contentId} 0 R /Resources << /Font << /F1 99 0 R >> >> >>\nendobj\n`,
    );
    objs.push(`${contentId} 0 obj\n<< /Length ${stream.length} >>\nstream\n${stream}\nendstream\nendobj\n`);
  }
  const kidsRef = kids.map((id) => `${id} 0 R`).join(" ");
  const body =
    `%PDF-1.4\n` +
    `1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n` +
    `2 0 obj\n<< /Type /Pages /Kids [ ${kidsRef} ] /Count ${labels.length} >>\nendobj\n` +
    objs.join("") +
    `99 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n`;
  const xrefStart = body.length;
  const xrefEntries = ["0000000000 65535 f \n"];
  const idPositions = new Map<number, number>();
  for (const m of body.matchAll(/(\d+) 0 obj/g)) {
    idPositions.set(parseInt(m[1], 10), m.index ?? 0);
  }
  const maxId = Math.max(...idPositions.keys());
  for (let id = 0; id <= maxId; id++) {
    const pos = idPositions.get(id);
    xrefEntries.push(pos === undefined ? "0000000000 00000 n \n" : `${String(pos).padStart(10, "0")} 00000 n \n`);
  }
  const trailer = `xref\n0 ${maxId + 1}\n${xrefEntries.join("")}` +
    `trailer\n<< /Size ${maxId + 1} /Root 1 0 R >>\nstartxref\n${xrefStart}\n%%EOF\n`;
  writeFileSync(path, body + trailer);
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