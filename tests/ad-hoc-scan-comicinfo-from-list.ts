// Scan paths listed in tests/tmp/all-cbz.txt for ComicInfo.xml.
// Run after: cmd /c "dir /s /b x:\backup\comics\*.cbz x:\backup\comics\*.cbr" > tests/tmp/all-cbz.txt
//
// Run: bun tests/ad-hoc-scan-comicinfo-from-list.ts

import { closeSync, createReadStream, openSync, readSync, statSync, writeFileSync } from "node:fs";
import { createInterface } from "node:readline";
import { join } from "node:path";
import { tmpPath } from "./util.ts";

const LIST = tmpPath("all-cbz.txt");
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

async function main(): Promise<void> {
  const found: string[] = [];
  let checked = 0;
  const rl = createInterface({ input: createReadStream(LIST), crlfDelay: Infinity });
  for await (const line of rl) {
    const p = line.trim();
    if (!p) {
      continue;
    }
    checked++;
    try {
      if (hasComicInfo(p)) {
        found.push(p);
        console.log(`found: ${p}`);
        if (found.length >= MAX) {
          break;
        }
      }
    } catch {
      // skip
    }
    if (checked % 1000 === 0) {
      console.log(`checked ${checked}, found ${found.length}`);
    }
  }
  writeFileSync(OUT, found.join("\n") + (found.length ? "\n" : ""), "utf8");
  console.log(`checked ${checked}, wrote ${found.length} paths to ${OUT}`);
}

await main();