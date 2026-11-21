// `SumatraPDF cli annotations` and `cli render` (src/shared/Cli.cpp): a
// highlight comes with the text it marks and a note with its contents; an
// EPUB has no annotations (exit 5); render writes one image per page with
// <N> for the page number.
//
// ng only for now: orig doesn't have the cli command yet.
// Run: bun tests/cli-annotations-render.ts [--no-build]

import { existsSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, EXE, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";

const name = "cli-annotations-render";

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

// "hello world" in 18pt Helvetica at (72, 700), a highlight over "hello"
// (38pt wide) and a note
function makeAnnotatedPdf(): string {
  const content = "BT /F1 18 Tf 72 700 Td (hello world) Tj ET";
  return assemblePdf([
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Resources << /Font << /F1 4 0 R >> >> " +
      "/Contents 5 0 R /Annots [6 0 R 7 0 R] >>",
    "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>",
    `<< /Length ${content.length} >>\nstream\n${content}\nendstream`,
    "<< /Type /Annot /Subtype /Highlight /Rect [71 696 111 718] /QuadPoints [71 718 111 718 71 696 111 696] " +
      "/C [1 1 0] /T (kjk) /P 3 0 R >>",
    "<< /Type /Annot /Subtype /Text /Rect [300 700 320 720] /Contents (a note) /T (kjk) /P 3 0 R >>",
  ]);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const pdf = tmpPath("cli-annotated.pdf");
  writeFileSync(pdf, makeAnnotatedPdf(), "latin1");

  let r = cli(["annotations", pdf, "--json"]);
  expect("annotations", r.code === 0, r);
  const a = JSON.parse(r.out);
  const hl = a.annotations.find((x: any) => x.type === "Highlight");
  const note = a.annotations.find((x: any) => x.type === "Text");
  expect(`highlight ${JSON.stringify(hl)}`, hl?.text?.trim() === "hello" && hl?.color === "#ffff00", r);
  expect(`note ${JSON.stringify(note)}`, note?.contents === "a note" && note?.author === "kjk", r);

  r = cli(["annotations", join(ROOT, "tests", "issue-6007.epub")]);
  expect("an EPUB has no annotations: 5", r.code === 5, r);

  const dir = tmpPath("cli-render");
  rmSync(dir, { recursive: true, force: true });
  const pages = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  r = cli(["render", pages, "--pages", "1-2", "--dpi", "36", "-o", join(dir, "p<N>.png"), "--json"]);
  expect("render", r.code === 0, r);
  const imgs = JSON.parse(r.out).images;
  expect(`render ${JSON.stringify(imgs)}`, imgs.length === 2 && imgs[0].width === 306, r);
  expect("render wrote the files", existsSync(join(dir, "p1.png")) && existsSync(join(dir, "p2.png")), r);
  r = cli(["render", pages, "-o", join(dir, "x.bogus")]);
  expect("an unknown image type is 2", r.code === 2, r);
  r = cli(["render", pages]);
  expect("render without -o is 2", r.code === 2, r);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
