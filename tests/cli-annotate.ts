// `SumatraPDF cli annotate` (src/shared/Cli.cpp): words highlighted where
// they are (read back with `cli annotations`, which gives the text a
// highlight marks), a color, a note with its text, a shape at a rect; the
// input stays as it was; refusals.
//
// Run: bun tests/cli-annotate.ts [--no-build]

import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { EXE, makeTextPdf, ROOT, runStandalone, tmpPath } from "./util.ts";

const name = "cli-annotate";

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

function annotsOf(pdf: string): any[] {
  const r = cli(["annotations", pdf, "--json"]);
  expect(`annotations of ${pdf}`, r.code === 0, r);
  return JSON.parse(r.out).annotations;
}

export async function testit(): Promise<void> {
  const src = tmpPath("cli-annotate.pdf");
  writeFileSync(src, makeTextPdf("hello cli world, hello again"));
  const before = readFileSync(src);
  const out = (f: string) => tmpPath(`cli-annotate-${f}`);

  let r = cli(["annotate", src, "highlight", "--text", "hello", "-o", out("h.pdf"), "--json"]);
  expect("highlight", r.code === 0 && JSON.parse(r.out).count === 2, r);
  let a = annotsOf(out("h.pdf"));
  expect(`both 'hello's ${JSON.stringify(a)}`, a.length === 2 && a.every((x) => x.text?.trim() === "hello"), "");
  expect("yellow by default", a[0].color === "#ffff00", JSON.stringify(a));

  r = cli(["annotate", src, "underline", "--text", "cli world", "--color", "#ff0000", "-o", out("u.pdf")]);
  expect("underline", r.code === 0, r);
  a = annotsOf(out("u.pdf"));
  expect(`the underline ${JSON.stringify(a)}`, a[0]?.type === "Underline" && a[0]?.text?.trim() === "cli world", "");
  expect("its color", a[0].color === "#ff0000", JSON.stringify(a));

  r = cli(["annotate", src, "note", "--at", "300,100", "--contents", "check this", "-o", out("n.pdf")]);
  expect("note", r.code === 0, r);
  a = annotsOf(out("n.pdf"));
  expect(`the note ${JSON.stringify(a)}`, a[0]?.type === "Text" && a[0]?.contents === "check this", "");

  r = cli(["annotate", src, "square", "--rect", "100,200,50,40", "-o", out("s.pdf")]);
  expect("square", r.code === 0, r);
  const rect = annotsOf(out("s.pdf"))[0].rect;
  expect(`the square's rect ${rect}`, Math.abs(rect[0] - 100) < 3 && Math.abs(rect[2] - 50) < 4, "");

  expect("the input stays as it was", readFileSync(src).equals(before), "");

  // refusals
  expect("highlight without --text is 2", cli(["annotate", src, "highlight", "-o", out("x.pdf")]).code === 2, "");
  expect(
    "words not there is 2",
    cli(["annotate", src, "highlight", "--text", "nope", "-o", out("x.pdf")]).code === 2,
    "",
  );
  expect("an unknown type is 2", cli(["annotate", src, "sparkle", "-o", out("x.pdf")]).code === 2, "");
  expect(
    "a note without --at is 2",
    cli(["annotate", src, "note", "--contents", "x", "-o", out("x.pdf")]).code === 2,
    "",
  );
  const epub = join(ROOT, "tests", "issue-6007.epub");
  expect("an EPUB is 5", cli(["annotate", epub, "highlight", "--text", "a", "-o", out("x.pdf")]).code === 5, "");
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
