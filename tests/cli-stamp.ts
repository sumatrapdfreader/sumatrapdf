// `SumatraPDF cli watermark` and `cli number` (src/shared/Cli.cpp,
// EngineMupdfStampText): a watermark across the middle of the pages, rising
// from the bottom left as the page shows (a rotated page too); page numbers
// where asked, as the page shows; only on the pages asked for; any script
// (system fonts, shaped, right to left); refusals.
//
// Run: bun tests/cli-stamp.ts [--no-build]

import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, loadPng, makePdf, ROOT, runStandalone, tmpPath } from "./util.ts";

const name = "cli-stamp";

type Res = { code: number; out: string; err: string };

function cli(args: string[]): Res {
  const p = Bun.spawnSync([EXE, "cli", ...args], { stdout: "pipe", stderr: "pipe" });
  return { code: p.exitCode ?? -1, out: p.stdout.toString(), err: p.stderr.toString() };
}

function expect(what: string, ok: boolean, res: Res | string): void {
  if (!ok) {
    const s = typeof res === "string" ? res : `exit ${res.code}\nstdout: ${res.out}\nstderr: ${res.err}`;
    throw new Error(`${name}: ${what}:\n${s}`);
  }
}

// the red pixels of a page as rendered, by quarter of the page
function redQuarters(pdf: string, page: number, png: string): { tl: number; tr: number; bl: number; br: number } {
  const r = cli(["render", pdf, "--pages", `${page}`, "--dpi", "30", "-o", png]);
  expect("render", r.code === 0, r);
  const img = loadPng(png);
  const q = { tl: 0, tr: 0, bl: 0, br: 0 };
  for (let y = 0; y < img.h; y++) {
    for (let x = 0; x < img.w; x++) {
      const i = (y * img.w + x) * img.nComp;
      const [rr, gg, bb] = [img.data[i]!, img.data[i + 1]!, img.data[i + 2]!];
      if (rr > 200 && gg < 90 && bb < 90) {
        const top = y < img.h / 2;
        const left = x < img.w / 2;
        if (top) {
          left ? q.tl++ : q.tr++;
        } else {
          left ? q.bl++ : q.br++;
        }
      }
    }
  }
  return q;
}

// along the rising diagonal: the bottom left and top right quarters have it,
// the other two little of it
function expectRising(what: string, q: { tl: number; tr: number; bl: number; br: number }): void {
  const on = Math.min(q.bl, q.tr);
  const off = Math.max(q.tl, q.br);
  expect(`${what}: rising diagonal ${JSON.stringify(q)}`, on > 50 && off * 3 < on, "");
}

// where `text` is on a page, in points from the top left as it shows
function boxOf(pdf: string, page: number, text: string): number[] {
  const r = cli(["search", pdf, text, "--pages", `${page}`, "--json"]);
  const hits = JSON.parse(r.out).hits;
  expect(`'${text}' on page ${page}`, hits.length === 1, r);
  return hits[0].rect;
}

function near(what: string, got: number, want: number, tolerance: number): void {
  expect(`${what}: ${got}, want ${want}`, Math.abs(got - want) <= tolerance, "");
}

function testNumber(src: string, out: (f: string) => string): void {
  // Letter: 612 x 792; the baseline 30pt above the bottom by default
  let r = cli(["number", src, "--format", "Page {n} of {total}", "-o", out("n.pdf"), "--json"]);
  expect("number", r.code === 0 && JSON.parse(r.out).pages === 2, r);
  const [x, y, dx, dy] = boxOf(out("n.pdf"), 2, "Page 2 of 2");
  near("bottom-center: the middle", x! + dx! / 2, 306, 3);
  near("bottom-center: the bottom", y! + dy!, 792 - 30, 6);

  // a page turned 90 degrees shows as 792 x 612: top-right is its top right
  r = cli(["number", out("rot.pdf"), "--format", "Page {n}", "--position", "top-right", "-o", out("nr.pdf")]);
  expect("number a rotated page", r.code === 0, r);
  const [rx, ry, rdx] = boxOf(out("nr.pdf"), 1, "Page 1");
  near("top-right: the right", rx! + rdx!, 792 - 30, 3);
  near("top-right: the top", ry!, 30, 5);

  // --pages and --start: page 2 is numbered 5, of 5; page 1 not at all
  r = cli(["number", src, "--pages", "2", "--start", "5", "--format", "Page {n} of {total}", "-o", out("n5.pdf")]);
  expect("--pages --start", r.code === 0, r);
  boxOf(out("n5.pdf"), 2, "Page 5 of 5");
  expect("page 1 has no number", !cli(["text", out("n5.pdf"), "--pages", "1"]).out.includes("Page "), "");

  expect("a bad position is 2", cli(["number", src, "--position", "middle", "-o", out("x.pdf")]).code === 2, "");
}

