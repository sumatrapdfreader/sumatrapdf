// Amalgamates a vendored third-party library into a single .c/.cpp plus its
// public headers under ext/a-<lib>/. One library per run, selected by a -<lib>
// flag. Everything except the per-library recipe (source list, include rules,
// cl.exe flags) is shared.
import { cpSync, existsSync, mkdirSync, readdirSync, readFileSync, rmSync, statSync, writeFileSync } from "node:fs";
import { basename, dirname, join, normalize, relative } from "node:path";
import { detectVisualStudio2026, runLogged } from "./util";

// ---------------------------------------------------------------------------
// generic text / file helpers
// ---------------------------------------------------------------------------

function readText(path: string): string {
  return readFileSync(path, "utf-8").replace(/\r\n/g, "\n");
}

function normPath(path: string): string {
  return normalize(path).replace(/\\/g, "/");
}

function listFiles(dir: string, ext?: string): string[] {
  return readdirSync(dir)
    .map((name) => join(dir, name))
    .filter((path) => statSync(path).isFile())
    .filter((path) => !ext || path.endsWith(ext))
    .sort((a, b) => basename(a).localeCompare(basename(b)));
}

function listFilesRec(dir: string): string[] {
  return readdirSync(dir, { withFileTypes: true })
    .flatMap((entry) => {
      const path = join(dir, entry.name);
      return entry.isDirectory() ? listFilesRec(path) : [path];
    })
    .sort();
}

function mapByName(paths: string[]): Map<string, string> {
  return new Map(paths.map((path) => [basename(path), path]));
}

// Finds the directory holding the library sources: tries each relative
// candidate under root and returns the first one containing all markers.
function findSrcDir(root: string, candidates: string[], markers: string[]): string {
  for (const rel of candidates) {
    const dir = rel ? join(root, rel) : root;
    if (existsSync(dir) && markers.every((m) => existsSync(join(dir, m)))) {
      return dir;
    }
  }
  throw new Error(`could not find ${markers.join(", ")} under ${root}`);
}

// Removes comments while preserving line count-ish structure and string/char
// literals. A hand-rolled scanner because the sources aren't preprocessed.
function stripComments(text: string): string {
  let out = "";
  let i = 0;
  let state: "code" | "line" | "block" | "string" | "char" = "code";
  while (i < text.length) {
    const c = text[i];
    const n = text[i + 1];
    if (state === "code") {
      if (c === "/" && n === "/") {
        state = "line";
        i += 2;
        continue;
      }
      if (c === "/" && n === "*") {
        state = "block";
        i += 2;
        continue;
      }
      if (c === '"') {
        state = "string";
      } else if (c === "'") {
        state = "char";
      }
      out += c;
      i++;
      continue;
    }
    if (state === "line") {
      if (c === "\n") {
        out += "\n";
        state = "code";
      }
      i++;
      continue;
    }
    if (state === "block") {
      if (c === "\n") {
        out += "\n";
      }
      if (c === "*" && n === "/") {
        state = "code";
        i += 2;
      } else {
        i++;
      }
      continue;
    }
    out += c;
    if ((state === "string" || state === "char") && c === "\\") {
      out += n ?? "";
      i += 2;
      continue;
    }
    if (state === "string" && c === '"') {
      state = "code";
    } else if (state === "char" && c === "'") {
      state = "code";
    }
    i++;
  }
  return out;
}

function normalizeBlankLines(text: string): string {
  return (
    text
      .replace(/[ \t]+$/gm, "")
      .replace(/\n{3,}/g, "\n\n")
      .trim() + "\n"
  );
}

// ---------------------------------------------------------------------------
// local #include handling
// ---------------------------------------------------------------------------

type ResolveInclude = (fromPath: string, inc: string) => string | undefined;

// How to treat `#include "..."` lines. Checked in order: drop (delete the
// line), keep (emit verbatim), then resolve+inline. Unresolved includes are
// emitted verbatim.
type IncludeRules = {
  resolve?: ResolveInclude;
  keep?: (inc: string) => boolean;
  drop?: (inc: string) => boolean;
  // Shared across chunks so a header is inlined only once per amalgamation.
  seen?: Set<string>;
  // Rely on the sources' include guards instead of `seen`: inline a header at
  // every occurrence and skip only a self-referential one, exactly as the
  // preprocessor would. Needed when an include sits inside an #ifdef, where
  // deduping would drop it from the one place it is actually reachable.
  guarded?: boolean;
  // Narrows `seen` to headers whose include guard wraps the whole file. A
  // header that closes its guard right after opening it ("dummy header guard")
  // is an X-macro fragment meant to be expanded once per includer, so deduping
  // it would drop it from every place but the first.
  dedupOnlyGuarded?: boolean;
  // Wrap inlined text in /* begin x */ ... /* end x */ markers.
  markers?: boolean;
};

const includeRe = /^\s*#\s*include\s+"([^"]+)"/;

const fullFileGuardCache = new Map<string, boolean>();

// True when `#ifndef G` / `#define G` at the top is not immediately closed.
function hasFullFileGuard(path: string): boolean {
  const cached = fullFileGuardCache.get(path);
  if (cached !== undefined) {
    return cached;
  }

  const lines = stripComments(readText(path))
    .split("\n")
    .map((line) => line.trim())
    .filter(Boolean);
  const m = /^#\s*ifndef\s+(\w+)$/.exec(lines[0] ?? "");
  const guard = m?.[1];
  const opened = guard !== undefined && new RegExp(`^#\\s*define\\s+${guard}\\b`).test(lines[1] ?? "");
  const result = opened && !/^#\s*endif\b/.test(lines[2] ?? "");

  fullFileGuardCache.set(path, result);
  return result;
}

function expandIncludes(path: string, rules: IncludeRules, stack: string[] = []): string {
  const out: string[] = [];
  for (const line of readText(path).split("\n")) {
    const m = includeRe.exec(line);
    if (!m) {
      out.push(line);
      continue;
    }

    const inc = m[1];
    if (rules.drop?.(inc)) {
      continue;
    }
    if (rules.keep?.(inc)) {
      out.push(line);
      continue;
    }

    const incPath = rules.resolve?.(path, inc);
    if (!incPath) {
      out.push(line);
      continue;
    }
    if (rules.seen?.has(incPath)) {
      continue;
    }
    if (stack.includes(incPath)) {
      if (!rules.guarded) {
        throw new Error(`include cycle: ${[...stack, incPath].join(" -> ")}`);
      }
      continue;
    }
    if (!rules.dedupOnlyGuarded || hasFullFileGuard(incPath)) {
      rules.seen?.add(incPath);
    }

    if (rules.markers) {
      out.push(`/* begin ${inc} */`);
    }
    out.push(expandIncludes(incPath, rules, [...stack, path]));
    if (rules.markers) {
      out.push(`/* end ${inc} */`);
    }
  }
  return out.join("\n");
}

// Resolves an include against a flat basename -> path map.
function byNameResolver(byName: Map<string, string>): ResolveInclude {
  return (_fromPath, inc) => byName.get(inc);
}

// Resolves an include relative to the including file, then against dirs.
function dirResolver(dirs: string[]): ResolveInclude {
  return (fromPath, inc) => {
    for (const dir of [dirname(fromPath), ...dirs]) {
      const candidate = normPath(join(dir, inc));
      if (existsSync(candidate)) {
        return candidate;
      }
    }
    return undefined;
  };
}

function inSet(names: Iterable<string>, matchBasename = false): (inc: string) => boolean {
  const set = new Set(names);
  return (inc) => set.has(inc) || (matchBasename && set.has(basename(inc)));
}

// Reads one source file into its amalgamated form. `rules` omitted means local
// includes are left alone; `transform` runs after comment stripping.
function prepare(path: string, rules?: IncludeRules, transform?: (text: string) => string): string {
  const raw = rules ? expandIncludes(path, rules) : readText(path);
  const text = stripComments(raw);
  return normalizeBlankLines(transform ? transform(text) : text);
}

// Gives a file's local statics a unique name so they don't collide with a
// same-named static from another file once everything is one translation unit.
function scopeSymbols(text: string, prefix: string, names: string[]): string {
  const defs = names.map((name) => `#define ${name} ${prefix}_${name}`);
  const undefs = names.map((name) => `#undef ${name}`);
  return [...defs, text, ...undefs].join("\n") + "\n";
}

// Undefines a file's macros at the end of its chunk so they don't leak into a
// later one that defines the same name differently.
function sealMacros(text: string, names: string[]): string {
  return text + "\n" + names.map((name) => `#undef ${name}`).join("\n") + "\n";
}

function joinChunks(chunks: string[]): string {
  return normalizeBlankLines(chunks.join("\n"));
}

// ---------------------------------------------------------------------------
// git
// ---------------------------------------------------------------------------

function gitOutput(args: string[], cwd: string, required = true): string {
  const proc = Bun.spawnSync(["git", ...args], { cwd, stdout: "pipe", stderr: "pipe" });
  if (proc.exitCode !== 0) {
    if (required) {
      throw new Error(proc.stderr.toString().trim());
    }
    return "";
  }
  return proc.stdout.toString().trim();
}

