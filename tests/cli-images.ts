// `SumatraPDF cli images` (src/shared/Cli.cpp): a PDF's JPEG comes out as the
// same bytes, pictures without a format of their own as PNG, a comic's pages
// and an EPUB's pictures too; --pages. `cli images-to-pdf`: a page per image,
// JPEG, PNG and WebP.
//
// ng only for now: orig doesn't have the cli command yet.
// Run: bun tests/cli-images.ts [--no-build]

import { readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { assemblePdf, EXE, ROOT, runStandalone, tmpPath, USE_NG } from "./util.ts";

const name = "cli-images";

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

// a 2-page PDF: the JPEG on page 1, a gray image the PDF stores as raw
// samples (no format of its own) on page 2
function pdfWithImages(jpeg: Buffer, w: number, h: number): string {
  const raw = "80".repeat(16 * 16);
  const objs = [
    "<< /Type /Catalog /Pages 2 0 R >>",
    "<< /Type /Pages /Kids [3 0 R 5 0 R] /Count 2 >>",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 400] /Resources << /XObject << /Im0 7 0 R >> >> /Contents 4 0 R >>",
    "",
    "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 400] /Resources << /XObject << /Im1 8 0 R >> >> /Contents 6 0 R >>",
    "",
    "",
    `<< /Type /XObject /Subtype /Image /Width 16 /Height 16 /ColorSpace /DeviceGray /BitsPerComponent 8 /Filter /ASCIIHexDecode /Length ${raw.length + 1} >>\nstream\n${raw}>\nendstream`,
  ];
  const draw = (im: string) => `q 200 0 0 200 100 100 cm /${im} Do Q`;
  objs[3] = `<< /Length ${draw("Im0").length} >>\nstream\n${draw("Im0")}\nendstream`;
  objs[5] = `<< /Length ${draw("Im1").length} >>\nstream\n${draw("Im1")}\nendstream`;
  objs[6] =
    `<< /Type /XObject /Subtype /Image /Width ${w} /Height ${h} /ColorSpace /DeviceRGB /BitsPerComponent 8 ` +
    `/Filter /DCTDecode /Length ${jpeg.length} >>\nstream\n${jpeg.toString("latin1")}\nendstream`;
  return assemblePdf(objs);
}

export async function testit(): Promise<void> {
  if (!USE_NG) {
    console.log(`SKIP ${name}: ng only (run with -ng)`);
    return;
  }
  const dir = tmpPath(name);
  rmSync(dir, { recursive: true, force: true });
  // a JPEG to put in a PDF: a page rendered as one
  let r = cli([
    "render",
    join(ROOT, "ext", "a-zlib", "zlib.3.pdf"),
    "--dpi",
    "20",
    "-o",
    join(dir, "src.jpg"),
    "--json",
  ]);
  expect("render a JPEG", r.code === 0, r);
  const { width, height } = JSON.parse(r.out).images[0];
  const jpeg = readFileSync(join(dir, "src.jpg"));
  const pdf = join(dir, "pics.pdf");
  writeFileSync(pdf, pdfWithImages(jpeg, width, height), "latin1");

  r = cli(["images", pdf, "-o", join(dir, "out"), "--json"]);
  expect("images", r.code === 0, r);
  const imgs = JSON.parse(r.out).images;
  expect(`2 images ${r.out}`, imgs.length === 2 && imgs[0].page === 1 && imgs[1].page === 2, r);
  expect("the JPEG is a .jpg", imgs[0].file.endsWith("pics-p1-1.jpg"), r);
  expect("the JPEG's bytes as they were", readFileSync(imgs[0].file).equals(jpeg), r);
  expect("raw samples as PNG", imgs[1].file.endsWith(".png"), r);
  expect("a PNG", readFileSync(imgs[1].file).subarray(1, 4).toString() === "PNG", r);

  r = cli(["images", pdf, "--pages", "2", "-o", join(dir, "p2")]);
  expect("--pages 2", r.code === 0 && readdirSync(join(dir, "p2")).length === 1, r);

  // repeated: a page prefetch still running at exit crashed about every other run
  for (let i = 0; i < 10; i++) {
    rmSync(join(dir, "cbz"), { recursive: true, force: true });
    r = cli(["images", join(ROOT, "tests", "issue-1201.cbz"), "--pages", "1-2", "-o", join(dir, "cbz")]);
    expect(`a comic's pages (run ${i + 1})`, r.code === 0 && readdirSync(join(dir, "cbz")).length === 2, r);
  }
  r = cli(["images", join(ROOT, "tests", "issue-6007.epub"), "-o", join(dir, "epub")]);
  expect("an EPUB's pictures", r.code === 0 && readdirSync(join(dir, "epub")).length > 0, r);
  expect("no -o is 2", cli(["images", pdf]).code === 2, "");

  // images-to-pdf: a JPEG, a PNG and a WebP
  const src = join(ROOT, "ext", "a-zlib", "zlib.3.pdf");
  const imgs3: string[] = [];
  for (const ext of ["jpg", "png"]) {
    const f = join(dir, `page.${ext}`);
    expect(`render ${ext}`, cli(["render", src, "--dpi", "24", "-o", f]).code === 0, "");
    imgs3.push(f);
  }
  imgs3.push(join(ROOT, "ext", "mupdf", "docs", "images", "200x200-rect.webp"));
  const made = join(dir, "made.pdf");
  r = cli(["images-to-pdf", ...imgs3, "-o", made, "--json"]);
  expect("images-to-pdf", r.code === 0 && JSON.parse(r.out).pages === 3, r);
  expect("3 pages", JSON.parse(cli(["info", made, "--json"]).out).pages === 3, "");
  r = cli(["images-to-pdf", imgs3[0]!, src, "-o", join(dir, "x.pdf")]);
  expect("a PDF isn't an image: 3", r.code === 3, r);
  console.log(`${name}: OK`);
}

if (import.meta.main) {
  await runStandalone(testit);
}
