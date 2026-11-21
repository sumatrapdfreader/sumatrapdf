// `SumatraPDF cli markdown` (src/shared/Cli.cpp): headings (from bold lines
// and from the table of contents), emphasis with the spaces around it kept,
// page markers, pictures written next to the .md, --json and --pages.
//
// ng only for now: orig doesn't have the cli command yet.
// Run: bun tests/cli-markdown.ts [--no-build]

import { existsSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";

const name = "cli-markdown";

type Res = { code: number; out: string; err: string };

function cli(args: string[]): Res {
  const p = Bun.spawnSync([EXE, "cli", ...args], { stdout: "pipe", stderr: "pipe" });
  return { code: p.exitCode ?? -1, out: p.stdout.toString(), err: p.stderr.toString() };
}

function expect(what: string, ok: boolean, res: Res): void {
  if (!ok) {
    throw new Error(`${name}: ${what}: exit ${res.code}\nstdout: ${res.out.slice(0, 3000)}\nstderr: ${res.err}`);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  // a man page: bold section titles in the body's size, italic words
  const man = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  let r = cli(["markdown", man]);
  expect("markdown", r.code === 0, r);
  expect("page markers", r.out.startsWith("<!-- page 1 -->") && r.out.includes("<!-- page 2 -->"), r);
  expect("a bold line is a heading", /^# NAME$/m.test(r.out) && /^# DESCRIPTION$/m.test(r.out), r);
  // the brackets are escaped: [see ...] could be a link
  expect("emphasis keeps its spaces", r.out.includes("\\[see *zlib.h* for full description\\]"), r);
  expect("no NUL bytes", !r.out.includes("\0"), r);

  r = cli(["markdown", man, "--pages", "2", "--no-page-markers", "--json"]);
  const j = JSON.parse(r.out);
  expect("--pages --json", r.code === 0 && j.pages === 1 && !j.markdown.includes("<!-- page"), r);

  // an EPUB: the chapter title is in the table of contents
  r = cli(["markdown", join(ROOT, "tests", "issue-6095.epub")]);
  expect("a title from the table of contents is a heading", /^# Dzikie ziele$/m.test(r.out), r);

  // pictures go to <name>_images next to the markdown, linked relatively
  const dir = tmpPath("cli-markdown");
  rmSync(dir, { recursive: true, force: true });
  const md = join(dir, "doc.md");
  r = cli(["markdown", join(ROOT, "tests", "issue-6132.pdf"), "-o", md, "--json"]);
  expect("markdown -o", r.code === 0, r);
  const out = JSON.parse(r.out);
  expect(`images ${JSON.stringify(out.images)}`, out.images.length > 0 && existsSync(out.images[0]), r);
  const text = readFileSync(md, "utf8");
  expect("an image link", /!\[\]\(doc_images\/page-\d+-\d+\.png\)/.test(text), r);
  r = cli(["markdown", join(ROOT, "tests", "issue-6132.pdf"), "-o", join(dir, "none.md"), "--no-images"]);
  expect("--no-images", r.code === 0 && !existsSync(join(dir, "none_images")), r);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
