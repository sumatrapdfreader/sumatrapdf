// Build tests/issue-1201.cbz from tests/issue-1201-data/.
// Regenerate after editing ComicInfo.xml or page images.
//
// Run: bun tests/issue-1201-make-fixture.ts

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT } from "./util.ts";

const DATA = join(ROOT, "tests", "issue-1201-data");
const OUT = join(ROOT, "tests", "issue-1201.cbz");

// 1x1 transparent PNG
const PNG = Buffer.from(
  "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==",
  "base64",
);

mkdirSync(DATA, { recursive: true });
for (let i = 1; i <= 5; i++) {
  writeFileSync(join(DATA, `${String(i).padStart(3, "0")}.png`), PNG);
}

rmSync(OUT, { force: true });
const proc = Bun.spawnSync(
  [
    "powershell",
    "-NoProfile",
    "-Command",
    `Compress-Archive -Path '${DATA.replace(/'/g, "''")}\\*' -DestinationPath '${OUT.replace(/'/g, "''")}' -Force`,
  ],
  { stdout: "pipe", stderr: "pipe" },
);
if (proc.exitCode !== 0) {
  const err = new TextDecoder().decode(proc.stderr);
  throw new Error(`failed to create ${OUT}: ${err}`);
}
console.log(`Created ${OUT}`);