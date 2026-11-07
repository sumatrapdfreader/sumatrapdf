// Test for https://github.com/sumatrapdfreader/sumatrapdf/issues/6500
//
// Chinese EPUB text was set too tight compared to a browser:
// - a font-family that doesn't resolve fell back to mupdf's Latin serif font,
//   so punctuation inside Chinese text (“” ……) was half width. CJK runs in a
//   generic family now use the script's system font (mupdf patch 0085)
// - "zw", a font name Chinese EPUB tools use, now maps to Microsoft YaHei, and
//   to the system's CJK font on Linux and macOS
// - justified lines were only spread at spaces, which Chinese has none of, so
//   they ended ragged. They are now spread between the characters too
//
// Chapter 1 names no font, chapter 2 names "zw". Each is one paragraph that
// fits on one page.
//
// Needs a Chinese system font: Windows and macOS have one. On a Linux system
// without one, fontconfig is pointed at mupdf's Source Han Serif for the run (its
// SC face has full-width quotes, like Microsoft YaHei; Droid Sans Fallback's aren't).
// For ng on Linux pass -exe out/linux/dbg/SumatraPDF.
//
// Run:  bun tests/issue-6500.ts [--no-build]   (or via tests/run-almost-all.ts)

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { IS_MAC, IS_WIN } from "./host.ts";
import { EXE, makeZip, ROOT, runStandalone, tmpPath, type ZipEntry } from "./util.ts";

const name = "issue-6500";
const kOpenQuote = 0x201c;
// a full-width quote is 1em, the Latin one about 0.4em
const kMinQuoteRatio = 0.8;
// how far the right edges of a paragraph's justified lines may differ, in pt
const kMaxRaggedness = 2;

const sentence = "今天天气很好，我们一起去公园散步。她说：“这里的风景真美！”我们在湖边坐了很久，看着小船慢慢地划过。";
const chapters = [
  `<p style="text-align:justify">${sentence.repeat(6)}</p>`,
  `<p style="text-align:justify;font-family:zw">${sentence.repeat(6)}</p>`,
];

function makeEpub(): Buffer {
  const enc = new TextEncoder();
  const container =
    `<?xml version="1.0"?>\n<container version="1.0" ` +
    `xmlns="urn:oasis:names:tc:opendocument:xmlns:container"><rootfiles>` +
    `<rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/>` +
    `</rootfiles></container>`;
  const items = chapters.map((_, i) => `<item id="c${i}" href="c${i}.xhtml" media-type="application/xhtml+xml"/>`);
  const refs = chapters.map((_, i) => `<itemref idref="c${i}"/>`);
  const opf =
    `<?xml version="1.0" encoding="utf-8"?>\n` +
    `<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">` +
    `<metadata xmlns:dc="http://purl.org/dc/elements/1.1/">` +
    `<dc:identifier id="id">${name}</dc:identifier>` +
    `<dc:title>${name}</dc:title><dc:language>zh-CN</dc:language></metadata>` +
    `<manifest>${items.join("")}</manifest><spine>${refs.join("")}</spine></package>`;
  const xhtml = (body: string) =>
    `<?xml version="1.0" encoding="utf-8"?>\n<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="zh-CN">` +
    `<head><title>t</title></head><body>${body}</body></html>`;
  const entries: ZipEntry[] = [
    { name: "mimetype", data: enc.encode("application/epub+zip"), store: true },
    { name: "META-INF/container.xml", data: enc.encode(container) },
    { name: "OEBPS/content.opf", data: enc.encode(opf) },
    ...chapters.map((body, i) => ({ name: `OEBPS/c${i}.xhtml`, data: enc.encode(xhtml(body)) })),
  ];
  return makeZip(entries);
}

type Box = { cp: number; x: number; y: number; dx: number };

