// `SumatraPDF cli` read commands (src/shared/Cli.cpp): info, text, search and
// toc, as text and --json, the exit codes, an encrypted PDF with no, a wrong
// and the right password, and agent-skill.
//
// ng only for now: orig doesn't have the cli command yet.
// Run: bun tests/cli-read.ts [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { EXE, makePdf, makeTextPdf, runStandalone, tmpPath, USE_NG } from "./util.ts";

const name = "cli-read";

type Res = { code: number; out: string; err: string };

function cli(args: string[], env: Record<string, string> = {}): Res {
  const p = Bun.spawnSync([EXE, "cli", ...args], {
    stdout: "pipe",
    stderr: "pipe",
    env: { ...process.env, ...env },
  });
  return { code: p.exitCode ?? -1, out: p.stdout.toString(), err: p.stderr.toString() };
}

function expect(what: string, ok: boolean, res: Res): void {
  if (!ok) {
    throw new Error(`${name}: ${what}: exit ${res.code}\nstdout: ${res.out}\nstderr: ${res.err}`);
  }
}

function json(what: string, res: Res): any {
  try {
    return JSON.parse(res.out);
  } catch {
    throw new Error(`${name}: ${what}: not JSON: ${res.out}`);
  }
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const text = tmpPath("cli-read-text.pdf");
  writeFileSync(text, makeTextPdf("hello cli world, hello again"));
  const toc = tmpPath("cli-read-toc.pdf");
  writeFileSync(toc, makePdf(3, 400, 2, 0), "latin1");

  let r = cli(["info", text, "--json"]);
  expect("info --json", r.code === 0, r);
  const info = json("info", r);
  expect(`info pages=${info.pages} format=${info.format}`, info.pages === 1 && info.format === "pdf", r);

  r = cli(["text", text]);
  expect("text", r.code === 0 && r.out.includes("hello cli world"), r);

  r = cli(["search", text, "HELLO", "--json"]);
  const s = json("search", r);
  expect(`search count=${s.count}`, r.code === 0 && s.count === 2 && s.hits[0].page === 1, r);
  expect("search rect", Array.isArray(s.hits[0].rect) && s.hits[0].rect[2] > 0, r);
  r = cli(["search", text, "HELLO", "--match-case", "--json"]);
  expect("search --match-case finds nothing", json("search", r).count === 0, r);
  r = cli(["search", text, "hell", "--whole-word", "--json"]);
  expect("search --whole-word skips a part of a word", json("search", r).count === 0, r);

  r = cli(["toc", toc, "--json"]);
  const t = json("toc", r);
  expect(`toc ${JSON.stringify(t)}`, r.code === 0 && t.toc[0]?.title === "Target" && t.toc[0]?.page === 2, r);

  // exit codes
  r = cli(["info", tmpPath("cli-read-missing.pdf")]);
  expect("a missing file is 3", r.code === 3, r);
  r = cli(["bogus"]);
  expect("an unknown command is 2", r.code === 2, r);
  r = cli(["text", text, "--pages", "5", "--json"]);
  expect("a page past the end is 2", r.code === 2 && json("bad pages", r).error.code === 2, r);
  r = cli(["text", "--help"]);
  expect("--help", r.code === 0 && r.out.includes("examples:"), r);

  // an encrypted PDF
  const enc = tmpPath("cli-read-enc.pdf");
  const p = Bun.spawnSync([EXE, "clean", "-E", "aes-256", "-U", "pw", "-O", "owner", text, enc]);
  if (p.exitCode !== 0) {
    throw new Error(`${name}: encrypting failed: ${p.stderr.toString()}`);
  }
  r = cli(["info", enc]);
  expect("no password is 4", r.code === 4, r);
  r = cli(["info", enc, "--password", "bad"]);
  expect("a wrong password is 4", r.code === 4, r);
  r = cli(["text", enc, "--password-env", "CLI_READ_PW"], { CLI_READ_PW: "pw" });
  expect("--password-env", r.code === 0 && r.out.includes("hello cli world"), r);
  // the agent skill names this executable
  const skillDir = tmpPath("cli-read-skill");
  r = cli(["agent-skill", "--install", skillDir, "--json"]);
  expect("agent-skill --install", r.code === 0, r);
  const skill = readFileSync(json("agent-skill", r).file, "utf8");
  expect("the skill names the exe", skill.startsWith("---\nname: sumatrapdf") && skill.includes(`"${EXE}" cli`), r);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