function normalizeGithubUrl(repo: string): string | undefined {
  let url = repo.trim();
  const remoteMatch = /^git@github\.com:([^/]+\/[^/]+?)(?:\.git)?$/.exec(url);
  if (remoteMatch) {
    return `https://github.com/${remoteMatch[1]}`;
  }

  url = url.replace(/^ssh:\/\/git@github\.com\//, "https://github.com/");
  if (!url.startsWith("https://github.com/")) {
    return undefined;
  }
  return url.replace(/\.git$/, "").replace(/\/$/, "");
}

// ---------------------------------------------------------------------------
// library recipes
// ---------------------------------------------------------------------------

type Ctx = {
  // where the upstream checkout lives
  checkoutDir: string;
  // relative path -> generated text, written to both tmpDir and outDir
  files: Map<string, string>;
};

type Lib = {
  name: string;
  homepage?: string;
  repo: string;
  rev: string;
  // gumbo may be pointed at a local directory instead of a git URL
  allowLocalRepo?: boolean;
  // one-line summary of what gets written, shown in usage
  writes: string;
  generate: (ctx: Ctx) => void;
  // files copied verbatim from the checkout into outDir (licenses etc.)
  copies?: string[];
  // wipe outDir before writing (libs whose header set can shrink upstream)
  wipeOutDir?: boolean;
  // the amalgamated file to compile, plus its extra cl.exe flags
  compile: { file: string; args: string[] };
};

const clCommonArgs = ["/nologo", "/c", "/W4", "/WX", "/O2", "/MT"];

// --- unrar -----------------------------------------------------------------

// global.cpp comes first on purpose: it defines INCLUDEGLOBAL before pulling in
// rar.hpp, which is what turns global.hpp's `EXTVAR ErrorHandler ErrHandler`
// into the definition rather than a declaration. rar.hpp is inlined once, so
// whichever chunk pulls it in first decides that.
const unrarSources = [
  "global.cpp",
  "archive.cpp",
  "arcread.cpp",
  "blake2s.cpp",
  "cmddata.cpp",
  "consio.cpp",
  "crc.cpp",
  "crypt.cpp",
  "dll.cpp",
  "encname.cpp",
  "errhnd.cpp",
  "extinfo.cpp",
  "extract.cpp",
  "filcreat.cpp",
  "file.cpp",
  "filefn.cpp",
  "filestr.cpp",
  "find.cpp",
  "getbits.cpp",
  "hash.cpp",
  "headers.cpp",
  "isnt.cpp",
  "largepage.cpp",
  "list.cpp",
  "match.cpp",
  "motw.cpp",
  "options.cpp",
  "pathfn.cpp",
  "qopen.cpp",
  "rarvm.cpp",
  "rawread.cpp",
  "rdwrfn.cpp",
  "recvol.cpp",
  "rijndael.cpp",
  "rs.cpp",
  "rs16.cpp",
  "scantree.cpp",
  "secpassword.cpp",
  "sha1.cpp",
  "sha256.cpp",
  "smallfn.cpp",
  "strfn.cpp",
  "strlist.cpp",
  "system.cpp",
  "threadpool.cpp",
  "timefn.cpp",
  "ui.cpp",
  "unicode.cpp",
  "unpack.cpp",
  "volume.cpp",
];

// Upstream spells these Windows headers with capitals, which breaks the mingw
// cross build on a case-sensitive filesystem. Was a local edit to the vendored
// tree; now it is applied here.
function lowercaseWinIncludes(text: string): string {
  return text.replace(/^#include <(PowrProf|Sddl|Wbemidl)\.h>/gm, (_m, name) => `#include <${name.toLowerCase()}.h>`);
}

// rs.cpp's helper macro would otherwise swallow SecPassword::Clean().
const unrarSeals: Record<string, string[]> = {
  "rs.cpp": ["Clean"],
};

function genUnrar(ctx: Ctx): void {
  const root = ctx.checkoutDir;

  // src/base/Archive.cpp includes this for the RAR API; nothing else in the
  // header set is public.
  ctx.files.set("dll.hpp", readText(join(root, "dll.hpp")));

  // Every .cpp opens with rar.hpp, which pulls in the whole header set, and a
  // few .cpp include others (crypt.cpp -> crypt1..5.cpp, unpack.cpp -> the
  // per-format unpackers). Both are handled by inlining local includes once.
  const rules: IncludeRules = {
    resolve: dirResolver([root]),
    seen: new Set(unrarSources.map((name) => normPath(join(root, name)))),
    dedupOnlyGuarded: true,
  };
  const chunks: string[] = [];
  for (const name of unrarSources) {
    const chunk = prepare(join(root, name), rules, lowercaseWinIncludes);
    chunks.push(unrarSeals[name] ? sealMacros(chunk, unrarSeals[name]) : chunk);
  }
  ctx.files.set("unrar.cpp", joinChunks(chunks));
}

// --- zlib ------------------------------------------------------------------

const zlibSources = [
  "adler32.c",
  "compress.c",
  "crc32.c",
  "deflate.c",
  "inffast.c",
  "inflate.c",
  "inftrees.c",
  "trees.c",
  "zutil.c",
  "gzclose.c",
  "gzlib.c",
  "gzread.c",
  "gzwrite.c",
];

// zlib.h probes HAVE_UNISTD_H/HAVE_STDARG_H with `#if X-0`, which MSVC's /W4
// flags. We always build without either, so test for definedness instead.
function patchZlibHeader(text: string): string {
  return text
    .replace("#if HAVE_UNISTD_H-0\n#  define Z_HAVE_UNISTD_H", "#ifdef HAVE_UNISTD_H\n#  define Z_HAVE_UNISTD_H")
    .replace("#if HAVE_STDARG_H-0\n#  define Z_HAVE_STDARG_H", "#ifdef HAVE_STDARG_H\n#  define Z_HAVE_STDARG_H");
}

function genZlib(ctx: Ctx): void {
  const srcDir = findSrcDir(ctx.checkoutDir, [""], ["zlib.h"]);
  const resolve = byNameResolver(mapByName(listFiles(srcDir)));

  ctx.files.set("zlib.h", patchZlibHeader(prepare(join(srcDir, "zlib.h"), { resolve })));

  const rules: IncludeRules = { resolve, drop: inSet(["zlib.h"]), seen: new Set() };
  const chunks = ['#include "zlib.h"\n'];
  for (const name of zlibSources) {
    chunks.push(prepare(join(srcDir, name), rules));
  }
  ctx.files.set("zlib.c", joinChunks(chunks));
}

// --- brotli ----------------------------------------------------------------

// common, then dec, then enc: the order the non-amalgamated build compiles
// them in, and the order their private headers depend on each other.
const brotliDirs = ["common", "dec", "enc"];

// File-local statics that collide once every .c is one translation unit.
// compress_fragment_two_pass.c is a near-copy of compress_fragment.c, so most
// of the list is its half of that pair.
const brotliRenames: Record<string, string[]> = {
  "compress_fragment_two_pass.c": [
    "BrotliStoreMetaBlockHeader",
    "BuildAndStoreCommandPrefixCode",
    "EmitCopyLen",
    "EmitCopyLenLastDistance",
    "EmitDistance",
    "EmitInsertLen",
    "EmitUncompressedMetaBlock",
    "Hash",
    "HashBytesAtOffset",
    "IsMatch",
    "RewindBitPosition",
    "ShouldCompress",
  ],
  "encoder_dict.c": ["ComputeCutoffTransforms", "Hash"],
  "entropy_encode.c": ["BrotliReverseBits", "SortHuffmanTree"],
  "static_dict.c": ["IsMatch"],
};

// compress_fragment.c and its two-pass near-copy both define MIN_RATIO, with
// different values.
const brotliSeals: Record<string, string[]> = {
  "compress_fragment.c": ["MIN_RATIO"],
  "compress_fragment_two_pass.c": ["MIN_RATIO"],
};

function genBrotli(ctx: Ctx): void {
  const root = join(ctx.checkoutDir, "c");

  // Ship the public headers under brotli/ so `#include <brotli/decode.h>` keeps
  // working with ext/a-brotli on the include path.
  const incDir = join(root, "include", "brotli");
  for (const path of listFiles(incDir, ".h")) {
    ctx.files.set(join("brotli", basename(path)), readText(path));
  }

  // Every quoted include is relative to the including file. The *_inc.h X-macro
  // fragments have no include guard, so dedupOnlyGuarded expands them at each
  // use, which is what they are for.
  const rules: IncludeRules = {
    resolve: dirResolver([]),
    seen: new Set(),
    dedupOnlyGuarded: true,
  };
  const chunks: string[] = [];
  for (const dir of brotliDirs) {
    for (const path of listFiles(join(root, dir), ".c")) {
      const name = basename(path);
      let chunk = prepare(path, rules);
      if (brotliSeals[name]) {
        chunk = sealMacros(chunk, brotliSeals[name]);
      }
      if (brotliRenames[name]) {
        chunk = scopeSymbols(chunk, basename(path, ".c"), brotliRenames[name]);
      }
      chunks.push(chunk);
    }
  }
  ctx.files.set("brotli.c", joinChunks(chunks));
}

// --- bzip2 -----------------------------------------------------------------

const bzip2Sources = ["blocksort.c", "bzlib.c", "compress.c", "crctable.c", "decompress.c", "huffman.c", "randtable.c"];

// bzip2 expects the embedder to supply this; it's called on internal corruption.
const bzip2Additions = `
#include <assert.h>

void bz_internal_error(int errcode) {
  (void)errcode;
  assert(0);
}
`;

function genBzip2(ctx: Ctx): void {
  const srcDir = findSrcDir(ctx.checkoutDir, [""], ["bzlib.h"]);
  // bzip2 has only two local headers and both are concatenated in order, so
  // drop every local include rather than inlining.
  const rules: IncludeRules = { drop: () => true };

  ctx.files.set("bzlib.h", prepare(join(srcDir, "bzlib.h"), rules));

  const preamble = `#ifndef BZ_NO_STDIO
#define BZ_NO_STDIO
#endif
#include "bzlib.h"
`;
  const chunks = [preamble, prepare(join(srcDir, "bzlib_private.h"), rules)];
  for (const name of bzip2Sources) {
    chunks.push(prepare(join(srcDir, name), rules));
  }
  chunks.push(normalizeBlankLines(bzip2Additions));
  ctx.files.set("bzip2.c", joinChunks(chunks));
}

// --- extract ---------------------------------------------------------------

const extractSources = [
  "alloc.c",
  "astring.c",
  "boxer.c",
  "buffer.c",
  "document.c",
  "docx.c",
  "docx_template.c",
  "extract.c",
  "html.c",
  "join.c",
  "json.c",
  "mem.c",
  "memento.c",
  "odt_template.c",
  "odt.c",
  "outf.c",
  "rect.c",
  "sys.c",
  "text.c",
  "xml.c",
  "zip.c",
];

function genExtract(ctx: Ctx): void {
  const root = ctx.checkoutDir;
  for (const name of ["alloc.h", "buffer.h", "extract.h"]) {
    ctx.files.set(join("extract", name), prepare(join(root, "include", "extract", name)));
  }
  ctx.files.set("memento.h", prepare(join(root, "src", "memento.h")));

  const rules: IncludeRules = {
    resolve: dirResolver([join(root, "include"), join(root, "src")]),
    // these stay as includes: they're the public headers we ship
    keep: (inc) => inc.startsWith("extract/") || inc === "memento.h",
    seen: new Set(),
  };
  const chunks = ['#include "extract/extract.h"\n#include "extract/buffer.h"\n#include "memento.h"\n'];
  for (const name of extractSources) {
    let chunk = prepare(join(root, "src", name), rules);
    // mupdf already ships memento.c; when extract is compiled into the mupdf
    // static lib we define EXTRACT_NO_OWN_MEMENTO to avoid LNK4006 duplicates.
    if (name === "memento.c") {
      chunk = "#ifndef EXTRACT_NO_OWN_MEMENTO\n" + chunk + "\n#endif /* EXTRACT_NO_OWN_MEMENTO */\n";
    }
    chunks.push(chunk);
  }
  ctx.files.set("extract.c", joinChunks(chunks));
}

// --- freetype --------------------------------------------------------------

// The module set mupdf needs: no autofit, bdf, cache, pcf, pfr, sdf, svg,
// type42 or winfonts. Adding a module means adding its aggregate .c here.
const freetypeBaseSources = [
  "ftbase.c",
  "ftbbox.c",
  "ftbitmap.c",
  "ftdebug.c",
  "ftfstype.c",
  "ftgasp.c",
  "ftglyph.c",
  "ftinit.c",
  "ftotval.c",
  "ftstroke.c",
  "ftsynth.c",
  "ftsystem.c",
  "fttype1.c",
];

const freetypeModuleSources = [
  join("gzip", "ftgzip.c"),
  join("cff", "cff.c"),
  join("cid", "type1cid.c"),
  join("psaux", "psaux.c"),
  join("pshinter", "pshinter.c"),
  join("psnames", "psnames.c"),
  join("raster", "raster.c"),
  join("sfnt", "sfnt.c"),
  join("smooth", "smooth.c"),
  join("truetype", "truetype.c"),
  join("type1", "type1.c"),
];

function genFreetype(ctx: Ctx): void {
  const root = ctx.checkoutDir;

  // Ship the header tree verbatim. mupdf and harfbuzz include <freetype/...>
  // directly and the amalgamated sources keep those includes, so this stays the
  // library's include dir; comments carry the per-file FTL notice.
  const incDir = join(root, "include");
  for (const path of listFilesRec(incDir)) {
    ctx.files.set(join("include", normPath(relative(incDir, path))), readText(path));
  }

  // Every quoted include in these modules names a file in the includer's own
  // directory: either a module-private header or, for the per-module aggregate
  // .c files, another .c. Inlining exactly those collapses the 24 translation
  // units into one. <freetype/...> and <brotli/...> includes are left alone.
  // No dedup: sfnt.c pulls pngshim.c first, and its ttload.h include sits in a
  // disabled #ifdef, so deduping would drop ttload.h from every later use. The
  // headers' own guards make the repeats no-ops for the compiler.
  const rules: IncludeRules = { resolve: dirResolver([]), guarded: true };
  const chunks: string[] = [];
  for (const name of freetypeBaseSources) {
    chunks.push(prepare(join(root, "src", "base", name), rules));
  }
  for (const name of freetypeModuleSources) {
    chunks.push(prepare(join(root, "src", name), rules));
  }
  ctx.files.set("freetype.c", joinChunks(chunks));
}

// --- gumbo -----------------------------------------------------------------

const gumboHeaderAdditions = `
void gumbo_destroy_node_iter(GumboOptions* options, GumboNode* node);
void gumbo_destroy_output_iter(const GumboOptions* options, GumboOutput* output);
`;

// Upstream frees the parse tree recursively, which blows the stack on deeply
// nested HTML. These are iterative replacements.
const gumboSourceAdditions = `
void gumbo_destroy_node_iter(GumboOptions* options, GumboNode* node) {
  GumboParser parser;
  parser._options = options;

  GumboVector stack;
  gumbo_vector_init(&parser, 10, &stack);
  gumbo_vector_add(&parser, node, &stack);
  while (stack.length > 0) {
    GumboNode* n = (GumboNode*) gumbo_vector_pop(&parser, &stack);
    switch (n->type) {
      case GUMBO_NODE_DOCUMENT: {
        GumboDocument* doc = &n->v.document;
        for (unsigned int i = 0; i < doc->children.length; ++i) {
          gumbo_vector_add(&parser, doc->children.data[i], &stack);
        }
        gumbo_parser_deallocate(&parser, (void*) doc->children.data);
        gumbo_parser_deallocate(&parser, (void*) doc->name);
        gumbo_parser_deallocate(&parser, (void*) doc->public_identifier);
        gumbo_parser_deallocate(&parser, (void*) doc->system_identifier);
      } break;
      case GUMBO_NODE_TEMPLATE:
      case GUMBO_NODE_ELEMENT:
        for (unsigned int i = 0; i < n->v.element.attributes.length; ++i) {
          gumbo_destroy_attribute(&parser, n->v.element.attributes.data[i]);
        }
        gumbo_parser_deallocate(&parser, n->v.element.attributes.data);
        for (unsigned int i = 0; i < n->v.element.children.length; ++i) {
          gumbo_vector_add(&parser, n->v.element.children.data[i], &stack);
        }
        gumbo_parser_deallocate(&parser, n->v.element.children.data);
        break;
      case GUMBO_NODE_TEXT:
      case GUMBO_NODE_CDATA:
      case GUMBO_NODE_COMMENT:
      case GUMBO_NODE_WHITESPACE:
        gumbo_parser_deallocate(&parser, (void*) n->v.text.text);
        break;
    }
    gumbo_parser_deallocate(&parser, n);
  }
  gumbo_vector_destroy(&parser, &stack);
}

void gumbo_destroy_output_iter(const GumboOptions* options, GumboOutput* output) {
  GumboParser parser;
  parser._options = options;
  gumbo_destroy_node_iter((GumboOptions*) options, output->document);
  for (unsigned int i = 0; i < output->errors.length; ++i) {
    gumbo_error_destroy(&parser, output->errors.data[i]);
  }
  gumbo_vector_destroy(&parser, &output->errors);
  gumbo_parser_deallocate(&parser, output);
}
`;

// <strings.h> doesn't exist on Windows and the preamble already defines
// _CRT_SECURE_NO_WARNINGS.
function dropGumboPosixLines(text: string): string {
  return text
    .split("\n")
    .filter((line) => !/^\s*#\s*include\s+<strings\.h>/.test(line))
    .filter((line) => !/^\s*#\s*define\s+_CRT_SECURE_NO_WARNINGS/.test(line))
    .join("\n");
}

function genGumbo(ctx: Ctx): void {
  const srcDir = findSrcDir(ctx.checkoutDir, ["src", ""], ["gumbo.h"]);
  const resolve = byNameResolver(mapByName(listFiles(srcDir, ".h")));

  // No dedup: gumbo has include fragments (tag_enum.h, tag_gperf.h) that are
  // only valid where they appear and are pulled in more than once.
  let header = prepare(join(srcDir, "gumbo.h"), { resolve, markers: true }, dropGumboPosixLines);
  const lastEndif = header.lastIndexOf("#endif");
  if (lastEndif < 0) {
    throw new Error("could not find final #endif in gumbo.h");
  }
  header = header.slice(0, lastEndif) + normalizeBlankLines(gumboHeaderAdditions) + header.slice(lastEndif);
  ctx.files.set("gumbo.h", header);

  const preamble = `#ifdef _MSC_VER
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "gumbo.h"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif
`;
  const rules: IncludeRules = { resolve, drop: inSet(["gumbo.h"]), markers: true };
  const chunks = [preamble];
  for (const path of listFiles(srcDir, ".c")) {
    chunks.push(prepare(path, rules, dropGumboPosixLines));
  }
  chunks.push(normalizeBlankLines(gumboSourceAdditions));
  ctx.files.set("gumbo.c", joinChunks(chunks));
}

// --- harfbuzz --------------------------------------------------------------

// hb_base_sources + hb_subset_sources + hb-ft.cc from harfbuzz src/meson.build.
// The backends we don't build (cairo, coretext, directwrite, graphite2, icu,
// raster, uniscribe, wasm) are left out, same as the non-amalgamated build.
const harfbuzzSources = [
  join("OT", "Var", "VARC", "VARC.cc"),
  "hb-aat-layout.cc",
  "hb-aat-map.cc",
  "hb-blob.cc",
  "hb-buffer-serialize.cc",
  "hb-buffer-verify.cc",
  "hb-buffer.cc",
  "hb-common.cc",
  "hb-draw.cc",
  "hb-face-builder.cc",
  "hb-face.cc",
  "hb-fallback-shape.cc",
  "hb-font.cc",
  "hb-map.cc",
  "hb-number.cc",
  "hb-ot-cff1-table.cc",
  "hb-ot-cff2-table.cc",
  "hb-ot-color.cc",
  "hb-ot-face.cc",
  "hb-ot-font.cc",
  "hb-ot-layout.cc",
  "hb-ot-map.cc",
  "hb-ot-math.cc",
  "hb-ot-meta.cc",
  "hb-ot-metrics.cc",
  "hb-ot-name.cc",
  "hb-ot-shape-fallback.cc",
  "hb-ot-shape-normalize.cc",
  "hb-ot-shape.cc",
  "hb-ot-shaper-arabic.cc",
  "hb-ot-shaper-default.cc",
  "hb-ot-shaper-hangul.cc",
  "hb-ot-shaper-hebrew.cc",
  "hb-ot-shaper-indic-table.cc",
  "hb-ot-shaper-indic.cc",
  "hb-ot-shaper-khmer.cc",
  "hb-ot-shaper-myanmar.cc",
  "hb-ot-shaper-syllabic.cc",
  "hb-ot-shaper-thai.cc",
  "hb-ot-shaper-use.cc",
  "hb-ot-shaper-vowel-constraints.cc",
  "hb-ot-tag.cc",
  "hb-ot-var.cc",
  "hb-outline.cc",
  "hb-paint-bounded.cc",
  "hb-paint-extents.cc",
  "hb-paint.cc",
  "hb-set.cc",
  "hb-shape-plan.cc",
  "hb-shape.cc",
  "hb-shaper.cc",
  "hb-static.cc",
  "hb-style.cc",
  "hb-ucd.cc",
  "hb-unicode.cc",
  join("graph", "gsubgpos-context.cc"),
  "hb-subset-cff-common.cc",
  "hb-subset-cff1.cc",
  "hb-subset-cff2-to-cff1.cc",
  "hb-subset-cff2.cc",
  "hb-subset-input.cc",
  "hb-subset-instancer-iup.cc",
  "hb-subset-instancer-solver.cc",
  "hb-subset-plan-layout.cc",
  "hb-subset-plan-var.cc",
  "hb-subset-plan.cc",
  "hb-subset-serialize.cc",
  "hb-subset-table-cff.cc",
  "hb-subset-table-color.cc",
  "hb-subset-table-layout.cc",
  "hb-subset-table-other.cc",
  "hb-subset-table-var.cc",
  "hb-subset.cc",
  "hb-ft.cc",
];

// The public C API the rest of the tree includes; everything reachable from
// these by a quoted include ships next to the amalgamated .cc.
const harfbuzzApiRoots = ["hb.h", "hb-ot.h", "hb-aat.h", "hb-ft.h", "hb-subset.h", "hb-subset-serialize.h"];

// X-macro fragments the inliner can't expand: the first two are reached through
// `#include HB_STRING_ARRAY_LIST` (a macro), and win1256 includes itself a
// second time to emit the table it just measured. They ship as files next to
// harfbuzz.cc and stay as includes.
const harfbuzzFragments = ["hb-ot-cff1-std-str.hh", "hb-ot-post-macroman.hh", "hb-ot-shaper-arabic-win1256.hh"];

function harfbuzzApiHeaders(srcDir: string): Set<string> {
  const found = new Set<string>();
  const todo = [...harfbuzzApiRoots];
  while (todo.length > 0) {
    const name = todo.pop() as string;
    if (found.has(name)) {
      continue;
    }
    found.add(name);
    for (const line of readText(join(srcDir, name)).split("\n")) {
      const m = includeRe.exec(line);
      if (m && m[1].endsWith(".h")) {
        todo.push(m[1]);
      }
    }
  }
  return found;
}

function genHarfbuzz(ctx: Ctx): void {
  const srcDir = findSrcDir(ctx.checkoutDir, ["src", ""], ["hb.h", "hb.hh"]);

  const api = harfbuzzApiHeaders(srcDir);
  for (const name of [...api, ...harfbuzzFragments]) {
    ctx.files.set(name, readText(join(srcDir, name)));
  }

  // Only the private .hh/.cc are inlined; the public headers above stay as
  // includes. Dedup is required here, not just an optimization: the OT/ headers
  // include each other densely enough that inlining every occurrence blows up
  // to gigabytes.
  const rules: IncludeRules = {
    resolve: dirResolver([srcDir]),
    keep: (inc) => api.has(inc) || harfbuzzFragments.includes(inc),
    seen: new Set(),
    dedupOnlyGuarded: true,
  };
  const chunks: string[] = [];
  for (const name of harfbuzzSources) {
    chunks.push(prepare(join(srcDir, name), rules));
  }
  ctx.files.set("harfbuzz.cc", joinChunks(chunks));
}

// --- jbig2dec --------------------------------------------------------------

const jbig2decSources = [
  "jbig2.c",
  "jbig2_arith.c",
  "jbig2_arith_iaid.c",
  "jbig2_arith_int.c",
  "jbig2_generic.c",
  "jbig2_huffman.c",
  "jbig2_hufftab.c",
  "jbig2_halftone.c",
  "jbig2_image.c",
  "jbig2_mmr.c",
  "jbig2_page.c",
  "jbig2_refinement.c",
  "jbig2_segment.c",
  "jbig2_symbol_dict.c",
  "jbig2_text.c",
];

function genJbig2dec(ctx: Ctx): void {
  const root = ctx.checkoutDir;
  const resolve = dirResolver([root]);

  // jbig2.h uses size_t/uint32_t without including their headers.
  ctx.files.set(
    "jbig2.h",
    normalizeBlankLines(`#include <stddef.h>
#include <stdint.h>

${prepare(join(root, "jbig2.h"), { resolve })}`),
  );

  // config.h is generated by autotools and we supply its defines via cl flags.
  const rules: IncludeRules = { resolve, drop: inSet(["config.h", "jbig2.h"], true), seen: new Set() };
  const chunks = ['#include "jbig2.h"\n'];
  for (const name of jbig2decSources) {
    chunks.push(prepare(join(root, name), rules));
  }
  ctx.files.set("jbig2dec.c", joinChunks(chunks));
}

// --- lcms2 -----------------------------------------------------------------

// A chameleonic header: cmsxform.c includes it ~66 times, redefining
// FUNCTION_NAME and friends each time, so it ships as a file and stays an
// include.
const lcms2XformHeader = "extra_xform.h";

// File-local statics that collide with a same-named static elsewhere once every
// .c lands in one translation unit. Only one side of each pair needs renaming;
// cmsio1.c's PickLstarMatrix collides with a local variable in cmsvirt.c.
const lcms2Renames: Record<string, string[]> = {
  "cmshalf.c": ["Base", "Offset"],
  "cmsio1.c": ["PickLstarMatrix"],
  "cmsmtrx.c": ["CloseEnough"],
  "cmsnamed.c": ["mywcslen"],
  "cmspack.c": ["PixelSize"],
  "cmsps2.c": ["WriteCLUT"],
};

function genLcms2(ctx: Ctx): void {
  const root = ctx.checkoutDir;
  const incDir = join(root, "include");
  const srcDir = join(root, "src");

  const api = ["lcms2mt.h", "lcms2mt_plugin.h"];
  for (const name of api) {
    ctx.files.set(name, prepare(join(incDir, name)));
  }
  ctx.files.set(lcms2XformHeader, readText(join(srcDir, lcms2XformHeader)));

  // Only lcms2_internal.h is inlined, once; every .c starts with it.
  const rules: IncludeRules = {
    resolve: dirResolver([incDir, srcDir]),
    keep: (inc) => api.includes(inc) || inc === lcms2XformHeader,
    seen: new Set(),
  };
  const chunks = ['#include "lcms2mt.h"\n#include "lcms2mt_plugin.h"\n'];
  for (const path of listFiles(srcDir, ".c")) {
    const renames = lcms2Renames[basename(path)];
    const chunk = prepare(path, rules);
    chunks.push(renames ? scopeSymbols(chunk, basename(path, ".c"), renames) : chunk);
  }
  ctx.files.set("lcms2.c", joinChunks(chunks));
}

// --- libarchive ------------------------------------------------------------

// The readers and filters we build: no write side, no disk write, no POSIX
// backends. Same list the non-amalgamated build compiled, minus
// archive_read_extract*.c: nothing calls them and in one translation unit their
// references to the write-disk API can no longer be dropped by the linker.
const libarchiveSources = [
  "archive_acl.c",
  "archive_check_magic.c",
  "archive_cmdline.c",
  "archive_cryptor.c",
  "archive_digest.c",
  "archive_entry.c",
  "archive_entry_copy_bhfi.c",
  "archive_entry_copy_stat.c",
  "archive_entry_link_resolver.c",
  "archive_entry_sparse.c",
  "archive_entry_stat.c",
  "archive_entry_strmode.c",
  "archive_entry_xattr.c",
  "archive_hmac.c",
  "archive_match.c",
  "archive_options.c",
  "archive_pack_dev.c",
  "archive_pathmatch.c",
  "archive_ppmd7.c",
  "archive_ppmd8.c",
  "archive_random.c",
  "archive_rb.c",
  "archive_string.c",
  "archive_string_sprintf.c",
  "archive_time.c",
  "archive_util.c",
  "archive_version_details.c",
  "archive_virtual.c",
  "archive_windows.c",
  "archive_blake2s_ref.c",
  "archive_blake2sp_ref.c",
  "archive_read.c",
  "archive_read_add_passphrase.c",
  "archive_read_append_filter.c",
  "archive_read_data_into_fd.c",
  "archive_read_open_fd.c",
  "archive_read_open_file.c",
  "archive_read_open_filename.c",
  "archive_read_open_memory.c",
  "archive_read_set_format.c",
  "archive_read_set_options.c",
  "archive_read_support_filter_all.c",
  "archive_read_support_filter_by_code.c",
  "archive_read_support_filter_bzip2.c",
  "archive_read_support_filter_compress.c",
  "archive_read_support_filter_grzip.c",
  "archive_read_support_filter_gzip.c",
  "archive_read_support_filter_lrzip.c",
  "archive_read_support_filter_lz4.c",
  "archive_read_support_filter_lzop.c",
  "archive_read_support_filter_none.c",
  "archive_read_support_filter_program.c",
  "archive_read_support_filter_rpm.c",
  "archive_read_support_filter_uu.c",
  "archive_read_support_filter_xz.c",
  "archive_read_support_filter_zstd.c",
  "archive_read_support_format_7zip.c",
  "archive_read_support_format_all.c",
  "archive_read_support_format_ar.c",
  "archive_read_support_format_by_code.c",
  "archive_read_support_format_cab.c",
  "archive_read_support_format_cpio.c",
  "archive_read_support_format_empty.c",
  "archive_read_support_format_iso9660.c",
  "archive_read_support_format_lha.c",
  "archive_read_support_format_mtree.c",
  "archive_read_support_format_rar.c",
  "archive_read_support_format_rar5.c",
  "archive_read_support_format_raw.c",
  "archive_read_support_format_tar.c",
  "archive_read_support_format_warc.c",
  "archive_read_support_format_xar.c",
  "archive_read_support_format_zip.c",
  "xxhash.c",
  "archive_read_disk_set_standard_lookup.c",
  "archive_read_disk_windows.c",
  "archive_parse_date.c",
  "filter_fork_windows.c",
];

// File-local statics that collide once every .c is one translation unit. Only
// one side of each pair is renamed; archive_ppmd8.c is a near-copy of
// archive_ppmd7.c, hence the size of its entry.
const libarchiveRenames: Record<string, string[]> = {
  "archive_ppmd8.c": [
    "AllocUnits",
    "AllocUnitsRare",
    "CreateSuccessors",
    "GlueFreeBlocks",
    "InsertNode",
    "NextContext",
    "RemoveNode",
    "Rescale",
    "RestartModel",
    "SetSuccessor",
    "ShrinkUnits",
    "SplitBlock",
    "SwapStates",
    "UpdateModel",
    "kInitBinEsc",
    // a file-local typedef, same name and different type in both
    "CTX_PTR",
  ],
  "archive_read_disk_windows.c": [
    "_archive_read_close",
    "_archive_read_data_block",
    "_archive_read_free",
    "_archive_read_next_header",
    "_archive_read_next_header2",
    "next_entry",
  ],
  "archive_read_open_filename.c": ["file_close", "file_read", "file_seek", "file_skip"],
  // each read filter has its own `struct private_data`
  "archive_read_support_filter_compress.c": ["private_data"],
  "archive_read_support_filter_gzip.c": ["private_data"],
  "archive_read_support_filter_lz4.c": ["private_data"],
  "archive_read_support_filter_lzop.c": ["consume_header"],
  "archive_read_support_filter_xz.c": ["private_data"],
  "archive_read_support_filter_zstd.c": ["private_data"],
  "archive_read_support_format_7zip.c": ["set_error"],
  "archive_read_support_format_cpio.c": ["links_entry"],
  "archive_read_support_format_lha.c": ["cache_masks", "huffman", "truncated_error"],
  "archive_read_support_format_mtree.c": ["cleanup", "rb_ops"],
  "archive_read_support_format_rar.c": ["cache_masks", "ppmd_read", "read_header"],
  "archive_read_support_format_rar5.c": ["parse_filter"],
  "archive_read_support_format_tar.c": ["get_time_t_max", "readline"],
  "archive_read_support_format_warc.c": ["time_from_tm"],
  "archive_read_support_format_xar.c": [
    "base64",
    "decompress",
    "heap_add_entry",
    "heap_get_entry",
    "heap_queue",
    "time_from_tm",
  ],
  "archive_read_support_format_zip.c": ["compression_name", "ppmd_read", "rb_ops", "slurp_central_directory"],
};

// The PPMd model macros, defined by both variants with different values and
// (kTopValue) redefined again by the 7zip reader.
const ppmdMacros = [
  "CTX",
  "I2U",
  "MASK",
  "MAX_FREQ",
  "MyMem12Cpy",
  "NODE",
  "ONE_STATE",
  "STATS",
  "STATS_REF",
  "SUCCESSOR",
  "SUFFIX",
  "U2B",
  "U2I",
  "UNIT_SIZE",
  "kTopValue",
];

// Macros a file leaves defined that a later one redefines differently.
const libarchiveSeals: Record<string, string[]> = {
  "archive_ppmd7.c": ppmdMacros,
  "archive_ppmd8.c": ppmdMacros,
  "archive_rb.c": ["T"],
  "archive_read_support_format_7zip.c": ["ATIME_IS_SET", "CTIME_IS_SET", "MTIME_IS_SET"],
  "archive_read_support_format_iso9660.c": [
    "ATIME_IS_SET",
    "BIRTHTIME_IS_SET",
    "CTIME_IS_SET",
    "MTIME_IS_SET",
    "next_entry",
  ],
};

function genLibarchive(ctx: Ctx): void {
  const srcDir = findSrcDir(ctx.checkoutDir, ["libarchive", ""], ["archive.h", "archive_entry.h"]);

  // config_windows.h is hand-written (not upstream) and lives in the output dir
  // already; archive_platform.h reaches it through `#include PLATFORM_CONFIG_H`,
  // a macro the inliner leaves alone.
  // Under libarchive/ because mupdf and src/base/Archive.cpp include
  // "libarchive/archive.h"; both ext/a-libarchive and ext/a-libarchive/libarchive
  // are on the include path, as the vendored tree's two dirs were.
  const shipped = ["archive.h", "archive_entry.h"];
  for (const name of shipped) {
    ctx.files.set(join("libarchive", name), readText(join(srcDir, name)));
  }

  const rules: IncludeRules = {
    resolve: dirResolver([srcDir]),
    keep: (inc) => shipped.includes(inc),
    seen: new Set(),
    dedupOnlyGuarded: true,
  };
  const chunks: string[] = [];
  for (const name of libarchiveSources) {
    let chunk = prepare(join(srcDir, name), rules);
    if (libarchiveSeals[name]) {
      chunk = sealMacros(chunk, libarchiveSeals[name]);
    }
    if (libarchiveRenames[name]) {
      chunk = scopeSymbols(chunk, basename(name, ".c"), libarchiveRenames[name]);
    }
    chunks.push(chunk);
  }
  ctx.files.set("libarchive.c", joinChunks(chunks));
}

// --- libwebp ---------------------------------------------------------------

// Decoder only: src/dec, src/utils and the dsp files those need. No encoder,
// no mux/demux, no sharpyuv, same as the non-amalgamated build, minus the
// encoder-side cost.c: nothing calls it and in one translation unit its
// reference to the SSE2 encoder can no longer be dropped by the linker.
const libwebpDspSources = [
  "alpha_processing.c",
  "alpha_processing_neon.c",
  "alpha_processing_sse2.c",
  "alpha_processing_sse41.c",
  "cpu.c",
  "dec.c",
  "dec_clip_tables.c",
  "dec_neon.c",
  "dec_sse2.c",
  "dec_sse41.c",
  "filters.c",
  "filters_neon.c",
  "filters_sse2.c",
  "lossless.c",
  "lossless_avx2.c",
  "lossless_neon.c",
  "lossless_sse2.c",
  "lossless_sse41.c",
  "rescaler.c",
  "rescaler_neon.c",
  "rescaler_sse2.c",
  "ssim.c",
  "ssim_sse2.c",
  "upsampling.c",
  "upsampling_neon.c",
  "upsampling_sse2.c",
  "upsampling_sse41.c",
  "yuv.c",
  "yuv_neon.c",
  "yuv_sse2.c",
  "yuv_sse41.c",
];

// File-local statics that collide once every .c is one translation unit.
// filters_neon.c repeats filters.c's GradientPredictor_C; only ARM builds see
// both, so the x64 validation compile below can't catch it.
const libwebpRenames: Record<string, string[]> = {
  "filters_neon.c": ["GradientPredictor_C"],
  "quant_levels_dec_utils.c": ["clip_8b"],
  "ssim_sse2.c": ["kWeight"],
};

function genLibwebp(ctx: Ctx): void {
  const root = ctx.checkoutDir;

  // The public API, shipped for src/WebpReader.cpp and mupdf, which include
  // <webp/decode.h>. The amalgamated .c inlines its own copy.
  const webpDir = join(root, "src", "webp");
  for (const path of listFiles(webpDir, ".h")) {
    ctx.files.set(join("webp", basename(path)), readText(path));
  }

  const sources = [
    ...listFiles(join(root, "src", "dec"), ".c"),
    ...libwebpDspSources.map((name) => join(root, "src", "dsp", name)),
    ...listFiles(join(root, "src", "utils"), ".c"),
  ];

  // Quoted includes are rooted at the checkout ("src/dec/vp8i_dec.h"). Marking
  // the sources as seen keeps a .c that another .c includes from being emitted
  // twice.
  const rules: IncludeRules = {
    resolve: dirResolver([root]),
    seen: new Set(sources.map(normPath)),
    dedupOnlyGuarded: true,
  };
  const chunks: string[] = [];
  for (const path of sources) {
    const name = basename(path);
    const chunk = prepare(path, rules);
    chunks.push(libwebpRenames[name] ? scopeSymbols(chunk, basename(name, ".c"), libwebpRenames[name]) : chunk);
  }
  ctx.files.set("libwebp.c", joinChunks(chunks));
}

// --- mujs ------------------------------------------------------------------

function genMujs(ctx: Ctx): void {
  const root = ctx.checkoutDir;
  const resolve = byNameResolver(mapByName(listFiles(root)));

  ctx.files.set("mujs.h", prepare(join(root, "mujs.h"), { resolve, seen: new Set() }));

  // one.c is upstream's own single-TU build; inlining its includes is enough.
  const rules: IncludeRules = { resolve, drop: inSet(["mujs.h"]), seen: new Set() };
  ctx.files.set("mujs.c", joinChunks(['#include "mujs.h"\n', prepare(join(root, "one.c"), rules)]));
}

// --- openjpeg --------------------------------------------------------------

const openjpegSources = [
  "bio.c",
  "cidx_manager.c",
  "cio.c",
  "dwt.c",
  "event.c",
  "function_list.c",
  "ht_dec.c",
  "image.c",
  "invert.c",
  "j2k.c",
  "jp2.c",
  "mct.c",
  "mqc.c",
  "openjpeg.c",
  "opj_clock.c",
  "phix_manager.c",
  "pi.c",
  "ppix_manager.c",
  "sparse_array.c",
  "t1.c",
  "t2.c",
  "tcd.c",
  "tgt.c",
  "thix_manager.c",
  "thread.c",
  "tpix_manager.c",
];

// ht_dec.c has its own opj_t1_allocate_buffers() that collides with t1.c's once
// both land in one translation unit.
function renameHtDecSymbol(text: string): string {
  return `#define opj_t1_allocate_buffers opj_ht_dec_t1_allocate_buffers\n${text}\n#undef opj_t1_allocate_buffers\n`;
}

function genOpenjpeg(ctx: Ctx): void {
  const srcDir = findSrcDir(ctx.checkoutDir, [join("src", "lib", "openjp2"), ""], ["openjpeg.h", "j2k.c"]);

  // All private headers ship alongside the .c, so local includes stay as-is.
  for (const path of listFiles(srcDir, ".h")) {
    ctx.files.set(basename(path), prepare(path));
  }

  // The amalgamation is a single TU mixing non-SIMD code with SIMD sections that
  // include <immintrin.h>/<mm_malloc.h>. opj_malloc.h poisons malloc/free, and
  // #pragma GCC poison can't be undone, so the first include poisons them and the
  // later SIMD headers fail to compile (clang-18's mm_malloc.h uses malloc/free).
  // Disable the poison for the whole amalgamated TU.
  const chunks = ["#define OPJ_SKIP_POISON\n", '#include "openjpeg.h"\n'];
  for (const name of openjpegSources) {
    chunks.push(prepare(join(srcDir, name), undefined, name === "ht_dec.c" ? renameHtDecSymbol : undefined));
  }
  ctx.files.set("openjpeg.c", joinChunks(chunks));
}

// --- zopfli ----------------------------------------------------------------

const zopfliSources = [
  "blocksplitter.c",
  "cache.c",
  "deflate.c",
  "gzip_container.c",
  "hash.c",
  "katajainen.c",
  "lz77.c",
  "squeeze.c",
  "tree.c",
  "util.c",
  "zlib_container.c",
  "zopfli_lib.c",
];

const zopfliCppSources = [
  join("zopflipng", "lodepng", "lodepng.cpp"),
  join("zopflipng", "lodepng", "lodepng_util.cpp"),
  join("zopflipng", "zopflipng_lib.cc"),
];

// Hoisted from lodepng.cpp. In the non-amalgamated build, zlib_container.c and
// lodepng.cpp each had their own file-local static adler32(). Concatenating both
// into one translation unit collides on Win32 where size_t is unsigned.
const zopfliAdler32Helpers = `
static unsigned update_adler32(unsigned adler, const unsigned char* data, unsigned len) {
  unsigned s1 = adler & 0xffffu;
  unsigned s2 = (adler >> 16u) & 0xffffu;

  while(len != 0u) {
    unsigned i;
    unsigned amount = len > 5552u ? 5552u : len;
    len -= amount;
    for(i = 0; i != amount; ++i) {
      s1 += (*data++);
      s2 += s1;
    }
    s1 %= 65521u;
    s2 %= 65521u;
  }

  return (s2 << 16u) | s1;
}

static unsigned adler32(const unsigned char* data, unsigned len) {
  return update_adler32(1u, data, len);
}
`;

// Drops the two originals now that zopfliAdler32Helpers provides them.
function dropZopfliAdler32(name: string, text: string): string {
  if (basename(name) === "zlib_container.c") {
    return text.replace(/static unsigned adler32\(const unsigned char\* data, size_t size\)\s*\{[\s\S]*?\n\}\n\n/, "");
  }
  if (basename(name) === "lodepng.cpp") {
    return text.replace(
      /static unsigned update_adler32\(unsigned adler, const unsigned char\* data, unsigned len\) \{[\s\S]*?\n\}\n\nstatic unsigned adler32\(const unsigned char\* data, unsigned len\) \{\s*return update_adler32\(1u, data, len\);\s*\}\n\n/,
      "",
    );
  }
  return text;
}

// zopfli's includes are written relative to src/, so map by both.
function zopfliIncludeMap(checkoutDir: string): Map<string, string> {
  const srcDir = join(checkoutDir, "src");
  const listed = gitOutput(["-C", checkoutDir, "ls-files", "src"], ".")
    .split(/\r?\n/)
    .filter(Boolean)
    .map((path) => join(checkoutDir, path))
    .filter((path) => /\.(h|c|cc|cpp)$/.test(path));

  const map = new Map<string, string>();
  for (const path of listed) {
    map.set(normPath(relative(srcDir, path)), path);
    map.set(basename(path), path);
  }
  return map;
}

function genZopfli(ctx: Ctx): void {
  const srcDir = join(ctx.checkoutDir, "src");
  const byName = zopfliIncludeMap(ctx.checkoutDir);
  const resolve: ResolveInclude = (fromPath, inc) => {
    const relPath = normPath(join(dirname(fromPath), inc));
    if (existsSync(relPath)) {
      return relPath;
    }
    const srcRel = normPath(relative(srcDir, relPath));
    return byName.get(srcRel) ?? byName.get(inc) ?? byName.get(basename(inc));
  };

  const publicHeaders: [string, string][] = [
    [join("zopflipng", "lodepng", "lodepng.h"), join(srcDir, "zopflipng", "lodepng", "lodepng.h")],
    [join("zopflipng", "zopflipng_lib.h"), join(srcDir, "zopflipng", "zopflipng_lib.h")],
  ];
  for (const [outName, path] of publicHeaders) {
    ctx.files.set(outName, prepare(path, { resolve, drop: inSet([basename(path)]) }));
  }

  const rules: IncludeRules = {
    resolve,
    // the two public headers above are included by the preamble instead
    drop: inSet(["zopflipng_lib.h", "lodepng.h", "zopflipng/lodepng/lodepng.h", "lodepng/lodepng.h"], true),
    seen: new Set(),
  };
  const preamble = `#include "zopflipng/zopflipng_lib.h"
#include "zopflipng/lodepng/lodepng.h"
`;
  const chunks = [preamble, zopfliAdler32Helpers];
  for (const name of zopfliSources) {
    chunks.push(dropZopfliAdler32(name, prepare(join(srcDir, "zopfli", name), rules)));
  }
  for (const name of zopfliCppSources) {
    chunks.push(dropZopfliAdler32(name, prepare(join(srcDir, name), rules)));
  }
  ctx.files.set("zopfli.cpp", joinChunks(chunks));
}

// --- the table -------------------------------------------------------------

const libs: Lib[] = [
  {
    name: "brotli",
    homepage: "https://github.com/google/brotli",
    repo: "https://github.com/ArtifexSoftware/thirdparty-brotli",
    rev: "2523f3314501aa5c90e561922b2e668b3ccee26e",
    writes: "brotli/*.h, brotli.c, LICENSE",
    generate: genBrotli,
    copies: ["LICENSE"],
    compile: {
      file: "brotli.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_CRT_SECURE_NO_WARNINGS", "_HAS_ITERATOR_DEBUGGING=0"),
        "/I",
        ".",
        "/wd4100",
        "/wd4127",
        "/wd4189",
        "/wd4201",
      ],
    },
  },
  {
    name: "bzip2",
    homepage: "https://www.sourceware.org/bzip2/",
    repo: "git://sourceware.org/git/bzip2.git",
    rev: "bzip2-1.0.8",
    writes: "bzlib.h, bzip2.c, LICENSE",
    generate: genBzip2,
    copies: ["LICENSE"],
    compile: {
      file: "bzip2.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "BZ_NO_STDIO", "_CRT_SECURE_NO_WARNINGS", "_HAS_ITERATOR_DEBUGGING=0"),
        "/wd4018",
        "/wd4100",
        "/wd4127",
        "/wd4244",
        "/wd4267",
        "/wd4701",
        "/wd4706",
      ],
    },
  },
  {
    name: "extract",
    homepage: "https://github.com/ArtifexSoftware/extract",
    repo: "https://github.com/ArtifexSoftware/extract",
    rev: "8750ac39c30a0d65119b426b5a491c5b8e8bf674",
    writes: "extract/*.h, memento.h, extract.c",
    generate: genExtract,
    compile: {
      file: "extract.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_CRT_SECURE_NO_WARNINGS", "_HAS_ITERATOR_DEBUGGING=0"),
        "/I",
        ".",
        "/I",
        "..\\..\\..\\ext\\a-zlib",
        "/wd4005",
        "/wd4100",
        "/wd4127",
        "/wd4130",
        "/wd4201",
        "/wd4245",
        "/wd4310",
        "/wd4389",
        "/wd4456",
        "/wd4457",
        "/wd4701",
        "/wd4996",
      ],
    },
  },
  {
    name: "freetype",
    homepage: "https://www.freetype.org/",
    repo: "https://github.com/ArtifexSoftware/thirdparty-freetype2",
    rev: "0a0221a1347e2f1e07c395263540026e9a0aa7c7",
    writes: "include/**, freetype.c, LICENSE.TXT, FTL.TXT, GPLv2.TXT",
    generate: genFreetype,
    // FreeType is dual-licensed (FTL / GPLv2) and both notices must ship.
    copies: ["LICENSE.TXT", join("docs", "FTL.TXT"), join("docs", "GPLv2.TXT")],
    // the shipped header tree follows upstream's, so stale headers must not linger
    wipeOutDir: true,
    compile: {
      file: "freetype.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_CRT_SECURE_NO_WARNINGS", "FT2_BUILD_LIBRARY"),
        // mupdf's trimmed module/option set, same as the build defines
        '/DFT_CONFIG_MODULES_H="slimftmodules.h"',
        '/DFT_CONFIG_OPTIONS_H="slimftoptions.h"',
        "/I",
        "include",
        "/I",
        "..\\..\\..\\ext\\mupdf\\scripts\\freetype",
        "/I",
        "..\\..\\..\\ext\\a-brotli",
        "/wd4018",
        "/wd4100",
        "/wd4101",
        "/wd4244",
        "/wd4267",
        "/wd4312",
        "/wd4701",
        "/wd4706",
        "/wd4996",
      ],
    },
  },
  {
    name: "gumbo",
    repo: "https://github.com/ArtifexSoftware/thirdparty-gumbo-parser.git",
    rev: "v0.10.1",
    allowLocalRepo: true,
    writes: "gumbo.h, gumbo.c",
    generate: genGumbo,
    compile: {
      file: "gumbo.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_HAS_ITERATOR_DEBUGGING=0"),
        "/wd4018",
        "/wd4100",
        "/wd4132",
        "/wd4189",
        "/wd4204",
        "/wd4244",
        "/wd4245",
        "/wd4267",
        "/wd4305",
        "/wd4306",
        "/wd4389",
        "/wd4456",
        "/wd4701",
        "/wd4702",
      ],
    },
  },
  {
    name: "harfbuzz",
    homepage: "https://harfbuzz.org/",
    repo: "https://github.com/ArtifexSoftware/thirdparty-harfbuzz",
    rev: "c28ba35a1e6dc4b6b32e2b1d11fd2a408f6c86bb",
    writes: "hb*.h, harfbuzz.cc, COPYING",
    generate: genHarfbuzz,
    copies: ["COPYING"],
    // the shipped header set follows upstream's, so stale ones must not linger
    wipeOutDir: true,
    compile: {
      file: "harfbuzz.cc",
      args: [
        "/TP",
        "/EHsc",
        "/std:c++latest",
        // one TU of templated OpenType tables blows past the 64k section limit
        "/bigobj",
        ...defines("_CRT_SECURE_NO_WARNINGS", "HAVE_FALLBACK=1", "HAVE_OT", "HAVE_FREETYPE"),
        "/I",
        ".",
        "/I",
        "..\\..\\..\\ext\\mupdf\\scripts\\freetype",
        "/I",
        "..\\..\\..\\ext\\a-freetype\\include",
        "/wd4100",
        "/wd4127",
        "/wd4146",
        "/wd4244",
        "/wd4245",
        "/wd4267",
        "/wd4310",
        "/wd4456",
        "/wd4457",
        "/wd4458",
        "/wd4459",
        "/wd4505",
        "/wd4701",
        "/wd4702",
        "/wd4706",
        "/wd4805",
        "/wd4996",
      ],
    },
  },
  {
    name: "jbig2dec",
    homepage: "https://github.com/ArtifexSoftware/jbig2dec",
    repo: "https://github.com/ArtifexSoftware/jbig2dec",
    rev: "dc15c39bbbddc90f79c14563d2eb5a794106be8f",
    writes: "jbig2.h, jbig2dec.c, COPYING, LICENSE",
    generate: genJbig2dec,
    // jbig2dec is AGPL, so the notices have to ship with the source. This is the
    // only copy in the tree now that ext/jbig2dec is gone; AUTHORS points at
    // ext/a-jbig2dec/COPYING.
    copies: ["COPYING", "LICENSE"],
    compile: {
      file: "jbig2dec.c",
      args: [
        ...defines(
          "WIN32",
          "_WIN32",
          "NDEBUG",
          "_CRT_SECURE_NO_WARNINGS",
          "HAVE_STRING_H=1",
          "JBIG_NO_MEMENTO",
          "_HAS_ITERATOR_DEBUGGING=0",
        ),
        "/wd4018",
        "/wd4100",
        "/wd4146",
        "/wd4244",
        "/wd4267",
        "/wd4456",
        "/wd4701",
      ],
    },
  },
  {
    name: "lcms2",
    homepage: "https://littlecms.com/",
    repo: "https://github.com/ArtifexSoftware/thirdparty-lcms2",
    rev: "d69c64417c4a33a4629957fc0e08f4f4c5abcc3d",
    writes: "lcms2mt.h, lcms2mt_plugin.h, extra_xform.h, lcms2.c, LICENSE, AUTHORS",
    generate: genLcms2,
    copies: ["LICENSE", "AUTHORS"],
    compile: {
      file: "lcms2.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_CRT_SECURE_NO_WARNINGS", "_HAS_ITERATOR_DEBUGGING=0"),
        "/I",
        ".",
        "/wd4100",
        "/wd4244",
      ],
    },
  },
  {
    name: "libarchive",
    homepage: "https://www.libarchive.org/",
    repo: "https://github.com/libarchive/libarchive",
    rev: "v3.8.8",
    writes: "libarchive/*.h, libarchive.c, COPYING",
    generate: genLibarchive,
    copies: ["COPYING"],
    compile: {
      file: "libarchive.c",
      args: [
        ...defines(
          "WIN32",
          "_WIN32",
          "NDEBUG",
          "_CRT_SECURE_NO_WARNINGS",
          "LIBARCHIVE_STATIC",
          'PLATFORM_CONFIG_H="config_windows.h"',
          "BZ_NO_STDIO",
          "LZMA_API_STATIC",
        ),
        "/I",
        ".",
        "/I",
        "libarchive",
        // config_windows.h is hand-maintained there, not generated
        "/I",
        "..\\..\\..\\ext\\a-libarchive",
        "/I",
        "..\\..\\..\\ext\\a-zlib",
        "/I",
        "..\\..\\..\\ext\\a-bzip2",
        "/I",
        "..\\..\\..\\ext\\liblzma\\api",
        "/wd4018",
        "/wd4054",
        "/wd4055",
        "/wd4090",
        "/wd4098",
        "/wd4100",
        "/wd4127",
        "/wd4130",
        "/wd4146",
        "/wd4152",
        "/wd4200",
        "/wd4201",
        "/wd4244",
        "/wd4245",
        "/wd4267",
        "/wd4305",
        "/wd4389",
        "/wd4456",
        "/wd4457",
        "/wd4701",
        "/wd4703",
        "/wd4706",
        "/wd4996",
      ],
    },
  },
  {
    name: "libwebp",
    homepage: "https://developers.google.com/speed/webp",
    repo: "https://github.com/webmproject/libwebp",
    rev: "v1.6.0",
    writes: "webp/*.h, libwebp.c, COPYING, PATENTS, AUTHORS",
    generate: genLibwebp,
    copies: ["COPYING", "PATENTS", "AUTHORS"],
    compile: {
      file: "libwebp.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_CRT_SECURE_NO_WARNINGS", "_HAS_ITERATOR_DEBUGGING=0"),
        "/I",
        ".",
        "/wd4057",
        "/wd4127",
        "/wd4204",
        "/wd4244",
        "/wd4245",
        "/wd4310",
        "/wd4701",
      ],
    },
  },
  {
    name: "mujs",
    homepage: "https://mujs.com/",
    repo: "https://github.com/ArtifexSoftware/mujs",
    rev: "e892c9fdbbddba94e52f656ccb378ed4885e30cc",
    writes: "mujs.h, mujs.c, regexp.h, COPYING",
    generate: genMujs,
    // regexp.h is used directly by MuPDF text search.
    copies: ["regexp.h", "COPYING"],
    compile: {
      file: "mujs.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_HAS_ITERATOR_DEBUGGING=0"),
        "/wd4090",
        "/wd4100",
        "/wd4127",
        "/wd4146",
        "/wd4310",
        "/wd4324",
        "/wd4702",
        "/wd4706",
      ],
    },
  },
  {
    name: "openjpeg",
    homepage: "https://www.openjpeg.org/",
    repo: "https://github.com/ArtifexSoftware/thirdparty-openjpeg",
    rev: "957029eb875eee1118743f200cb86da9d8289de2",
    writes: "*.h, openjpeg.c, LICENSE",
    generate: genOpenjpeg,
    copies: ["LICENSE"],
    // the shipped header set follows upstream's, so stale ones must not linger
    wipeOutDir: true,
    compile: {
      file: "openjpeg.c",
      args: [
        ...defines("_CRT_SECURE_NO_WARNINGS", "USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS"),
        "/I",
        ".",
        "/wd4005",
        "/wd4100",
        "/wd4127",
        "/wd4244",
        "/wd4310",
        "/wd4389",
        "/wd4456",
        "/wd4702",
      ],
    },
  },
  {
    name: "unrar",
    homepage: "https://www.rarlab.com/rar_add.htm",
    // rarlab ships tarballs, not git; this mirror commits each one verbatim
    repo: "https://github.com/aawc/unrar",
    rev: "b82477a7d45b6998fbcfc504a0fa59aca6345e4c",
    writes: "dll.hpp, unrar.cpp, license.txt, acknow.txt",
    generate: genUnrar,
    copies: ["license.txt", "acknow.txt"],
    compile: {
      file: "unrar.cpp",
      args: [
        "/TP",
        "/EHsc",
        ...defines("WIN32", "_WIN32", "NDEBUG", "UNRAR", "RARDLL", "SILENT"),
        "/I",
        ".",
        "/wd4005",
        "/wd4100",
        "/wd4127",
        "/wd4189",
        "/wd4201",
        "/wd4211",
        "/wd4244",
        "/wd4310",
        "/wd4389",
        "/wd4456",
        "/wd4458",
        "/wd4459",
        "/wd4505",
        "/wd4701",
        "/wd4702",
        "/wd4706",
        "/wd4709",
        "/wd4731",
        "/wd4828",
        "/wd4996",
      ],
    },
  },
  {
    name: "zlib",
    homepage: "https://zlib.net/",
    repo: "https://github.com/madler/zlib",
    rev: "v1.3.2",
    writes: "zlib.h, zlib.c, LICENSE",
    generate: genZlib,
    copies: ["LICENSE"],
    compile: {
      file: "zlib.c",
      args: [
        ...defines("WIN32", "_WIN32", "NDEBUG", "_CRT_SECURE_NO_WARNINGS", "_HAS_ITERATOR_DEBUGGING=0"),
        "/wd4131",
        "/wd4005",
        "/wd4244",
        "/wd4245",
        "/wd4267",
        "/wd4996",
      ],
    },
  },
  {
    name: "zopfli",
    homepage: "https://github.com/google/zopfli",
    repo: "https://github.com/google/zopfli",
    rev: "ccf9f0588d4a4509cb1040310ec122243e670ee6",
    writes: "zopflipng/*.h, zopfli.cpp, COPYING",
    generate: genZopfli,
    copies: ["COPYING"],
    compile: {
      file: "zopfli.cpp",
      args: [
        "/TP",
        "/EHsc",
        ...defines("WIN32", "_WIN32", "NDEBUG", "_CRT_SECURE_NO_WARNINGS", "_HAS_ITERATOR_DEBUGGING=0"),
        "/I",
        ".",
        "/wd4018",
        "/wd4100",
        "/wd4127",
        "/wd4244",
        "/wd4267",
        "/wd4334",
        "/wd4305",
        "/wd4457",
        "/wd4459",
        "/wd4477",
        "/wd4530",
        "/wd4702",
        "/wd4996",
      ],
    },
  },
];

