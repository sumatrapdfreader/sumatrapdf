// A .txt file in a legacy encoding (GBK, Big5, Shift-JIS, cp1251) or UTF-16
// with a BOM shows its text. The bytes used to be wrapped in HTML as they were
// and read as UTF-8, so such files showed U+FFFD garbage, and a UTF-16 BOM was
// no longer at the start of the buffer for mupdf to see.
//
// Run:  bun tests/txt-encoding.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

// Encodes text in a legacy multi-byte encoding by inverting TextDecoder over
// every one- and two-byte sequence.
function makeEncoder(encoding: string): (s: string) => Buffer {
  const dec = new TextDecoder(encoding);
  const map = new Map<string, number[]>();
  for (let b = 0; b < 0x100; b++) {
    const c = dec.decode(new Uint8Array([b]));
    if (c.length === 1 && c !== "�" && !map.has(c)) {
      map.set(c, [b]);
    }
  }
  for (let lead = 0x81; lead <= 0xfe; lead++) {
    for (let trail = 0x40; trail <= 0xfe; trail++) {
      const c = dec.decode(new Uint8Array([lead, trail]));
      if (c.length === 1 && c !== "�" && !map.has(c)) {
        map.set(c, [lead, trail]);
      }
    }
  }
  return (s: string) => {
    const out: number[] = [];
    for (const c of s) {
      const bytes = map.get(c);
      if (!bytes) {
        throw new Error(`txt-encoding: can't encode ${JSON.stringify(c)} in ${encoding}`);
      }
      out.push(...bytes);
    }
    return Buffer.from(out);
  };
}

function utf16(s: string, bigEndian: boolean): Buffer {
  const le = Buffer.from(s, "utf16le");
  if (bigEndian) {
    le.swap16();
  }
  return Buffer.concat([Buffer.from(bigEndian ? [0xfe, 0xff] : [0xff, 0xfe]), le]);
}

const cases: { name: string; text: string; encode: (s: string) => Buffer }[] = [
  { name: "gbk", text: "这是一个用简体中文写的文本文件。我们希望它能正确显示。", encode: makeEncoder("gbk") },
  { name: "big5", text: "這是一個用繁體中文寫的文字檔案。我們希望它能正確顯示。", encode: makeEncoder("big5") },
  {
    name: "shift-jis",
    text: "これは日本語で書かれたテキストファイルです。正しく表示されることを願っています。",
    encode: makeEncoder("shift_jis"),
  },
  {
    name: "cp1251",
    text: "Это текстовый файл, написанный по-русски. Мы надеемся, что он отображается правильно.",
    encode: makeEncoder("windows-1251"),
  },
  { name: "utf16le", text: "Hello été, ünïcödé text in UTF-16.", encode: (s) => utf16(s, false) },
  { name: "utf16be", text: "Hello été, ünïcödé text in UTF-16.", encode: (s) => utf16(s, true) },
  {
    name: "utf8-bom",
    text: "Hello été, ünïcödé text in UTF-8.",
    encode: (s) => Buffer.concat([Buffer.from([0xef, 0xbb, 0xbf]), Buffer.from(s)]),
  },
];

export async function testit(): Promise<void> {
  const failed: string[] = [];
  for (const c of cases) {
    const path = tmpPath(`txt-encoding-${c.name}.txt`);
    writeFileSync(path, c.encode(`${c.text}\n`.repeat(10)));
    const r = Bun.spawnSync([EXE, "cli", "text", path]);
    const out = r.stdout.toString();
    if (r.exitCode !== 0 || !out.includes(c.text)) {
      failed.push(`${c.name}: exit ${r.exitCode}, got ${JSON.stringify(out.slice(0, 80))}`);
    }
  }
  if (failed.length > 0) {
    throw new Error(`txt-encoding:\n${failed.join("\n")}`);
  }
  console.log("txt-encoding: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
