// Renders every page of the OFD samples in the shared test-files folder and
// reports failed pages and OFD warnings. Run: bun tests/ad-hoc-ofd-corpus.ts [--no-build] [filter]

import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

const samplesDir = "C:/Users/kjk/OneDrive/!sumatra/!test files/ofd";

// lines mupdf logs when it skips or substitutes OFD content
const ofdWarning = /OFD|ofd/;

export async function testit(): Promise<void> {
  if (!existsSync(samplesDir)) {
    console.log(`skipping: no samples in ${samplesDir}`);
    return;
  }
  const filter = process.argv.slice(2).find((a) => !a.startsWith("-"));
  const files = readdirSync(samplesDir)
    .filter((name) => name.toLowerCase().endsWith(".ofd"))
    .filter((name) => !filter || name.includes(filter));
  const logPath = tmpPath("ofd-corpus-log.txt");
  const failures: string[] = [];
  let pagesOk = 0;

  await withControlledSumatra(
    EXE,
    async (client) => {
      for (const name of files) {
        const path = join(samplesDir, name);
        const logStart = existsSync(logPath) ? readFileSync(logPath, "utf8").length : 0;
        let failed = 0;
        let pageNo = 1;
        for (; ; pageNo++) {
          const result = await client.request(ControlCommand.TestRenderPageColors, [path, pageNo, 50]);
          const raw = String(result[1] ?? "").trim();
          if (raw.includes("bad-page")) break;
          if (result[0] === 0) {
            pagesOk++;
            continue;
          }
          failed++;
          failures.push(`${name} page ${pageNo}: ${raw}`);
          if (raw.includes("engine-create-failed")) break;
        }
        const log = existsSync(logPath) ? readFileSync(logPath, "utf8").slice(logStart) : "";
        const warnings = [...new Set(log.split(/\r?\n/).filter((l) => ofdWarning.test(l)))];
        const status = failed ? `FAILED ${failed}/${pageNo - 1}` : `ok ${pageNo - 1} pages`;
        console.log(`${status}  ${name}`);
        for (const w of warnings.slice(0, 5)) console.log(`    ${w.trim()}`);
      }
    },
    ["-log-to-file", logPath],
  );

  console.log(`\n${pagesOk} pages rendered, ${failures.length} failed`);
  if (failures.length) {
    throw new Error(failures.join("\n"));
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
