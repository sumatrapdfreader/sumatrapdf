// issue #6140: a markdown file whose name has a space ("Test Test.md") opened
// as a WebView2 404: Navigate() was given https://sumatrapdf.markdown/Test Test.html
// (invalid URI). WebView2 then asked for Test%20Test.html and showed
// "A webpage cannot be found on that web address".
//
// The page is served if we can scroll to a heading far down the document — a
// 404 page has nothing to scroll. Folder names with a space already worked
// (they never appear in the virtual URL); the file name is what is in the path.
//
// Covered for both embedded browsers (-html-backend): the IE control resolves
// the same url through its own its:// protocol handler, so it can 404 on its
// own. Names beyond the reported space cover the rest of the url-special
// characters a Windows file name may hold, which break the same way: a raw '#'
// in the name used to make everything after it read as a fragment.

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ControlClient, ControlCommand, withControlledSumatra } from "./control.ts";
import { EXE, runStandalone, SLOW_BUILD_FACTOR, tmpPath } from "./util.ts";

const BACKENDS = ["webview2", "ie"];
const MD_NAMES = ["Test Test.md", "100% & C#1 (v2)+[a]@b;c=d!.md", "Ünïcode Tëst.md"];
const TARGET_DEST_NO = 3; // the file, "Start", then "Target Heading"
const MIN_TARGET_SCROLL_Y = 500;

async function tocNavigate(client: ControlClient, what: string, destNo: number, expected: string): Promise<string> {
  const deadline = Date.now() + 20_000 * SLOW_BUILD_FACTOR;
  let last = "";
  for (;;) {
    const res = await client.request(ControlCommand.TestMarkdownTocNavigate, [destNo, MIN_TARGET_SCROLL_Y]);
    const exitCode = res[0] as number;
    const output = String(res[1] ?? "").trim();
    last = output;
    if (exitCode === 0 && output.startsWith(expected)) {
      return output;
    }
    if (exitCode !== 2 || !output.startsWith("NOTREADY")) {
      throw new Error(`issue-6140: ${what}: navigating failed: ${output}`);
    }
    if (Date.now() > deadline) {
      throw new Error(`issue-6140: ${what}: never rendered (404?): ${last}`);
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
}

// each name gets its own folder so the TOC always has the same shape
function makeDoc(dir: string, idx: number, mdName: string): string {
  // folder-with-space is the reporter's path; it is not the bug, but keep it
  const folder = join(dir, `test test ${idx}`);
  mkdirSync(folder, { recursive: true });
  const paragraphs = Array.from(
    { length: 120 },
    (_, i) => `Paragraph ${i + 1}: enough content to put the target heading below the viewport.`,
  );
  const path = join(folder, mdName);
  writeFileSync(path, ["# Start", ...paragraphs, "## Target Heading", "Target content."].join("\n\n"));
  return path;
}

export async function testit(): Promise<void> {
  const dir = tmpPath("issue-6140-data");
  rmSync(dir, { recursive: true, force: true });
  const mdPaths = MD_NAMES.map((name, i) => makeDoc(dir, i, name));

  const appdata = tmpPath("issue-6140-appdata");
  rmSync(appdata, { recursive: true, force: true });
  mkdirSync(appdata, { recursive: true });
  writeFileSync(
    join(appdata, "SumatraPDF-settings.txt"),
    ["MarkdownUI [", "\tUseFixedPageUI = false", "]", "RestoreSession = false", "ShowStartPage = false", ""].join("\n"),
  );

  for (const backend of BACKENDS) {
    for (let i = 0; i < mdPaths.length; i++) {
      const what = `${backend}: '${MD_NAMES[i]}'`;
      await withControlledSumatra(
        EXE,
        async (client) => {
          const started = await tocNavigate(client, what, TARGET_DEST_NO, "NAVIGATING");
          if (!started.includes("target-heading")) {
            throw new Error(`issue-6140: ${what}: dest ${TARGET_DEST_NO} is not the target heading: ${started}`);
          }
          const landed = await tocNavigate(client, what, 0, "OK");
          console.log(`issue-6140: ${what} ${landed}`);
        },
        ["-appdata", appdata, "-html-backend", backend, mdPaths[i]],
      );
    }
  }

  console.log("issue-6140: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
