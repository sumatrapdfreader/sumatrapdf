// Regression test for discussion #6279: renaming (F2) a file opened from a
// UNC path left the tab showing the old name instead of the renamed document.
//
// Uses the administrative share of the repo's drive (\\localhost\d$\...), so
// it skips where that share is not reachable. The rename goes through the
// TestRenameFile control command, which is Rename File without its dialog.
//
// Run:  bun tests/issue-6279.ts [--no-build]

import { existsSync, rmSync, writeFileSync } from "node:fs";
import { basename } from "node:path";
import { makeMinimalPdf, pollUntil, runStandalone, tmpPath } from "./util.ts";
import { launchControlled, waitForTitle } from "./win-automation.ts";

const kOldName = "issue-6279-old";
const kNewName = "issue-6279-new";

// D:\dir\file.pdf => \\localhost\d$\dir\file.pdf
function toUncPath(path: string): string {
  return `\\\\localhost\\${path[0].toLowerCase()}$${path.slice(2)}`;
}

export async function testit(): Promise<void> {
  const oldPath = tmpPath(`${kOldName}.pdf`);
  const newPath = tmpPath(`${kNewName}.pdf`);
  const oldUnc = toUncPath(oldPath);
  const newUnc = toUncPath(newPath);
  rmSync(newPath, { force: true });
  writeFileSync(oldPath, makeMinimalPdf("issue 6279"));

  if (!existsSync(oldUnc)) {
    console.log(`\nSKIP issue-6279: '${oldUnc}' is not reachable (needs the administrative share)`);
    return;
  }

  const { proc, client, frame } = await launchControlled([oldUnc]);
  try {
    await client.waitForRenderIdle();

    await client.renameFile(newUnc);

    await pollUntil(
      () => existsSync(newPath) && !existsSync(oldPath),
      (renamed) => renamed,
      { timeoutMs: 10_000, error: "the file was not renamed on disk" },
    );
    await waitForTitle(frame, (title) => title.includes(basename(newPath)));
    // fails when the tab is stuck loading / failed to load
    await client.waitForRenderIdle();
  } finally {
    await client.quit();
    rmSync(oldPath, { force: true });
    rmSync(newPath, { force: true });
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}