function defines(...names: string[]): string[] {
  return names.flatMap((name) => ["/D", name]);
}

// ---------------------------------------------------------------------------
// driver
// ---------------------------------------------------------------------------

type Args = {
  lib: Lib;
  repo: string;
  rev: string;
  keep: boolean;
};

const depsDir = "deps";

function usage(err?: string): never {
  if (err) {
    console.error(`error: ${err}\n`);
  }
  const list = libs.map((lib) => `  -${lib.name.padEnd(10)} ${lib.writes}`).join("\n");
  console.error(`Usage: bun cmd/amalgam.ts -<library> [repo-url] [git-tag-or-checkin] [-keep]

Clones the library under deps/<library>, amalgamates it into ext/a-<library>/,
validates the result with cl.exe, and writes version.txt next to it.

Libraries (and what each writes into ext/a-<library>/):
${list}

Options:
  -keep   reuse an existing deps/<library> checkout instead of re-cloning

Each library defaults to the repo and revision recorded in ext/versions.txt,
so plain 'bun cmd/amalgam.ts -zlib' regenerates the current copy.
`);
  process.exit(err ? 1 : 0);
}

function parseArgs(): Args {
  const argv = process.argv.slice(2);
  if (argv.includes("-h") || argv.includes("-help") || argv.includes("--help")) {
    usage();
  }

  let lib: Lib | undefined;
  const positional: string[] = [];
  let keep = false;
  for (const arg of argv) {
    if (!arg.startsWith("-")) {
      positional.push(arg);
      continue;
    }
    if (arg === "-keep" || arg === "--keep") {
      keep = true;
      continue;
    }
    const found = libs.find((l) => l.name === arg.slice(1).replace(/^-/, ""));
    if (!found) {
      usage(`unknown option ${arg}`);
    }
    if (lib) {
      usage("pick exactly one library");
    }
    lib = found;
  }

  if (!lib) {
    usage("no library selected");
  }
  if (positional.length > 2) {
    usage("too many arguments");
  }
  return { lib, repo: positional[0] ?? lib.repo, rev: positional[1] ?? lib.rev, keep };
}

