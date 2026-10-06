// Generates the src/ng/EmbeddedData*.cpp byte arrays: everything orig packs into
// the LzSA resource IDR_EMBEDDED_PAK (cmd/pack-embedded-prebuild.cmd) -
// translations.txt, marked.min.js, mermaid.min.js and mupdf's built-in fonts.
// A .rc resource is Windows-only, so here each file is gzipped into a generated
// C++ array and every platform links the same data; src/EmbeddedResources.cpp
// reads them back through GetEmbeddedFileData().
//
// Re-run after changing anything in res/ or the font list below. The
// translations are staged by cmd/ng-gen-translations.ts first, which adds an
// entry for every string of the sources res/translations.txt lacks.

import { gzipSync } from "bun";
import { existsSync, readdirSync, statSync } from "node:fs";
import { join, relative } from "path";
import { genDocs, manualOutDir } from "./ng-gen-docs";
import { genTranslations, translationsOutPath } from "./ng-gen-translations";

type Entry = {
  /** path relative to the repo root */
  src: string;
  /** name GetEmbeddedFileData() answers to */
  name: string;
};

type Group = {
  /** file written under src/ */
  out: string;
  /** the EmbeddedBlob table it defines */
  table: string;
  entries: Entry[];
};

const rootDir = join(import.meta.dir, "..");
const fontsDir = join("ext", "mupdf", "resources", "fonts");

// the forges mupdf's font table names (src/mupdf/noto_sumatra.c), same picks as
// orig's prebuild: base 14 (URW), CJK fallback (Droid), Charis SIL for EPUB and
// a few Noto for math / music / symbols / emoji. Not packed, as in orig:
// NimbusBoxes, Source Han and the per-script Noto fonts.
function fonts(forge: string, names: string[]): Entry[] {
  return names.map((n) => ({ src: join(fontsDir, forge, n), name: `fonts/${n}` }));
}

const urw = [
  "Dingbats.cff",
  "NimbusMonoPS-Bold.cff",
  "NimbusMonoPS-BoldItalic.cff",
  "NimbusMonoPS-Italic.cff",
  "NimbusMonoPS-Regular.cff",
  "NimbusRoman-Bold.cff",
  "NimbusRoman-BoldItalic.cff",
  "NimbusRoman-Italic.cff",
  "NimbusRoman-Regular.cff",
  "NimbusSans-Bold.cff",
  "NimbusSans-BoldItalic.cff",
  "NimbusSans-Italic.cff",
  "NimbusSans-Regular.cff",
  "StandardSymbolsPS.cff",
];

const sil = ["CharisSIL.cff", "CharisSIL-Bold.cff", "CharisSIL-BoldItalic.cff", "CharisSIL-Italic.cff"];

const noto = [
  "NotoSans-Regular.otf",
  "NotoSerif-Regular.otf",
  "NotoSansMath-Regular.otf",
  "NotoMusic-Regular.otf",
  "NotoSansSymbols-Regular.otf",
  "NotoSansSymbols2-Regular.otf",
  "NotoEmoji-Regular.ttf",
];

// the in-app manual (cmd/ng-gen-docs.ts stages it): every file under
// .work/docs, named "manual/<path>" with forward slashes, which is
// what LaunchDocumentation() asks GetEmbeddedFileData() for
function manualEntries(): Entry[] {
  const out: Entry[] = [];
  const walk = (dir: string) => {
    for (const e of readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
      const p = join(dir, e.name);
      if (e.isDirectory()) {
        walk(p);
        continue;
      }
      const rel = relative(manualOutDir, p).split("\\").join("/");
      out.push({ src: relative(rootDir, p), name: `manual/${rel}` });
    }
  };
  if (existsSync(manualOutDir)) walk(manualOutDir);
  return out;
}