// environment for the app: on Linux without a Chinese font, a fontconfig
// config that adds mupdf's CJK font to the system's
function appEnv(): Record<string, string | undefined> {
  if (IS_WIN || IS_MAC) {
    return process.env;
  }
  const have = Bun.spawnSync(["fc-list", ":lang=zh"]);
  if (have.exitCode === 0 && have.stdout.toString().trim().length > 0) {
    return process.env;
  }
  const dir = tmpPath(`${name}-fontconfig`);
  mkdirSync(dir, { recursive: true });
  const han = join(ROOT, "ext", "mupdf", "resources", "fonts", "han");
  const conf = join(dir, "fonts.conf");
  writeFileSync(
    conf,
    `<?xml version="1.0"?>\n<!DOCTYPE fontconfig SYSTEM "fonts.dtd">\n<fontconfig>` +
      `<include ignore_missing="yes">/etc/fonts/fonts.conf</include><dir>${han}</dir>` +
      `<cachedir>${join(dir, "cache")}</cachedir>` +
      // every face claims Chinese; distros' CJK font packages ship a rule like this
      `<match target="pattern"><test name="lang" compare="contains"><string>zh-cn</string></test>` +
      `<edit name="family" mode="prepend" binding="strong"><string>Source Han Serif SC</string></edit></match>` +
      `</fontconfig>\n`,
  );
  return { ...process.env, FONTCONFIG_FILE: conf };
}

function pageBoxes(epub: string, pageNo: number, env: Record<string, string | undefined>): Box[] {
  const p = Bun.spawnSync([EXE, "-for-testing", "-extract-text", String(pageNo), epub], {
    stdout: "pipe",
    stderr: "pipe",
    env,
  });
  const raw = p.stdout.toString() + p.stderr.toString();
  const m = new RegExp(`boxes on page ${pageNo}:(.*)`).exec(raw);
  if (!m) {
    throw new Error(`${name}: no boxes line (debug build?). out=${raw.slice(0, 500).replace(/\s+/g, " ")}`);
  }
  return m[1]
    .trim()
    .split(/\s+/)
    .filter((s) => s.length > 0)
    .map((s) => {
      const [cp, rest] = s.split(":");
      const [x, y, dx] = rest.split(",").map((n) => parseInt(n, 10));
      return { cp: parseInt(cp, 10), x, y, dx };
    });
}

function isHan(cp: number): boolean {
  return cp >= 0x4e00 && cp <= 0x9fff;
}

function median(a: number[]): number {
  const s = [...a].sort((x, y) => x - y);
  return s[Math.floor(s.length / 2)];
}

function check(epub: string, pageNo: number, what: string, env: Record<string, string | undefined>): void {
  const boxes = pageBoxes(epub, pageNo, env).filter((b) => b.dx > 0);
  const hanDx = median(boxes.filter((b) => isHan(b.cp)).map((b) => b.dx));
  const quotes = boxes.filter((b) => b.cp === kOpenQuote);
  if (!hanDx || quotes.length === 0) {
    throw new Error(`${name}: ${what}: no Chinese text or quotes on page ${pageNo}`);
  }
  for (const q of quotes) {
    if (q.dx < hanDx * kMinQuoteRatio) {
      throw new Error(`${name}: ${what}: “ is ${q.dx}pt wide, a Chinese char ${hanDx}pt: not full width`);
    }
  }

  // right edge of each line; all but the paragraph's last are justified
  const rights = new Map<number, number>();
  for (const b of boxes) {
    rights.set(b.y, Math.max(rights.get(b.y) ?? 0, b.x + b.dx));
  }
  const lines = [...rights.entries()].sort((a, b) => a[0] - b[0]).map((e) => e[1]);
  const justified = lines.slice(0, -1);
  if (justified.length < 3) {
    throw new Error(`${name}: ${what}: expected several lines, got ${lines.length}`);
  }
  const spread = Math.max(...justified) - Math.min(...justified);
  if (spread > kMaxRaggedness) {
    throw new Error(`${name}: ${what}: justified lines end ${spread}pt apart: ${justified.join(" ")}`);
  }
  console.log(`PASS ${name}: ${what}: “ ${quotes[0].dx}pt vs ${hanDx}pt, line ends within ${spread}pt`);
}

export async function testit(): Promise<void> {
  const epub = tmpPath(`${name}.epub`);
  writeFileSync(epub, makeEpub());
  const env = appEnv();
  check(epub, 1, "no font-family", env);
  check(epub, 2, 'font-family "zw"', env);
}

if (import.meta.main) {
  await runStandalone(testit);
}