async function checkout(lib: Lib, dir: string, repo: string, rev: string, keep: boolean): Promise<void> {
  mkdirSync(depsDir, { recursive: true });
  if (!keep && existsSync(dir)) {
    rmSync(dir, { recursive: true, force: true });
  }

  if (!existsSync(dir)) {
    if (lib.allowLocalRepo && existsSync(repo)) {
      cpSync(repo, dir, { recursive: true });
    } else {
      await runLogged("git", ["clone", repo, dir]);
    }
  }

  // a local copy may not carry a .git, in which case rev is informational
  if (existsSync(join(dir, ".git"))) {
    await runLogged("git", ["-C", dir, "fetch", "--tags", "--force"]);
    await runLogged("git", ["-C", dir, "checkout", "--force", rev]);
  }
}

function versionText(lib: Lib, dir: string, repo: string, rev: string): string {
  let repoUrl = repo;
  let commitSha1 = rev;
  if (existsSync(join(dir, ".git"))) {
    commitSha1 = gitOutput(["rev-parse", "HEAD"], dir);
    repoUrl = gitOutput(["config", "--get", "remote.origin.url"], dir, false) || repo;
  }

  const lines: string[] = [];
  if (lib.homepage) {
    lines.push(`project_homepage: ${lib.homepage}`);
  }
  lines.push(`repo_url: ${repoUrl}`, `revision: ${rev}`, `commit_sha1: ${commitSha1}`);

  const githubUrl = normalizeGithubUrl(repoUrl);
  if (githubUrl) {
    lines.push(`github_url: ${githubUrl}`, `github_commit_url: ${githubUrl}/commit/${commitSha1}`);
  }
  return lines.join("\n") + "\n";
}

