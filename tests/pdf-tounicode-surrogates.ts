// The ToUnicode map mupdf writes for a font it embeds (text stamps, typed
// text) must give characters outside the BMP (emoji, rare CJK) as UTF-16
// surrogate pairs. It wrote the code point as 5 hex digits, "<1f600>", which
// isn't UTF-16, so such text couldn't be copied or searched in the saved PDF
// (ext/patches/0102-pdf-tounicode-surrogates).
//
// Run:  bun tests/pdf-tounicode-surrogates.ts [--no-build]   (or via tests/run-almost-all.ts)

import { existsSync, writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

const FONT = "C:/Windows/Fonts/seguiemj.ttf";

const SCRIPT = `
var doc = new PDFDocument();
var font = doc.addFont(new Font("emoji", scriptArgs[0]));
print("CMAP-START");
print(font.ToUnicode.readStream().asString());
print("CMAP-END");
`;

export async function testit(): Promise<void> {
  if (!existsSync(FONT)) {
    console.log(`SKIP pdf-tounicode-surrogates: no ${FONT}`);
    return;
  }
  const script = tmpPath("pdf-tounicode-surrogates.js");
  writeFileSync(script, SCRIPT);
  const r = Bun.spawnSync([EXE, "run", script, FONT]);
  const out = r.stdout.toString() + r.stderr.toString();
  const cmap = /CMAP-START\n([\s\S]*)CMAP-END/.exec(out.replaceAll("\r\n", "\n"))?.[1];
  if (r.exitCode !== 0 || !cmap) {
    throw new Error(`pdf-tounicode-surrogates: exit ${r.exitCode}\n${out.slice(0, 2000)}`);
  }

  // destination values: the last <hex> of each bfchar / bfrange line
  const bad: string[] = [];
  for (const line of cmap.split("\n")) {
    const hex = [...line.matchAll(/<([0-9a-fA-F]+)>/g)].map((m) => m[1]!);
    if (hex.length < 2) {
      continue;
    }
    const dst = hex[hex.length - 1]!;
    if (dst.length !== 4 && dst.length !== 8) {
      bad.push(line.trim());
    }
  }
  if (bad.length > 0) {
    throw new Error(
      `pdf-tounicode-surrogates: ${bad.length} values aren't UTF-16, e.g.\n${bad.slice(0, 3).join("\n")}`,
    );
  }
  // emoji (U+1F3xx..U+1F7xx) as surrogate pairs <d83c..d83d xxxx>
  if (!/<d83[cd][0-9a-f]{4}>/i.test(cmap)) {
    throw new Error("pdf-tounicode-surrogates: no emoji surrogate pair (<d83d....>) in the ToUnicode map");
  }
  console.log("pdf-tounicode-surrogates: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
