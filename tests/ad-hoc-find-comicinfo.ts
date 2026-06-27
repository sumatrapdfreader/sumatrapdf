// Scan x:\backup\comics for .cbz/.cbr archives containing ComicInfo.xml.
// Writes paths to tests/tmp/issue-1201-comics.txt (one per line).
//
// Run: bun tests/ad-hoc-find-comicinfo.ts

import { closeSync, openSync, readdirSync, readSync, statSync, writeFileSync } from "node:fs";
import { extname, join } from "node:path";
import { tmpPath } from "./util.ts";

const ROOT = "x:/backup/comics";
const OUT = tmpPath("issue-1201-comics.txt");
const MAX = 50;

function hasComicInfo(path: string): boolean {
  const fd = openSync(path, "r");
  try {
    const stat = statSync(path);
    const isCbz = path.toLowerCase().endsWith(".cbz");
    const scanSize = Math.min(stat.size, isCbz ? 65557 : 524288);
    const buf = Buffer.alloc(scanSize);
    const offset = isCbz ? Math.max(0, stat.size - scanSize) : 0;
    readSync(fd, buf, 0, scanSize, offset);
    return buf.indexOf("ComicInfo.xml") !== -1;
  } finally {
    closeSync(fd);
  }
}

const found: string[] = [];

function walk(dir: string, depth = 0): void {
  if (found.length >= MAX || depth > 8) {
    return;
  }
  let entries;
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return;
  }
  for (const e of entries) {
    if (found.length >= MAX) {
      break;
    }
    const p = join(dir, e.name);
    if (e.isDirectory()) {
      walk(p, depth + 1);
    } else if (e.isFile()) {
      const ext = extname(e.name).toLowerCase();
      if ((ext === ".cbz" || ext === ".cbr") && hasComicInfo(p)) {
        found.push(p);
      }
    }
  }
}

walk(ROOT);
writeFileSync(OUT, found.join("\n") + (found.length ? "\n" : ""), "utf8");
console.log(`Wrote ${found.length} paths to ${OUT}`);
for (const p of found) {
  console.log(p);
}