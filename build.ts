import { $ } from "bun";
import { unlink, readdir, readFile, writeFile } from "node:fs/promises";
import { join, extname } from "node:path";

async function countLines(filePath: string): Promise<number> {
  const content = await readFile(filePath, "utf-8");
  return content.split("\n").length;
}

interface Stats {
  lines: number;
  files: number;
}

async function scanDir(dir: string, stats: Map<string, Stats>): Promise<void> {
  const entries = await readdir(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = join(dir, entry.name);
    if (entry.isDirectory()) {
      await scanDir(fullPath, stats);
    } else if (entry.isFile()) {
      const ext = extname(entry.name).toLowerCase();
      if (ext) {
        const lines = await countLines(fullPath);
        const current = stats.get(ext) || { lines: 0, files: 0 };
        current.lines += lines;
        current.files += 1;
        stats.set(ext, current);
      }
    }
  }
}

async function wordCount(): Promise<void> {
  const stats = new Map<string, Stats>();
  await scanDir("src", stats);

  const extensions = [".cpp", ".h", ".c", ".txt"];
  const rows: { ext: string; lines: number; files: number }[] = [];
  for (const ext of extensions) {
    const s = stats.get(ext) || { lines: 0, files: 0 };
    rows.push({ ext, lines: s.lines, files: s.files });
  }

  const extWidth = Math.max(3, ...rows.map((r) => r.ext.length));
  const linesWidth = Math.max(5, ...rows.map((r) => r.lines.toString().length));
  const filesWidth = Math.max(5, ...rows.map((r) => r.files.toString().length));

  console.log("ext".padStart(extWidth) + "  " + "lines".padStart(linesWidth) + "  " + "files".padStart(filesWidth));
  for (const r of rows) {
    console.log(
      r.ext.padStart(extWidth) +
        "  " +
        r.lines.toString().padStart(linesWidth) +
        "  " +
        r.files.toString().padStart(filesWidth),
    );
  }
}

async function collectFiles(dir: string, extensions: string[]): Promise<string[]> {
  const files: string[] = [];
  const entries = await readdir(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = join(dir, entry.name);
    if (entry.isDirectory()) {
      files.push(...(await collectFiles(fullPath, extensions)));
    } else if (entry.isFile()) {
      const ext = extname(entry.name).toLowerCase();
      if (extensions.includes(ext)) {
        files.push(fullPath);
      }
    }
  }
  return files;
}

async function fixVirtualOverride(): Promise<void> {
  const files = await collectFiles("src", [".cpp", ".h"]);
  // Match: virtual ... override (with override anywhere after virtual on same line)
  const pattern = /^(\s*)virtual\s+(.*\boverride\b.*)$/;
  let totalFixed = 0;

  for (const filePath of files) {
    const content = await readFile(filePath, "utf-8");
    const hasCRLF = content.includes("\r\n");
    const lineEnding = hasCRLF ? "\r\n" : "\n";
    const lines = content.split(/\r?\n/);

    let modified = false;
    const newLines = lines.map((line) => {
      const match = line.match(pattern);
      if (match) {
        modified = true;
        return match[1] + match[2];
      }
      return line;
    });

    if (modified) {
      const newContent = newLines.join(lineEnding);
      await writeFile(filePath, newContent, "utf-8");
      console.log(`Fixed: ${filePath}`);
      totalFixed++;
    }
  }

  console.log(`\nTotal files fixed: ${totalFixed}`);
}

async function build(): Promise<void> {
  await unlink("./out/dbg64/SumatraPDF.exe").catch(() => {});
  await $`msbuild .\\vs2022\\SumatraPDF.sln /t:SumatraPDF "/p:Configuration=Debug;Platform=x64" /m`;
}

async function buildPreview2(): Promise<void> {
  await unlink("./out/dbg64/SumatraPDF.exe").catch(() => {});
  await $`msbuild .\\vs2022\\SumatraPDF.sln /t:SumatraPDF "/p:Configuration=Debug;Platform=x64" /m`;
}

const args = process.argv.slice(2);
if (args.includes("-wc")) {
  await wordCount();
} else if (args.includes("-fix-virt")) {
  await fixVirtualOverride();
} else if (args.includes("-build-preview2")) {
  buildPreview2();
} else {
  await build();
}
