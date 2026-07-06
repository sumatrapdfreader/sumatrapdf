/**
 * Build SumatraPDF dependency static libraries on Linux (mupdf/ and ext/).
 *
 * Usage:
 *   bun cmd/build-linux.ts -debug
 *   bun cmd/build-linux.ts -release
 *   bun cmd/build-linux.ts -debug -clean
 *
 * Output: out/linux-dbg64/lib/*.a (or out/linux-rel64 for -release)
 *
 * Mirrors the dependency projects in premake5.lua / premake5.files.lua.
 * Also builds portable src/base code, but not the Windows-only SumatraPDF UI.
 */

import { existsSync, mkdirSync, rmSync } from "node:fs";
import { writeFile } from "node:fs/promises";
import { join, basename } from "node:path";
import { cpus } from "node:os";
import {
  type BuildTools,
  type EmbedFormat,
  type LibDef,
  DEFAULT_JOBS,
  FONT_FILES,
  buildLibrary,
  embedBinaryFile,
} from "./build-deps-common";
import {
  zlib,
  unrar,
  libwebp,
  skcms,
  highway,
  libjxl,
  libheif,
  mupdfLibs as mupdfLibsBase,
  mupdf as mupdfBase,
} from "./build-lib-defs";

type LinuxArch = "arm64" | "x64";

function requireLinux(): void {
  if (process.platform !== "linux") {
    console.error(`This script must be run on Linux (got ${process.platform})`);
    process.exit(1);
  }
}

function detectLinuxArch(): LinuxArch {
  if (process.arch === "arm64") return "arm64";
  if (process.arch === "x64") return "x64";
  const proc = Bun.spawnSync(["uname", "-m"]);
  const m = proc.stdout.toString().trim();
  return m === "aarch64" || m === "arm64" ? "arm64" : "x64";
}

function resolveTool(role: string, candidates: string[]): string {
  for (const name of candidates) {
    if (Bun.which(name)) return name;
  }
  console.error(`Could not find ${role} (tried: ${candidates.join(", ")})`);
  console.error("Install build-essential (gcc/g++) and binutils (ar, objcopy).");
  process.exit(1);
}

function resolveLinuxTools(arch: LinuxArch): BuildTools {
  const cc = resolveTool("C compiler", ["clang", "gcc"]);
  const cxx = resolveTool("C++ compiler", ["clang++", "g++"]);
  const ar = resolveTool("archiver", ["ar", "llvm-ar"]);
  // x86_64: GNU objcopy embeds fonts as ELF .o; arm64 uses xxd+cc (see embedFonts).
  const embed =
    arch === "x64"
      ? resolveTool("objcopy for binary embedding", ["objcopy", "llvm-objcopy"])
      : cc;
  return { cc, cxx, ar, embed };
}

const CMARK_GFM_FILES = [
  {
    dir: "ext/cmark-gfm/src",
    patterns: [
      "arena.c",
      "blocks.c",
      "buffer.c",
      "cmark.c",
      "cmark_ctype.c",
      "footnotes.c",
      "houdini_href_e.c",
      "houdini_html_e.c",
      "houdini_html_u.c",
      "html.c",
      "inlines.c",
      "iterator.c",
      "linked_list.c",
      "map.c",
      "node.c",
      "plugin.c",
      "references.c",
      "registry.c",
      "scanners.c",
      "syntax_extension.c",
      "utf8.c",
    ],
  },
  {
    dir: "ext/cmark-gfm/extensions",
    patterns: [
      "autolink.c",
      "core-extensions.c",
      "ext_scanners.c",
      "strikethrough.c",
      "table.c",
      "tagfilter.c",
      "tasklist.c",
      "autoheaderid.c",
    ],
  },
];

function makeLibdjvu(): LibDef {
  return {
    ...structuredClone(libdjvuBase()),
    defines: [
      "UNIX",
      "NEED_JPEG_DECODER",
      "HAVE_PTHREAD",
      "HAS_WCHAR=1",
      "HAS_WCTYPE=1",
      "HAS_MBSTATE=1",
      "DDJVUAPI=",
      "MINILISPAPI=",
      "DEBUGLVL=0",
      "DISABLE_MMX",
    ],
  };
}