// text Helvetica lacks: drawn in a system font, shaped, searchable
function testScripts(src: string, out: (f: string) => string): void {
  const text = "ไทย বাংলা हिन्दी العربية Zażółć";
  let r = cli(["watermark", src, text, "--color", "#ff0000", "--opacity", "1", "-o", out("ws.pdf")]);
  expect("watermark in other scripts", r.code === 0, r);
  expectRising("other scripts", redQuarters(out("ws.pdf"), 1, out("ws1.png")));
  for (const word of ["ไทย", "বাংলা", "Zażółć"]) {
    r = cli(["text", out("ws.pdf"), "--pages", "1"]);
    expect(`the watermark has '${word}' as text`, r.out.includes(word), r);
  }
  // on a blank page: where the ink is (search boxes under ActualText are rough)
  writeFileSync(out("blank.pdf"), makePdf(1, 612, 0, 0), "latin1");
  r = cli(["number", out("blank.pdf"), "--format", "หน้า {n}", "-o", out("ns.pdf")]);
  expect("Thai page numbers", r.code === 0, r);
  expect("the page number is text", cli(["text", out("ns.pdf")]).out.includes("หน้า 1"), "");
  r = cli(["render", out("ns.pdf"), "--dpi", "72", "-o", out("ns.png")]);
  const img = loadPng(out("ns.png"));
  let x0 = img.w;
  let x1 = -1;
  let y1 = -1;
  for (let y = 0; y < img.h; y++) {
    for (let x = 0; x < img.w; x++) {
      if (img.data[(y * img.w + x) * img.nComp]! < 128) {
        x0 = Math.min(x0, x);
        x1 = Math.max(x1, x + 1);
        y1 = Math.max(y1, y + 1);
      }
    }
  }
  near("Thai bottom-center: the middle", (x0 + x1) / 2, 306, 2);
  near("Thai bottom-center: the bottom", y1, 792 - 30, 2);
}

export async function testit(): Promise<void> {
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const dir = tmpPath("cli-stamp");
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  const out = (f: string) => join(dir, f);

  let r = cli(["watermark", src, "DRAFT", "--color", "#ff0000", "--opacity", "1", "-o", out("w.pdf"), "--json"]);
  expect("watermark", r.code === 0 && JSON.parse(r.out).pages === 2, r);
  r = cli(["text", out("w.pdf"), "--pages", "1"]);
  expect("the watermark is text on the page", r.out.includes("DRAFT"), r);
  expectRising("page 1", redQuarters(out("w.pdf"), 1, out("w1.png")));

  // a page turned 90 degrees: the watermark goes as the page shows
  cli(["pages", "rotate", src, "1", "90", "-o", out("rot.pdf")]);
  r = cli(["watermark", out("rot.pdf"), "DRAFT", "--color", "#ff0000", "--opacity", "1", "-o", out("wr.pdf")]);
  expect("watermark a rotated page", r.code === 0, r);
  expectRising("the rotated page", redQuarters(out("wr.pdf"), 1, out("wr.png")));

  r = cli(["watermark", src, "DRAFT", "--pages", "2", "-o", out("p2.pdf")]);
  expect("--pages 2", r.code === 0, r);
  expect("not on page 1", !cli(["text", out("p2.pdf"), "--pages", "1"]).out.includes("DRAFT"), "");
  expect("on page 2", cli(["text", out("p2.pdf"), "--pages", "2"]).out.includes("DRAFT"), "");

  // refusals
  expect("a bad color is 2", cli(["watermark", src, "A", "--color", "red", "-o", out("x.pdf")]).code === 2, "");
  expect("opacity 2 is 2", cli(["watermark", src, "A", "--opacity", "2", "-o", out("x.pdf")]).code === 2, "");
  expect("over the input is 2", cli(["watermark", src, "A", "-o", src]).code === 2, "");
  const epub = join(ROOT, "tests", "issue-6007.epub");
  expect("an EPUB is 5", cli(["watermark", epub, "A", "-o", out("x.pdf")]).code === 5, "");
  testNumber(src, out);
  testScripts(src, out);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