const groups: Group[] = [
  {
    out: "EmbeddedDataText.cpp",
    table: "gEmbeddedText",
    entries: [
      // res/translations.txt completed by cmd/ng-gen-translations.ts
      { src: relative(rootDir, translationsOutPath), name: "translations.txt" },
      { src: join("ext", "marked.min.js"), name: "marked.min.js" },
    ],
  },
  {
    out: "EmbeddedDataMermaid.cpp",
    table: "gEmbeddedMermaid",
    entries: [{ src: join("ext", "mermaid.min.js"), name: "mermaid.min.js" }],
  },
  { out: "EmbeddedDataFontsUrw.cpp", table: "gEmbeddedFontsUrw", entries: fonts("urw", urw) },
  { out: "EmbeddedDataFontsSil.cpp", table: "gEmbeddedFontsSil", entries: fonts("sil", sil) },
  { out: "EmbeddedDataFontsNoto.cpp", table: "gEmbeddedFontsNoto", entries: fonts("noto", noto) },
  {
    out: "EmbeddedDataManual.cpp",
    table: "gEmbeddedManual",
    entries: [],
  },
  {
    out: "EmbeddedDataFontsDroid.cpp",
    table: "gEmbeddedFontsDroid",
    entries: fonts("droid", ["DroidSansFallbackFull.ttf"]),
  },
];

const bytesPerLine = 16;

function toByteArray(d: Uint8Array, out: string[]): void {
  for (let i = 0; i < d.length; i += bytesPerLine) {
    const n = Math.min(bytesPerLine, d.length - i);
    let line = "   ";
    for (let j = 0; j < n; j++) {
      line += " 0x" + d[i + j]!.toString(16).padStart(2, "0") + ",";
    }
    out.push(line + "\n");
  }
}

function cName(name: string): string {
  return name.replace(/[^A-Za-z0-9]/g, "_");
}

// the generated files are big (36 MB for 5.9 MB of data), so they are not
// checked in; cmd/ng-build.ts regenerates the ones whose input is newer, which is
// what orig's prebuild (cmd/pack-embedded-prebuild.cmd) does for the pak.
function isUpToDate(g: Group): boolean {
  const dst = join(rootDir, "src", "ng", g.out);
  if (!existsSync(dst)) return false;
  const out = statSync(dst).mtimeMs;
  if (statSync(join(import.meta.dir, "ng-gen-embedded.ts")).mtimeMs > out) return false;
  return g.entries.every((e) => statSync(join(rootDir, e.src)).mtimeMs <= out);
}

async function genGroup(g: Group): Promise<number> {
  const parts: string[] = [];
  parts.push(`/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// DO NOT EDIT. Generated by cmd/ng-gen-embedded.ts.

#include "base/Base.h"

#include "EmbeddedResources.h"

`);
  const rows: string[] = [];
  let total = 0;
  for (const e of g.entries) {
    const raw = new Uint8Array(await Bun.file(join(rootDir, e.src)).arrayBuffer());
    const gz = gzipSync(raw, { level: 9 });
    total += gz.length;
    const v = `g_${cName(e.name)}`;
    parts.push(`static const u8 ${v}[${gz.length}] = {\n`);
    toByteArray(gz, parts);
    parts.push("};\n\n");
    rows.push(`    {"${e.name}", ${v}, ${gz.length}, ${raw.length}},\n`);
  }
  parts.push(`const EmbeddedBlob ${g.table}[] = {\n`);
  if (rows.length === 0) rows.push("    {},\n");
  parts.push(...rows);
  parts.push("};\n\n");
  parts.push(`const int ${g.table}Count = ${g.entries.length};\n`);
  const dst = join(rootDir, "src", "ng", g.out);
  await Bun.write(dst, parts.join(""));
  console.log(`src/ng/${g.out}: ${g.entries.length} file(s), ${total} bytes gzipped`);
  return total;
}

/** Regenerates the src/EmbeddedData*.cpp whose input changed. */
export async function genEmbedded(): Promise<void> {
  const manualChanged = await genDocs();
  // staged before the up-to-date check below looks at its time stamp
  genTranslations();
  const manual = groups.find((g) => g.table === "gEmbeddedManual")!;
  manual.entries = manualEntries();
  let all = 0;
  let generated = 0;
  if (manualChanged) {
    all += await genGroup(manual);
    generated++;
  }
  for (const g of groups) {
    if (isUpToDate(g)) continue;
    all += await genGroup(g);
    generated++;
  }
  if (generated > 0) console.log(`embedded: ${generated} file(s), ${all} bytes gzipped`);
}

if (import.meta.main) await genEmbedded();