function libdjvuBase(): LibDef {
  return {
    name: "libdjvu",
    alwaysOptimize: true,
    exceptions: true,
    defines: [],
    includes: ["ext/libjpeg-turbo/src"],
    files: [
      {
        dir: "ext/libdjvu",
        patterns: [
          "Arrays.cpp",
          "atomic.cpp",
          "BSByteStream.cpp",
          "BSEncodeByteStream.cpp",
          "ByteStream.cpp",
          "DataPool.cpp",
          "DjVmDir0.cpp",
          "DjVmDoc.cpp",
          "DjVmNav.cpp",
          "DjVuAnno.cpp",
          "DjVuDocEditor.cpp",
          "DjVuDocument.cpp",
          "DjVuDumpHelper.cpp",
          "DjVuErrorList.cpp",
          "DjVuFile.cpp",
          "DjVuFileCache.cpp",
          "DjVuGlobal.cpp",
          "DjVuGlobalMemory.cpp",
          "DjVuImage.cpp",
          "DjVuInfo.cpp",
          "DjVuMessage.cpp",
          "DjVuMessageLite.cpp",
          "DjVuNavDir.cpp",
          "DjVuPalette.cpp",
          "DjVuPort.cpp",
          "DjVuText.cpp",
          "DjVuToPS.cpp",
          "GBitmap.cpp",
          "GContainer.cpp",
          "GException.cpp",
          "GIFFManager.cpp",
          "GMapAreas.cpp",
          "GOS.cpp",
          "GPixmap.cpp",
          "GRect.cpp",
          "GScaler.cpp",
          "GSmartPointer.cpp",
          "GString.cpp",
          "GThreads.cpp",
          "GUnicode.cpp",
          "GURL.cpp",
          "IFFByteStream.cpp",
          "IW44EncodeCodec.cpp",
          "IW44Image.cpp",
          "JB2EncodeCodec.cpp",
          "DjVmDir.cpp",
          "JB2Image.cpp",
          "JPEGDecoder.cpp",
          "MMRDecoder.cpp",
          "MMX.cpp",
          "UnicodeByteStream.cpp",
          "XMLParser.cpp",
          "XMLTags.cpp",
          "ZPCodec.cpp",
          "ddjvuapi.cpp",
          "debug.cpp",
          "miniexp.cpp",
        ],
      },
    ],
  };
}

function makeLibarchive(outDir: string): LibDef {
  const liblzmaConfigDir = join(outDir, "generated", "liblzma");
  return {
    name: "libarchive",
    alwaysOptimize: true,
    defines: [
      "LIBARCHIVE_STATIC",
      'PLATFORM_CONFIG_H="config_linux.h"',
      "BZ_NO_STDIO",
      "HAVE_CONFIG_H",
      "LZMA_API_STATIC",
    ],
    includes: [
      liblzmaConfigDir,
      "ext/libarchive/libarchive",
      "ext/libarchive",
      "ext/zlib",
      "ext/bzip2",
      "ext/lzma/C",
      "ext/liblzma/api",
      "ext/liblzma/common",
      "ext/liblzma/check",
      "ext/liblzma/delta",
      "ext/liblzma/lz",
      "ext/liblzma/lzma",
      "ext/liblzma/rangecoder",
      "ext/liblzma/simple",
      "ext/liblzma",
    ],
    files: [
      {
        dir: "ext/libarchive/libarchive",
        patterns: [
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
          "archive_blake2s_ref.c",
          "archive_blake2sp_ref.c",
          "archive_read.c",
          "archive_read_add_passphrase.c",
          "archive_read_append_filter.c",
          "archive_read_data_into_fd.c",
          "archive_read_extract.c",
          "archive_read_extract2.c",
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
          "archive_read_disk_set_standard_lookup.c",
          "archive_read_disk_posix.c",
          "archive_parse_date.c",
          "filter_fork_posix.c",
          "xxhash.c",
        ],
      },
      {
        dir: "ext/bzip2",
        patterns: [
          "blocksort.c",
          "bzlib.c",
          "bz_internal_error.c",
          "compress.c",
          "crctable.c",
          "decompress.c",
          "huffman.c",
          "randtable.c",
        ],
      },
      { dir: "ext/lzma/C", patterns: ["LzmaDec.c", "Bra86.c", "Bra.c"] },
      {
        dir: "ext/liblzma",
        patterns: [
          "common/alone_decoder.c",
          "common/auto_decoder.c",
          "common/block_decoder.c",
          "common/block_header_decoder.c",
          "common/block_util.c",
          "common/common.c",
          "common/filter_common.c",
          "common/filter_decoder.c",
          "common/filter_flags_decoder.c",
          "common/index.c",
          "common/index_decoder.c",
          "common/index_hash.c",
          "common/stream_decoder.c",
          "common/stream_flags_common.c",
          "common/stream_flags_decoder.c",
          "common/vli_decoder.c",
          "common/vli_size.c",
          "check/check.c",
          "check/crc32_fast.c",
          "check/crc64_fast.c",
          "lz/lz_decoder.c",
          "lzma/lzma_decoder.c",
          "lzma/lzma2_decoder.c",
          "rangecoder/price_table.c",
          "delta/delta_common.c",
          "delta/delta_decoder.c",
          "simple/simple_coder.c",
          "simple/simple_decoder.c",
          "simple/x86.c",
        ],
      },
    ],
  };
}

