import { $ } from "bun";
import { writeFile } from "node:fs/promises";
import { basename } from "node:path";

interface SymbolInfo {
  size: number;
  name: string;
  type: string;
}

function parseGlobals(raw: string): SymbolInfo[] {
  const symbols: SymbolInfo[] = [];
  const lines = raw.split(/\r?\n/);
  // format:
  //   func [0x... | sizeof=123] (FPO) void __cdecl FuncName(...)
  //   data [0x..., sizeof=123] static int varName
  const re = /^\s+(\w+)\s+\[.*?sizeof=(\d+)\]\s+(.+)$/;
  for (const line of lines) {
    const m = line.match(re);
    if (!m) continue;
    const type = m[1];
    const size = parseInt(m[2], 10);
    let name = m[3].trim();
    // strip (FPO) prefix from func names
    if (name.startsWith("(FPO) ")) {
      name = name.slice(6);
    }
    symbols.push({ size, name, type });
  }
  // sort biggest first
  symbols.sort((a, b) => b.size - a.size);
  return symbols;
}

function formatSymbols(symbols: SymbolInfo[]): string {
  if (symbols.length === 0) return "";
  const maxSizeWidth = symbols[0].size.toString().length;
  const lines: string[] = [];
  for (const s of symbols) {
    const sizeStr = s.size.toString().padStart(maxSizeWidth);
    lines.push(`${sizeStr} ${s.name} ${s.type}`);
  }
  return lines.join("\n") + "\n";
}

async function main() {
  const args = process.argv.slice(2);
  if (args.length === 0) {
    console.error("Usage: bun cmd/print-pdb-sizes.ts <path-to-pdb-file>");
    process.exit(1);
  }

  const pdbPath = args[0];

  // check llvm-pdbutil is available
  try {
    await $`llvm-pdbutil -h`.quiet();
  } catch {
    console.error("llvm-pdbutil not found in PATH. Make sure it is installed and available.");
    process.exit(1);
  }

  console.log(`Running llvm-pdbutil on ${pdbPath}...`);
  const result = await $`llvm-pdbutil pretty --globals --symbol-order=size ${pdbPath}`.text();

  const pdbName = basename(pdbPath, ".pdb");
  const rawPath = `${pdbName}.pdbglobals.raw.txt`;
  const parsedPath = `${pdbName}.pdbglobals.txt`;

  await writeFile(rawPath, result, "utf-8");
  console.log(`Wrote raw output to ${rawPath}`);

  const symbols = parseGlobals(result);
  const formatted = formatSymbols(symbols);
  await writeFile(parsedPath, formatted, "utf-8");
  console.log(`Wrote ${symbols.length} symbols to ${parsedPath}`);

  const totalSize = symbols.reduce((sum, s) => sum + s.size, 0);
  console.log(`Total size of ${symbols.length} symbols: ${totalSize.toLocaleString()} bytes`);

  // print 16 biggest
  const top = symbols.slice(0, 16);
  console.log(`\n16 biggest symbols:`);
  console.log(formatSymbols(top));
}

await main();