function writeFiles(dir: string, files: Map<string, string>): string[] {
  const written: string[] = [];
  for (const [name, text] of files) {
    const path = join(dir, name);
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, text);
    written.push(path);
  }
  return written;
}

// The ARM64 cross compiler next to the x64 cl.exe on PATH. Same toolset
// version, so the INCLUDE set by the developer prompt still applies (we only
// compile, never link, so LIB doesn't matter).
function findArm64Cl(): string {
  const found = whichExe("cl.exe");
  if (!found) {
    throw new Error("cl.exe not in PATH, run from a Visual Studio developer prompt");
  }
  const arm64 = found.replace(/\\x64\\cl\.exe$/i, "\\arm64\\cl.exe");
  if (arm64 === found || !existsSync(arm64)) {
    throw new Error(`no ARM64 cl.exe next to ${found}, install the VS ARM64 build tools`);
  }
  return arm64;
}

function whichExe(exe: string): string {
  const proc = Bun.spawnSync(["where", exe], { stdout: "pipe", stderr: "pipe" });
  return proc.exitCode === 0 ? proc.stdout.toString().split("\n")[0]!.trim() : "";
}

// Compiles for x64 and ARM64: each arch takes different #ifdef branches (SSE
// vs NEON), so only building both catches a chunk that breaks the other one.
async function validateCompile(lib: Lib, tmpDir: string, files: Map<string, string>): Promise<void> {
  rmSync(tmpDir, { recursive: true, force: true });
  mkdirSync(tmpDir, { recursive: true });
  writeFiles(tmpDir, files);

  detectVisualStudio2026();
  const args = [...clCommonArgs, ...lib.compile.args, lib.compile.file];
  console.log("compiling for x64");
  await runLogged("cl.exe", args, tmpDir);
  console.log("compiling for arm64");
  await runLogged(findArm64Cl(), args, tmpDir);
}

