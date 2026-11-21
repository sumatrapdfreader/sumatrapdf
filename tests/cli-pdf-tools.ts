// `SumatraPDF cli` PDF tools (src/shared/Cli.cpp): pages keep / delete /
// order / rotate, merge, compress, decompress, encrypt and decrypt. Each
// writes a new PDF, never the input, and refuses an EPUB (exit 5).
//
// ng only for now: orig doesn't have the cli command yet.
// Run: bun tests/cli-pdf-tools.ts [--no-build]

import { readFileSync, rmSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";

const name = "cli-pdf-tools";

type Res = { code: number; out: string; err: string };

function cli(args: string[]): Res {
  const p = Bun.spawnSync([EXE, "cli", ...args], { stdout: "pipe", stderr: "pipe" });
  return { code: p.exitCode ?? -1, out: p.stdout.toString(), err: p.stderr.toString() };
}

function expect(what: string, ok: boolean, res: Res): void {
  if (!ok) {
    throw new Error(`${name}: ${what}: exit ${res.code}\nstdout: ${res.out}\nstderr: ${res.err}`);
  }
}

// the page count and first page's text of a PDF, read back through the cli
function pdfInfo(path: string, extra: string[] = []): { pages: number; width: number; text: string } {
  const info = JSON.parse(cli(["info", path, "--json", ...extra]).out);
  const text = JSON.parse(cli(["text", path, "--json", ...extra]).out);
  return { pages: info.pages, width: info.pageSize.width, text: text.pages.map((p: any) => p.text).join("\f") };
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  // 2 pages, US Letter
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const before = readFileSync(src);
  const dir = tmpPath("cli-pdf-tools");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const out = (f: string) => join(dir, f);
  const p1 = pdfInfo(src).text.split("\f");

  let r = cli(["pages", "keep", src, "2", "-o", out("keep.pdf"), "--json"]);
  expect("pages keep", r.code === 0 && JSON.parse(r.out).pages === 1, r);
  expect("kept page 2", pdfInfo(out("keep.pdf")).text === p1[1], r);

  r = cli(["pages", "delete", src, "2", "-o", out("del.pdf")]);
  expect("pages delete", r.code === 0 && pdfInfo(out("del.pdf")).text === p1[0], r);

  r = cli(["pages", "order", src, "2,1,2", "-o", out("order.pdf")]);
  const order = pdfInfo(out("order.pdf")).text.split("\f");
  expect("pages order", r.code === 0 && order.length === 3 && order[0] === p1[1] && order[1] === p1[0], r);

  r = cli(["pages", "rotate", src, "1", "90", "-o", out("rot.pdf")]);
  expect("pages rotate turns page 1 sideways", r.code === 0 && pdfInfo(out("rot.pdf")).width === 792, r);

  r = cli(["merge", src, out("order.pdf"), "-o", out("merged.pdf"), "--json"]);
  expect("merge", r.code === 0 && JSON.parse(r.out).pages === 5, r);

  r = cli(["compress", src, "-o", out("small.pdf"), "--json"]);
  const c = JSON.parse(r.out);
  expect(`compress ${r.out}`, r.code === 0 && c.bytes <= c.inputBytes && pdfInfo(out("small.pdf")).pages === 2, r);
  r = cli(["decompress", src, "-o", out("big.pdf")]);
  expect("decompress", r.code === 0 && pdfInfo(out("big.pdf")).pages === 2, r);

  r = cli(["encrypt", src, "-o", out("locked.pdf"), "--new-password", "pw"]);
  expect("encrypt", r.code === 0, r);
  expect("the encrypted PDF needs the password", cli(["info", out("locked.pdf")]).code === 4, r);
  r = cli(["decrypt", out("locked.pdf"), "-o", out("open.pdf"), "--password", "pw"]);
  expect("decrypt", r.code === 0 && cli(["info", out("open.pdf")]).code === 0, r);

  // refusals
  r = cli(["compress", src, "-o", src]);
  expect("writing over the input is 2", r.code === 2, r);
  r = cli(["compress", src]);
  expect("no -o is 2", r.code === 2, r);
  r = cli(["compress", join(ROOT, "tests", "issue-6007.epub"), "-o", out("x.pdf")]);
  expect("an EPUB is 5", r.code === 5, r);
  r = cli(["pages", "delete", src, "1-2", "-o", out("none.pdf")]);
  expect("deleting every page is 2", r.code === 2, r);
  r = cli(["pages", "rotate", src, "1", "45", "-o", out("x.pdf")]);
  expect("45 degrees is 2", r.code === 2, r);
  if (!readFileSync(src).equals(before)) {
    throw new Error(`${name}: the input changed`);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