function makeDav1d(arch: LinuxArch, generatedDir: string): LibDef {
  const defines = [
    "CONFIG_16BPC=1",
    "CONFIG_8BPC=1",
    "CONFIG_LOG=1",
    "ENDIANNESS_BIG=0",
    "HAVE_ASM=0",
    "STACK_ALIGNMENT=16",
  ];
  const files: LibDef["files"] = [
    {
      dir: "ext/dav1d/src",
      patterns: [
        "lib.c",
        "thread_task.c",
        "cdf.c",
        "cpu.c",
        "ctx.c",
        "data.c",
        "decode.c",
        "dequant_tables.c",
        "getbits.c",
        "intra_edge.c",
        "itx_1d.c",
        "lf_mask.c",
        "log.c",
        "mem.c",
        "msac.c",
        "obu.c",
        "pal.c",
        "picture.c",
        "qm.c",
        "ref.c",
        "refmvs.c",
        "scan.c",
        "tables.c",
        "warpmv.c",
        "wedge.c",
        "sumatra_bitdepth_8.c",
        "sumatra_bitdepth_8_2.c",
        "sumatra_bitdepth_16.c",
        "sumatra_bitdepth_16_2.c",
      ],
    },
  ];
  const includes = ["ext/dav1d", "ext/dav1d/include", generatedDir];

  if (arch === "arm64") {
    defines.push("ARCH_AARCH64=1", "ARCH_ARM=0", "ARCH_X86=0", "PREFIX=1");
  } else {
    defines.push("ARCH_AARCH64=0", "ARCH_ARM=0", "ARCH_X86=1", "ARCH_X86_32=0", "ARCH_X86_64=1");
    files.push({ dir: "ext/dav1d/src/x86", patterns: ["cpu.c"] });
  }

  return {
    name: "dav1d",
    alwaysOptimize: true,
    defines,
    includes,
    files,
  };
}

function makeChm(): LibDef {
  return {
    name: "chm",
    alwaysOptimize: true,
    defines: ["_stricmp=strcasecmp", "_strnicmp=strncasecmp"],
    includes: [],
    extraCflags: ["-include", "limits.h"],
    files: [{ dir: "ext/CHMLib", patterns: ["chm_lib.c", "lzx.c"] }],
  };
}

const djvudec: LibDef = {
  name: "djvudec",
  alwaysOptimize: true,
  defines: [],
  includes: [],
  files: [{ dir: "ext/djvudec", patterns: ["djvu.c"] }],
};

function makeMupdfLibs(): LibDef {
  const lib = structuredClone(mupdfLibsBase);
  lib.defines = lib.defines.filter((d) => d !== "_CRT_SECURE_NO_WARNINGS");
  lib.defines.push("CMARK_GFM_STATIC_DEFINE");
  lib.includes.push(
    "ext/cmark-gfm/src",
    "ext/cmark-gfm/extensions",
    "mupdf/scripts/cmark-gfm",
  );
  lib.files.push(...CMARK_GFM_FILES);
  return lib;
}