// Licenses and hand-picked headers copied straight from upstream. Several are
// the only copy left in the tree, so re-copy them on every regeneration.
// Text files are written with LF, like the generated ones: upstream line
// endings vary by project and by how git checked them out, which would
// otherwise show up as a whole-file diff on every regeneration.
function copyFromCheckout(checkoutDir: string, outDir: string, names: string[]): string[] {
  const written: string[] = [];
  for (const name of names) {
    const src = join(checkoutDir, name);
    if (!existsSync(src)) {
      throw new Error(`missing file in checkout: ${src}`);
    }
    const dst = join(outDir, name);
    mkdirSync(dirname(dst), { recursive: true });
    writeFileSync(dst, toLf(readFileSync(src)));
    written.push(dst);
  }
  return written;
}

// A NUL byte means binary, and binaries are copied byte for byte.
function toLf(data: Buffer): Buffer | string {
  if (data.includes(0)) {
    return data;
  }
  return data.toString("utf-8").replace(/\r\n/g, "\n");
}

async function main(): Promise<void> {
  const args = parseArgs();
  const lib = args.lib;
  const checkoutDir = join(depsDir, lib.name);
  const outDir = join("ext", `a-${lib.name}`);
  const tmpDir = join("cmd", "tmp", `a-${lib.name}`);

  await checkout(lib, checkoutDir, args.repo, args.rev, args.keep);

  const ctx: Ctx = { checkoutDir, files: new Map() };
  lib.generate(ctx);
  const version = versionText(lib, checkoutDir, args.repo, args.rev);

  await validateCompile(lib, tmpDir, ctx.files);

  if (lib.wipeOutDir) {
    rmSync(outDir, { recursive: true, force: true });
  }
  mkdirSync(outDir, { recursive: true });

  const written = writeFiles(outDir, ctx.files);
  writeFileSync(join(outDir, "version.txt"), version);
  written.push(join(outDir, "version.txt"));
  written.push(...copyFromCheckout(checkoutDir, outDir, lib.copies ?? []));
  for (const path of written) {
    console.log(`wrote ${path}`);
  }
}

await main();
