// Test for issue #1201: TOC from ComicInfo.xml bookmarks in CBZ/CBR files.
//
// Uses the committed tests/issue-1201.cbz fixture. Optionally also checks files
// listed in tests/tmp/issue-1201-comics.txt (from ad-hoc-find-comicinfo.ts).

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, ROOT, runStandalone, tmpPath } from "./util.ts";

const FIXTURE = join(ROOT, "tests", "issue-1201.cbz");
const COMICS_LIST = tmpPath("issue-1201-comics.txt");

const EXPECTED_FIXTURE = ["Cover|page=1", "Chapter 1|page=3", "Chapter 2|page=5"].join("\n") + "\n";

async function expectOpensWithToc(path: string, label: string): Promise<void> {
  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestGetToc, [path]);
  if (exitCode !== 0) {
    throw new Error(`issue-1201: ${label} failed: ${(raw ?? "").trim()}`);
  }
  const lines = (raw ?? "").trim().split("\n").filter((l) => l.length > 0);
  if (lines.length === 0) {
    throw new Error(`issue-1201: ${label} has empty TOC`);
  }
  console.log(`issue-1201: ${label}: ${lines.length} TOC entries`);
}

export async function testit(): Promise<void> {
  if (!existsSync(FIXTURE)) {
    throw new Error(`issue-1201: missing fixture ${FIXTURE} (run tests/issue-1201-make-fixture.ts)`);
  }

  const [exitCode, raw] = await runControlCommand(EXE, ControlCommand.TestGetToc, [FIXTURE]);
  if (exitCode !== 0) {
    throw new Error(`issue-1201: fixture TOC failed: ${(raw ?? "").trim()}`);
  }
  const got = raw ?? "";
  if (got !== EXPECTED_FIXTURE) {
    throw new Error(`issue-1201: fixture TOC mismatch.\nexpected:\n${EXPECTED_FIXTURE}got:\n${got}`);
  }
  console.log("issue-1201: fixture TOC OK");

  if (existsSync(COMICS_LIST)) {
    const paths = readFileSync(COMICS_LIST, "utf8")
      .split(/\r?\n/)
      .map((l) => l.trim())
      .filter((l) => l.length > 0);
    if (paths.length === 0) {
      console.log(`  SKIP: ${COMICS_LIST} is empty`);
      return;
    }
    for (const p of paths.slice(0, 10)) {
      if (!existsSync(p)) {
        console.log(`  SKIP: missing ${p}`);
        continue;
      }
      await expectOpensWithToc(p, p);
    }
  } else {
    console.log(`  SKIP: no ${COMICS_LIST} (run: bun tests/ad-hoc-find-comicinfo.ts)`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}