function makeMupdf(arch: LinuxArch): LibDef {
  const lib = structuredClone(mupdfBase);
  lib.defines = lib.defines.filter((d) => !d.startsWith("_CRT"));
  lib.defines.push(
    "FZ_ENABLE_PDF=1",
    "FZ_ENABLE_MD=1",
    "HAVE_LIBARCHIVE",
    "LIBARCHIVE_STATIC",
    "CMARK_GFM_STATIC_DEFINE",
    "HAVE_PTHREAD",
  );
  if (arch === "arm64") {
    lib.defines.push("ARCH_HAS_NEON=1");
    lib.extraCflags = ["-fms-extensions"];
  } else {
    lib.extraCflags = ["-msse4.1", "-DARCH_HAS_SSE=1", "-fms-extensions"];
  }
  lib.includes.push(
    "ext/libarchive/libarchive",
    "ext/cmark-gfm/src",
    "ext/cmark-gfm/extensions",
    "mupdf/scripts/cmark-gfm",
  );

  const fitz = lib.files.find((g) => g.dir === "mupdf/source/fitz");
  if (fitz) {
    const pats = fitz.patterns as string[];
    const jxrIdx = pats.indexOf("load-jxr-win.c");
    if (jxrIdx >= 0) pats[jxrIdx] = "load-jxr.c";
    for (const extra of ["cull-device.c", "options.c"]) {
      if (!pats.includes(extra)) pats.push(extra);
    }
  }
  const html = lib.files.find((g) => g.dir === "mupdf/source/html");
  if (html && !html.patterns.includes("md.c")) {
    (html.patterns as string[]).push("md.c");
  }
  const pdf = lib.files.find((g) => g.dir === "mupdf/source/pdf");
  if (pdf && !pdf.patterns.includes("pdf-struct.c")) {
    (pdf.patterns as string[]).push("pdf-struct.c");
  }
  const tools = lib.files.find((g) => g.dir === "mupdf/source/tools");
  if (tools && !tools.patterns.includes("muraster.c")) {
    (tools.patterns as string[]).push("muraster.c");
  }
  const pkcs7 = lib.files.find((g) => g.dir === "mupdf/source/helpers/pkcs7");
  if (pkcs7) pkcs7.patterns = ["pkcs7-openssl.c"];

  return lib;
}

async function writeLiblzmaConfig(outDir: string): Promise<void> {
  const dir = join(outDir, "generated", "liblzma");
  mkdirSync(dir, { recursive: true });
  const src = await Bun.file("ext/liblzma/config_linux.h").text();
  await writeFile(join(dir, "config.h"), src);
}

async function writeDav1dConfig(generatedDir: string, arch: LinuxArch): Promise<void> {
  const isArm = arch === "arm64";
  const text = `/* Generated by cmd/build-linux.ts for dav1d on Linux */
#pragma once
#define ARCH_AARCH64 ${isArm ? 1 : 0}
#define ARCH_ARM 0
#define ARCH_X86 ${isArm ? 0 : 1}
#define ARCH_X86_32 0
#define ARCH_X86_64 ${isArm ? 0 : 1}
#define CONFIG_16BPC 1
#define CONFIG_8BPC 1
#define CONFIG_LOG 1
#define ENDIANNESS_BIG 0
#define HAVE_ASM 0
#define STACK_ALIGNMENT 16
#define PREFIX ${isArm ? 1 : 0}
`;
  mkdirSync(generatedDir, { recursive: true });
  await writeFile(join(generatedDir, "config.h"), text);
}

async function embedFonts(
  tools: BuildTools,
  outDir: string,
  arch: LinuxArch,
): Promise<string[]> {
  console.log("Embedding font files into mupdf...");
  const format: EmbedFormat = arch === "x64" ? "elf" : "macho";
  const objs: string[] = [];
  for (const font of FONT_FILES) {
    if (!existsSync(font.path)) {
      console.error(`  WARNING: font not found: ${font.path}`);
      continue;
    }
    const base = basename(font.path, `.${font.ext}`).replace(/-/g, "_");
    const sym = `_binary_${base}_${font.ext}`;
    const obj = join(outDir, "obj", "fonts", `${base}.o`);
    await embedBinaryFile(tools, format, font.path, obj, sym);
    objs.push(obj);
  }
  return objs;
}

function makeUnrar(): LibDef {
  const lib = structuredClone(unrar);
  lib.defines = lib.defines.filter((d) => d !== "_CRT_SECURE_NO_WARNINGS");
  const files = lib.files[0];
  files.patterns = files.patterns.filter((p) => p !== "isnt.cpp" && p !== "motw.cpp");
  return lib;
}

