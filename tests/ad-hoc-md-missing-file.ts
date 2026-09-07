// Opening a .md path that doesn't exist used to open a document anyway: the
// markdown model collects every sibling .md of the named directory, so a typo
// under a large tree loaded hundreds of pages and showed the missing file as a
// browser 404. It must fail to load, like a missing file of any other format:
// the tab stays empty (page 0) and reports the error.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, tmpPath } from "./util.ts";

async function currentPageNo(client: ControlClient): Promise<number> {
  const res = await client.request(ControlCommand.TestCurrentTab, []);
  const out = String(res[1] ?? "").trim();
  if (res[0] !== 0) {
    throw new Error(`ad-hoc-md-missing-file: current tab: ${out}`);
  }
  const m = /page=(\d+)$/.exec(out);
  if (!m) {
    throw new Error(`ad-hoc-md-missing-file: could not parse: ${out}`);
  }
  return parseInt(m[1]!, 10);
}

export async function testit(): Promise<void> {
  const dir = tmpPath("ad-hoc-md-missing-file");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  // a sibling the old code would have collected and opened instead
  writeFileSync(join(dir, "sibling.md"), "# Sibling\n");

  await withControlledSumatra(
    EXE,
    async (client) => {
      const pageNo = await currentPageNo(client);
      if (pageNo !== 0) {
        throw new Error(`ad-hoc-md-missing-file: a missing .md loaded a document (page ${pageNo})`);
      }
    },
    [join(dir, "missing.md")],
  );

  console.log("ad-hoc-md-missing-file: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
