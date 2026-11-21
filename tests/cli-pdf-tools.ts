// `SumatraPDF cli` PDF tools (src/shared/Cli.cpp): pages keep / delete /
// order / rotate, merge, split, compress, decompress, encrypt and decrypt. Each
// writes a new PDF, never the input, and refuses an EPUB (exit 5).
//
// ng only for now: orig doesn't have the cli command yet.
// Run: bun tests/cli-pdf-tools.ts [--no-build]

import { readdirSync, readFileSync, rmSync, mkdirSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";
import { EXE, makePdf, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";

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

// split: a PDF per page by default, numbers padded so the files sort; --every;
// --ranges
function testSplit(dir: string): void {
  const src = join(dir, "twelve.pdf");
  writeFileSync(src, makePdf(12, 300, 0, 0), "latin1");
  const pagesOf = (f: string) => JSON.parse(cli(["info", f, "--json"]).out).pages;

  let r = cli(["split", src, "-o", join(dir, "each")]);
  expect("split", r.code === 0, r);
  const each = readdirSync(join(dir, "each")).sort();
  expect(`a PDF per page ${JSON.stringify(each)}`, each.length === 12 && each[0] === "twelve-01.pdf", r);

  r = cli(["split", src, "--every", "5", "-o", join(dir, "fives"), "--json"]);
  const files = JSON.parse(r.out).files.map((f: any) => [basename(f.file), f.pages]);
  const want = [
    ["twelve-01-05.pdf", 5],
    ["twelve-06-10.pdf", 5],
    ["twelve-11-12.pdf", 2],
  ];
  expect(`--every 5 ${JSON.stringify(files)}`, JSON.stringify(files) === JSON.stringify(want), r);
  expect("the last one has 2 pages", pagesOf(join(dir, "fives", "twelve-11-12.pdf")) === 2, r);

  r = cli(["split", src, "--ranges", "1-3;10-N", "-o", join(dir, "ranges")]);
  expect("--ranges", r.code === 0 && readdirSync(join(dir, "ranges")).length === 2, r);
  expect("--ranges 10-N has 3 pages", pagesOf(join(dir, "ranges", "twelve-10-12.pdf")) === 3, r);
  expect("a bad range is 2", cli(["split", src, "--ranges", "1-3;20", "-o", join(dir, "x")]).code === 2, r);
  expect("no -o is 2", cli(["split", src]).code === 2, r);
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
  testSplit(dir);
  if (!readFileSync(src).equals(before)) {
    throw new Error(`${name}: the input changed`);
  }
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