const DEP_LIBS_BASE = [
  {
    name: "base",
    alwaysOptimize: false,
    defines: [],
    includes: ["src"],
    files: [
      {
        dir: "src/base",
        patterns: [
          "Base.cpp",
          "Base_posix.cpp",
          "Arena.cpp",
          "Arena_posix.cpp",
          "ByteOrderDecoder.cpp",
          "CmdLineArgsIter.cpp",
          "Color.cpp",
          "CssParser.cpp",
          "Dict.cpp",
          "File.cpp",
          "File_posix.cpp",
          "Geom.cpp",
          "HtmlTags.cpp",
          "JsonParser.cpp",
          "SettingsUtil.cpp",
          "SquareTreeParser.cpp",
          "StrQueue.cpp",
          "Thread.cpp",
          "Str.cpp",
          "StrUtf8.cpp",
          "StrFormatParse.cpp",
          "StrVec.cpp",
          "Strconv.cpp",
        ],
      },
    ],
  },
  zlib,
  makeUnrar,
  makeLibdjvu,
  makeChm,
  djvudec,
  "libarchive",
  libwebp,
  makeDav1d,
  libheif,
  skcms,
  highway,
  libjxl,
  makeMupdfLibs,
  makeMupdf,
] as const;

export interface LinuxBuildOptions {
  outDir: string;
  isRelease?: boolean;
  clean?: boolean;
  tools?: Partial<BuildTools>;
  jobs?: number;
}

export async function buildLinux(opts: LinuxBuildOptions): Promise<void> {
  requireLinux();
  const arch = detectLinuxArch();
  const tools: BuildTools = { ...resolveLinuxTools(arch), ...opts.tools };
  const jobs = opts.jobs ?? DEFAULT_JOBS;
  const isRelease = opts.isRelease ?? false;
  const outDir = opts.outDir;
  const generatedDir = join(outDir, "generated", "dav1d");

  if (opts.clean && existsSync(outDir)) {
    console.log(`Cleaning ${outDir}...`);
    rmSync(outDir, { recursive: true, force: true });
  }

  const startTime = performance.now();
  const config = isRelease ? "release" : "debug";
  console.log(`\n=== Building SumatraPDF dependencies (${config}, Linux ${arch}) ===\n`);
  console.log(`Output: ${outDir}`);
  console.log(`Tools: ${tools.cc}, ${tools.cxx}`);
  console.log(`Parallel jobs: ${jobs}\n`);

  mkdirSync(join(outDir, "obj"), { recursive: true });
  mkdirSync(join(outDir, "lib"), { recursive: true });
  await writeDav1dConfig(generatedDir, arch);
  await writeLiblzmaConfig(outDir);

  const commonDefines: string[] = [];
  const cxxFlags: string[] = ["-D__GXX_TYPEINFO_EQUALITY_INLINE=1"];

  const fontObjs = await embedFonts(tools, outDir, arch);

  for (const entry of DEP_LIBS_BASE) {
    let lib: LibDef;
    if (entry === "libarchive") {
      lib = makeLibarchive(outDir);
    } else if (typeof entry === "function") {
      if (entry === makeDav1d) {
        lib = makeDav1d(arch, generatedDir);
      } else if (entry === makeMupdf) {
        lib = makeMupdf(arch);
      } else {
        lib = entry();
      }
    } else {
      lib = structuredClone(entry);
      if (lib.defines) {
        lib.defines = lib.defines.filter((d) => d !== "_CRT_SECURE_NO_WARNINGS");
      }
    }

    const extraObjs = lib.name === "mupdf" ? fontObjs : undefined;
    await buildLibrary(lib, outDir, isRelease, {
      tools,
      commonDefines,
      cxxFlags,
      jobs,
      extraObjs,
    });
  }

  const elapsed = ((performance.now() - startTime) / 1000).toFixed(1);
  console.log(`\n=== Dependency build complete (${config}) in ${elapsed}s ===`);
  console.log(`Static libraries: ${join(outDir, "lib")}\n`);
}

if (import.meta.main) {
  const args = Bun.argv.slice(2);
  let doDebug = false;
  let doRelease = false;
  let doClean = false;

  for (const arg of args) {
    if (arg === "-debug") doDebug = true;
    else if (arg === "-release") doRelease = true;
    else if (arg === "-clean") doClean = true;
    else {
      console.error(`Unknown argument: ${arg}`);
      console.error("Usage: bun cmd/build-linux.ts [-debug] [-release] [-clean]");
      process.exit(1);
    }
  }

  if (!doDebug && !doRelease) {
    console.error("Usage: bun cmd/build-linux.ts [-debug] [-release] [-clean]");
    process.exit(1);
  }

  const configs: boolean[] = [];
  if (doDebug) configs.push(false);
  if (doRelease) configs.push(true);

  for (const isRelease of configs) {
    await buildLinux({
      outDir: isRelease ? "out/linux-rel64" : "out/linux-dbg64",
      isRelease,
      clean: doClean,
      jobs: Math.max(1, Math.min(4, cpus().length)),
    });
  }
}
