/**
 * Build SumatraPDF dependency static libraries on Linux (ext/).
 *
 * Invoked by cmd/build.ts -linux.
 *
 * Output: out/linux-dbg64/lib/*.a (or out/linux-rel64 / out/linux-asan64 / out/linux-rel64_asan)
 *
 * Mirrors the dependency projects in premake5.lua / premake5.files.lua.
 * Builds portable src/ test tools, but not the Windows-only SumatraPDF UI.
 */

import {
  chmodSync,
  copyFileSync,
  existsSync,
  mkdirSync,
  mkdtempSync,
  readFileSync,
  rmSync,
  writeFileSync,
} from "node:fs";
import { writeFile } from "node:fs/promises";
import { join, basename, dirname } from "node:path";
import { tmpdir } from "node:os";
import {
  type BuildTools,
  type EmbedFormat,
  type LibDef,
  DEFAULT_JOBS,
  FONT_FILES,
  buildLibrary,
  compileAll,
  dropX86OnlyCflags,
  embedBinaryFile,
  objPath,
  spawnCmd,
} from "../deps-build-common";
import {
  aGumbo,
  zlib,
  unrar,
  libwebp,
  jxldec,
  heicdec,
  libjpegTurbo,
  jbig2dec,
  openjpeg,
  freetype,
  lcms2,
  harfbuzz,
  mujs,
  extract,
  brotli,
  cmarkGfm,
  mupdf as mupdfBase,
} from "../deps-build-defs";
import { extractSumatraVersion } from "../util";

type LinuxArch = "arm64" | "x64";

const LINUX_APP_ID = "org.sumatrapdf.SumatraPDF";
const LINUX_DESKTOP_FILE = `packaging/linux/${LINUX_APP_ID}.desktop`;
const LINUX_METAINFO_FILE = `packaging/linux/${LINUX_APP_ID}.metainfo.xml`;
const LINUX_ICON_FILE = "gfx/svg/pdf-32bit.svg";

function requireLinux(): void {
  if (process.platform !== "linux") {
    console.error(`This script must be run on Linux (got ${process.platform})`);
    process.exit(1);
  }
}

// libssl-dev provides libcrypto.so; the runtime package only has libcrypto.so.3
function libCryptoLinkFlags(): string[] {
  const so = [
    "/usr/lib/x86_64-linux-gnu/libcrypto.so",
    "/usr/lib/aarch64-linux-gnu/libcrypto.so",
    "/usr/lib/libcrypto.so",
    "/lib/x86_64-linux-gnu/libcrypto.so",
    "/lib/aarch64-linux-gnu/libcrypto.so",
  ];
  return so.some((p) => existsSync(p)) ? ["-lcrypto"] : [];
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
  const embed = arch === "x64" ? resolveTool("objcopy for binary embedding", ["objcopy", "llvm-objcopy"]) : cc;
  return { cc, cxx, ar, embed };
}

function clangMajorVersion(): string | null {
  const clang = Bun.which("clang");
  if (!clang) {
    return null;
  }
  const p = Bun.spawnSync([clang, "-dumpversion"]);
  if (p.exitCode !== 0) {
    return null;
  }
  return p.stdout.toString().trim().split(".")[0] || null;
}

// Ubuntu's clang package does not include compiler-rt; -fsanitize=address then
// fails at link with missing libclang_rt.asan*.a. g++ uses libasan instead.
function compilerCanLinkAsan(cxx: string): boolean {
  const dir = mkdtempSync(join(tmpdir(), "sumatra-asan-"));
  try {
    const src = join(dir, "probe.cpp");
    const exe = join(dir, "probe");
    writeFileSync(src, "int main(){return 0;}\n");
    const p = Bun.spawnSync([cxx, "-fsanitize=address", src, "-o", exe], {
      stdout: "ignore",
      stderr: "pipe",
    });
    return p.exitCode === 0;
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

function asanInstallHint(): string {
  const major = clangMajorVersion();
  const pkg = major ? `libclang-rt-${major}-dev` : "libclang-rt-dev";
  return `sudo apt install ${pkg}   # clang ASan runtime\n  or: sudo apt install libasan8              # g++ ASan runtime`;
}

function ensureAsanLinker(tools: BuildTools): BuildTools {
  if (compilerCanLinkAsan(tools.cxx)) {
    return tools;
  }
  const gcc = Bun.which("gcc");
  const gxx = Bun.which("g++");
  if (gxx && gcc && compilerCanLinkAsan(gxx)) {
    console.log(`${tools.cxx} cannot link -fsanitize=address (compiler-rt ASan libs are missing).`);
    console.log(`Falling back to ${gxx}. To keep using clang:\n  ${asanInstallHint()}\n`);
    return { ...tools, cc: gcc, cxx: gxx };
  }
  console.error(`${tools.cxx} cannot link -fsanitize=address (compiler-rt ASan libs are missing).`);
  console.error(`Install one of:\n  ${asanInstallHint()}`);
  process.exit(1);
}

// Rebuild objects if the compiler (or asan/debug/release) changed so we don't
// mix clang-asan objects with a g++ link, or vice versa.
function invalidateObjsIfCompilerChanged(outDir: string, tools: BuildTools, config: string): void {
  const stampPath = join(outDir, ".compiler");
  const stamp = `${tools.cc}\n${tools.cxx}\n${config}\n`;
  let same = false;
  if (existsSync(stampPath)) {
    try {
      same = readFileSync(stampPath, "utf8") === stamp;
    } catch {}
  }
  if (!same && existsSync(join(outDir, "obj"))) {
    console.log("Compiler or config changed; rebuilding objects...");
    rmSync(join(outDir, "obj"), { recursive: true, force: true });
  }
  mkdirSync(outDir, { recursive: true });
  writeFileSync(stampPath, stamp);
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
      "ext/a-zlib",
      "ext/a-bzip2",
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
        dir: "ext/a-bzip2",
        patterns: ["bzip2.c"],
      },
      // LzmaDec/Bra* live in base/exe for LzSA (not in libsumatrapdf/libarchive)
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

function makeChmdec(): LibDef {
  return {
    name: "chmdec",
    alwaysOptimize: true,
    defines: ["_stricmp=strcasecmp", "_strnicmp=strncasecmp"],
    includes: [],
    extraCflags: ["-include", "limits.h"],
    files: [{ dir: "ext/chmdec", patterns: ["chm.c"] }],
  };
}

const djvudec: LibDef = {
  name: "djvudec",
  alwaysOptimize: true,
  defines: [],
  includes: [],
  files: [{ dir: "ext/djvudec", patterns: ["djvu.c"] }],
};

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
  if (arch === "x64") {
    lib.defines.push("HAVE_OBJCOPY");
  }
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
    "ext/mupdf/scripts/cmark-gfm",
  );

  const fitz = lib.files.find((g) => g.dir === "ext/mupdf/source/fitz");
  if (fitz) {
    const pats = fitz.patterns as string[];
    const jxrIdx = pats.indexOf("load-jxr-win.c");
    if (jxrIdx >= 0) pats[jxrIdx] = "load-jxr.c";
    for (const extra of ["cull-device.c", "options.c"]) {
      if (!pats.includes(extra)) pats.push(extra);
    }
  }
  const html = lib.files.find((g) => g.dir === "ext/mupdf/source/html");
  if (html && !html.patterns.includes("md.c")) {
    (html.patterns as string[]).push("md.c");
  }
  const pdf = lib.files.find((g) => g.dir === "ext/mupdf/source/pdf");
  if (pdf && !pdf.patterns.includes("pdf-struct.c")) {
    (pdf.patterns as string[]).push("pdf-struct.c");
  }
  const tools = lib.files.find((g) => g.dir === "ext/mupdf/source/tools");
  if (tools && !tools.patterns.includes("muraster.c")) {
    (tools.patterns as string[]).push("muraster.c");
  }
  const pkcs7 = lib.files.find((g) => g.dir === "ext/mupdf/source/helpers/pkcs7");
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
  const text = `/* Generated by cmd/build.ts -linux for dav1d on Linux */
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

async function embedFonts(tools: BuildTools, outDir: string, arch: LinuxArch): Promise<string[]> {
  console.log("Embedding font files into mupdf...");
  const format: EmbedFormat = arch === "x64" ? "elf" : "macho";
  const objs: string[] = [];
  for (const font of FONT_FILES) {
    if (!existsSync(font.path)) {
      console.error(`  WARNING: font not found: ${font.path}`);
      continue;
    }
    const base = basename(font.path, `.${font.ext}`).replace(/-/g, "_");
    const forge = basename(dirname(font.path));
    const sym = arch === "x64" ? `_binary_resources_fonts_${forge}_${base}_${font.ext}` : `_binary_${base}_${font.ext}`;
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
    includes: ["src", "ext/libarchive"],
    files: [
      {
        dir: "src/base",
        patterns: [
          "Base.cpp",
          "Base_posix.cpp",
          "Archive.cpp",
          "Arena.cpp",
          "Arena_posix.cpp",
          "ByteReaderWriter.cpp",
          "CmdLineArgsIter.cpp",
          "Color.cpp",
          "Crypto_posix.cpp",
          "CssParser.cpp",
          "Dict.cpp",
          "DbgHelpDyn_posix.cpp",
          "DirScan.cpp",
          "DirScan_posix.cpp",
          "Exif.cpp",
          "File.cpp",
          "File_posix.cpp",
          "Geom.cpp",
          "GuessFileType.cpp",
          "GuessFileTypeFromFile.cpp",
          "HtmlTags.cpp",
          "JsonParser.cpp",
          "Pixmap.cpp",
          "Pixmap_linux.cpp",
          "SettingsUtil.cpp",
          "SquareTreeParser.cpp",
          "StrQueue.cpp",
          "Thread.cpp",
          "UtAssert.cpp",
          "WinDynCalls_posix.cpp",
          "Str.cpp",
          "StrUtf8.cpp",
          "StrFormatParse.cpp",
          "StrVec.cpp",
          "Strconv.cpp",
          "TgaReader.cpp",
        ],
      },
      {
        dir: "src/gui",
        patterns: ["Dpi_posix.cpp", "Layout.cpp"],
      },
    ],
  },
  aGumbo,
  zlib,
  makeUnrar,
  makeChmdec,
  djvudec,
  "libarchive",
  libwebp,
  makeDav1d,
  heicdec,
  jxldec,
  libjpegTurbo,
  jbig2dec,
  openjpeg,
  freetype,
  lcms2,
  harfbuzz,
  mujs,
  extract,
  brotli,
  cmarkGfm,
  makeMupdf,
] as const;

const TEST_UTIL_SOURCES = [
  "src/Commands.cpp",
  "src/CrashHandlerNoOp.cpp",
  "src/DisplayMode.cpp",
  "src/DocProperties.cpp",
  "src/Flags.cpp",
  "src/FilterUtil.cpp",
  "src/base/FileWatcher_linux.cpp",
  "src/PdfDarkModeImageClassifier_ut.cpp",
  "src/PdfDarkModeImageRules.cpp",
  "src/PdfDarkModeOklab.cpp",
  "src/PdfDarkModeOklab_ut.cpp",
  "src/PageRenderPolicy.cpp",
  "src/gui/CommandPaletteModel.cpp",
  "src/RefHoverDetect.cpp",
  "src/RefHoverTextDetect.cpp",
  "src/SimpleLog_ut.cpp",
  "src/SumatraConfig.cpp",
  "src/SumatraLog_posix.cpp",
  "src/SumatraUnitTests.cpp",
  "src/base/tests/Base_ut.cpp",
  "src/base/tests/ByteReaderWriter_ut.cpp",
  "src/base/tests/Crypto_ut.cpp",
  "src/base/tests/CssParser_ut.cpp",
  "src/base/tests/Dict_ut.cpp",
  "src/base/tests/File_ut.cpp",
  "src/base/tests/GuessFileType_ut.cpp",
  "src/base/tests/JsonParser_ut.cpp",
  "src/base/tests/RefHover_ut.cpp",
  "src/base/tests/SettingsUtil_ut.cpp",
  "src/base/tests/SquareTreeParser_ut.cpp",
  "src/base/tests/StrFormat_ut.cpp",
  "src/base/tests/StrVec_ut.cpp",
  "src/base/tests/Str_ut.cpp",
  "src/base/tests/Vec_ut.cpp",
  "src/tools/test_util.cpp",
];

const PORTABLE_ENGINE_SOURCES = [
  "src/DocProperties.cpp",
  "src/EbookDoc.cpp",
  "src/EngineBase.cpp",
  "src/EngineDjvuDec.cpp",
  "src/EngineImages.cpp",
  "src/EngineMupdf.cpp",
  "src/ImageReader.cpp",
  "src/ImageReader_posix.cpp",
  "src/GumboHtmlParser.cpp",
  "src/GumboHelpers.cpp",
  "src/MobiDoc.cpp",
  "src/PalmDbReader.cpp",
  "src/PdfCadDetect.cpp",
  "src/PdfCadEnhanceDevice.cpp",
  "src/PdfDarkModeNoOp.cpp",
  "src/TreeModel.cpp",
];

const TEST_ENGINES_SOURCES = [
  "src/base/GuessFileType.cpp",
  ...PORTABLE_ENGINE_SOURCES,
  "src/TextSelection.cpp",
  "src/TextSearch.cpp",
  "src/tools/test_engines.cpp",
];

const PORTABLE_COMPILE_SOURCES = [
  // unblocked by LRESULT / HBITMAP / HBRUSH in base/Base.h
  "src/AppUnitTests.cpp",
  "src/FileHistory.cpp",
  "src/PdfDarkModeAnalysis.cpp",
  "src/RefHoverInternal.cpp",
  // Verified to compile on Linux (clang, WSL Ubuntu): no Win32 call, constant
  // or header, directly or through the headers they include.
  "src/GlobalPrefs.cpp",
  "src/MuPDF_Exports.cpp",
  // the dark-mode engine: color and image math over mupdf, no UI
  "src/PdfDarkModeCache.cpp",
  "src/PdfDarkModeDevice.cpp",
  "src/PdfDarkModeEngineCache.cpp",
  "src/PdfDarkModeImageBgBlend.cpp",
  "src/PdfDarkModeImageClassifier.cpp",
  "src/PdfDarkModeImageStats.cpp",
  "src/PdfDarkModeScanProcess.cpp",
  "src/GumboHtmlParser.cpp",
  "src/HtmlFormatter.cpp",
  "src/EbookFormatter.cpp",
  "src/EngineEbook.cpp",
  "src/gui/GuiColors.cpp",
  "src/SvgIcons.cpp",
];

const GTK4_KEYBOARD_HELP_SOURCES = [
  "src/Commands.cpp",
  "src/CrashHandlerNoOp.cpp",
  "src/KeyboardHelp.cpp",
  "src/SumatraLog_posix.cpp",
  "src/gui/GuiColors.cpp",
  "src/gui/PlatformFont.cpp",
  "src/gui/gtk4/GfxGtk.cpp",
  "src/gui/gtk4/PlatformFontGtk.cpp",
  "src/gui/gtk4/PlatformWindowGtk.cpp",
  "src/gui/gtk4/KeyboardHelpGtkMain.cpp",
];

const GTK4_APP_SOURCES = [
  "src/Commands.cpp",
  "src/CrashHandlerNoOp.cpp",
  "src/DisplayMode.cpp",
  "src/FilterUtil.cpp",
  "src/base/FileWatcher_linux.cpp",
  "src/GlobalPrefs.cpp",
  "src/SumatraConfig.cpp",
  "src/DocumentLayout.cpp",
  "src/PageRenderPolicy.cpp",
  "src/PageRenderService.cpp",
  "src/ReaderModel.cpp",
  "src/TextSelection.cpp",
  "src/TextSearch.cpp",
  "src/KeyboardHelp.cpp",
  "src/SumatraLog_posix.cpp",
  ...PORTABLE_ENGINE_SOURCES,
  "src/gui/DocumentView.cpp",
  "src/gui/CommandPaletteModel.cpp",
  "src/gui/GuiColors.cpp",
  "src/gui/PlatformFont.cpp",
  "src/gui/gtk4/GfxGtk.cpp",
  "src/gui/gtk4/PlatformCanvasGtk.cpp",
  "src/gui/gtk4/PlatformFontGtk.cpp",
  "src/gui/gtk4/PlatformWindowGtk.cpp",
  "src/linux/LinuxApp.cpp",
  "src/linux/LinuxCommandPalette.cpp",
  "src/linux/LinuxDesktop.cpp",
  "src/linux/LinuxPrefs.cpp",
  "src/linux/LinuxPrint.cpp",
  "src/linux/LinuxTab.cpp",
  "src/linux/LinuxWindow.cpp",
  "src/linux/SumatraLinux.cpp",
];

export interface LinuxBuildOptions {
  outDir: string;
  isRelease?: boolean;
  asan?: boolean;
  clean?: boolean;
  tools?: Partial<BuildTools>;
  jobs?: number;
}

export async function buildLinux(opts: LinuxBuildOptions): Promise<void> {
  requireLinux();
  const arch = detectLinuxArch();
  let tools: BuildTools = { ...resolveLinuxTools(arch), ...opts.tools };
  const jobs = opts.jobs ?? DEFAULT_JOBS;
  const isRelease = opts.isRelease ?? false;
  const isAsan = opts.asan ?? false;
  const outDir = opts.outDir;
  const generatedDir = join(outDir, "generated", "dav1d");

  if (opts.clean && existsSync(outDir)) {
    console.log(`Cleaning ${outDir}...`);
    rmSync(outDir, { recursive: true, force: true });
  }

  if (isAsan) {
    tools = ensureAsanLinker(tools);
  }

  const startTime = performance.now();
  const config = isAsan ? (isRelease ? "release-asan" : "asan") : isRelease ? "release" : "debug";
  invalidateObjsIfCompilerChanged(outDir, tools, config);
  console.log(`\n=== Building SumatraPDF dependencies (${config}, Linux ${arch}) ===\n`);
  console.log(`Output: ${outDir}`);
  console.log(`Tools: ${tools.cc}, ${tools.cxx}`);
  console.log(`Parallel jobs: ${jobs}\n`);

  mkdirSync(join(outDir, "obj"), { recursive: true });
  mkdirSync(join(outDir, "lib"), { recursive: true });
  await writeDav1dConfig(generatedDir, arch);
  await writeLiblzmaConfig(outDir);

  const commonDefines: string[] = isAsan ? ["ASAN_BUILD"] : [];
  const commonFlags = isAsan ? ["-fsanitize=address", "-fno-omit-frame-pointer"] : [];
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

    dropX86OnlyCflags(lib, arch);

    const extraObjs = lib.name === "mupdf" ? fontObjs : undefined;
    await buildLibrary(lib, outDir, isRelease, {
      tools,
      commonDefines,
      commonFlags,
      cxxFlags,
      jobs,
      extraObjs,
    });
  }

  await buildTestUtil(outDir, isRelease, tools, jobs, commonDefines, commonFlags, cxxFlags);
  await compilePortableSources(outDir, isRelease, tools, jobs, commonDefines, commonFlags, cxxFlags);
  await buildGtk4KeyboardHelp(outDir, isRelease, tools, jobs, commonDefines, commonFlags, cxxFlags);
  await buildGtk4LinuxApp(outDir, isRelease, tools, jobs, commonDefines, commonFlags, cxxFlags);
  stageLinuxDesktopResources(outDir);
  await buildTestEngines(outDir, isRelease, tools, jobs, commonDefines, commonFlags, cxxFlags);
  await buildLinuxPortableArchive(outDir, arch, config);

  const elapsed = ((performance.now() - startTime) / 1000).toFixed(1);
  console.log(`\n=== Dependency build complete (${config}) in ${elapsed}s ===`);
  console.log(`Static libraries: ${join(outDir, "lib")}\n`);
}

async function buildLinuxPortableArchive(outDir: string, arch: LinuxArch, config: string): Promise<void> {
  const version = extractSumatraVersion();
  const configSuffix = config === "release" ? "" : `-${config}`;
  const packageName = `SumatraPDF-${version}-linux-${arch}${configSuffix}`;
  const tempRoot = mkdtempSync(join(tmpdir(), "sumatrapdf-linux-package-"));
  const packageDir = join(tempRoot, packageName);
  const archivePath = join(outDir, `${packageName}.tar.gz`);
  rmSync(archivePath, { force: true });

  const files = [
    [join(outDir, "SumatraPDF"), join(packageDir, "bin", "SumatraPDF")],
    ["COPYING", join(packageDir, "COPYING")],
    ["packaging/linux/README.md", join(packageDir, "README.md")],
    [
      join(outDir, "share", "applications", `${LINUX_APP_ID}.desktop`),
      join(packageDir, "share", "applications", `${LINUX_APP_ID}.desktop`),
    ],
    [
      join(outDir, "share", "metainfo", `${LINUX_APP_ID}.metainfo.xml`),
      join(packageDir, "share", "metainfo", `${LINUX_APP_ID}.metainfo.xml`),
    ],
    [
      join(outDir, "share", "icons", "hicolor", "scalable", "apps", `${LINUX_APP_ID}.svg`),
      join(packageDir, "share", "icons", "hicolor", "scalable", "apps", `${LINUX_APP_ID}.svg`),
    ],
  ];
  console.log("Building portable Linux archive...");
  try {
    for (const [src, dst] of files) {
      mkdirSync(dirname(dst), { recursive: true });
      copyFileSync(src, dst);
      chmodSync(dst, dst.endsWith("/bin/SumatraPDF") ? 0o755 : 0o644);
    }

    const tar = resolveTool("tar archiver", ["tar"]);
    const result = await spawnCmd([
      tar,
      "--sort=name",
      "--mtime=@0",
      "--owner=0",
      "--group=0",
      "--numeric-owner",
      "-czf",
      archivePath,
      "-C",
      tempRoot,
      packageName,
    ]);
    if (!result.ok) {
      throw new Error(`create portable Linux archive failed: ${result.stderr}`);
    }
  } finally {
    rmSync(tempRoot, { recursive: true, force: true });
  }
  console.log(`  -> ${archivePath}`);
}

function requireResourceText(path: string, text: string, expected: string): void {
  if (!text.includes(expected)) {
    throw new Error(`${path} is missing required metadata: ${expected}`);
  }
}

function stageLinuxDesktopResources(outDir: string): void {
  const desktop = readFileSync(LINUX_DESKTOP_FILE, "utf8");
  const metainfo = readFileSync(LINUX_METAINFO_FILE, "utf8");
  requireResourceText(LINUX_DESKTOP_FILE, desktop, "[Desktop Entry]");
  requireResourceText(LINUX_DESKTOP_FILE, desktop, "Type=Application");
  requireResourceText(LINUX_DESKTOP_FILE, desktop, "Exec=SumatraPDF %U");
  requireResourceText(LINUX_DESKTOP_FILE, desktop, `Icon=${LINUX_APP_ID}`);
  requireResourceText(LINUX_DESKTOP_FILE, desktop, "MimeType=application/pdf;");
  requireResourceText(LINUX_METAINFO_FILE, metainfo, `<id>${LINUX_APP_ID}</id>`);
  requireResourceText(
    LINUX_METAINFO_FILE,
    metainfo,
    `<launchable type="desktop-id">${LINUX_APP_ID}.desktop</launchable>`,
  );
  requireResourceText(LINUX_METAINFO_FILE, metainfo, `<icon type="stock">${LINUX_APP_ID}</icon>`);

  const files = [
    [LINUX_DESKTOP_FILE, join(outDir, "share", "applications", `${LINUX_APP_ID}.desktop`)],
    [LINUX_METAINFO_FILE, join(outDir, "share", "metainfo", `${LINUX_APP_ID}.metainfo.xml`)],
    [LINUX_ICON_FILE, join(outDir, "share", "icons", "hicolor", "scalable", "apps", `${LINUX_APP_ID}.svg`)],
  ];
  console.log("Staging Linux desktop resources...");
  for (const [src, dst] of files) {
    mkdirSync(dirname(dst), { recursive: true });
    copyFileSync(src, dst);
    console.log(`  -> ${dst}`);
  }
}

async function buildGtk4LinuxApp(
  outDir: string,
  isRelease: boolean,
  tools: BuildTools,
  jobs: number,
  commonDefines: string[],
  commonFlags: string[],
  cxxFlags: string[],
): Promise<void> {
  console.log("Building GTK 4 SumatraPDF application...");
  const optFlags = isRelease ? ["-Os"] : ["-O0", "-g"];
  const configDefines = isRelease ? ["NDEBUG"] : ["DEBUG"];
  const defineFlags = [...commonDefines, ...configDefines].map((d) => `-D${d}`);
  const gtkCflags = [...pkgConfigFlags("--cflags", "gtk4"), ...pkgConfigFlags("--cflags", "gio-unix-2.0")];
  const gtkLibs = pkgConfigFlags("--libs", "gtk4");
  const includeFlags = ["-Isrc", "-Iext/djvudec", "-Iext/mupdf/include", "-Iext/mupdf/generated"];
  const units = GTK4_APP_SOURCES.map((src) => {
    const obj = objPath(outDir, "gtk4-app", src);
    const sourceDefines = src === "src/Commands.cpp" ? ["-DSUMATRA_TEST_UTIL=1"] : [];
    return {
      src,
      obj,
      args: [
        tools.cxx,
        ...optFlags,
        ...defineFlags,
        ...sourceDefines,
        ...includeFlags,
        ...gtkCflags,
        ...commonFlags,
        "-w",
        "-std=c++23",
        ...cxxFlags,
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-rtti",
        "-fno-exceptions",
        "-c",
        src,
        "-o",
        obj,
      ],
    };
  });
  await compileAll(units, jobs);

  const exePath = join(outDir, "SumatraPDF");
  const result = await spawnCmd([
    tools.cxx,
    "-o",
    exePath,
    ...commonFlags,
    "-Wl,--gc-sections",
    ...units.map((unit) => unit.obj),
    ...portableEngineLinkArgs(outDir),
    ...gtkLibs,
  ]);
  if (!result.ok) {
    throw new Error(`link SumatraPDF failed: ${result.stderr}`);
  }
  console.log(`  -> ${exePath}`);
}

function pkgConfigFlags(kind: "--cflags" | "--libs", pkg: string): string[] {
  const pkgConfig = resolveTool("pkg-config", ["pkg-config", "pkgconf"]);
  const result = Bun.spawnSync([pkgConfig, kind, pkg]);
  if (result.exitCode !== 0) {
    throw new Error(`${pkgConfig} ${kind} ${pkg} failed: ${result.stderr.toString()}`);
  }
  return result.stdout.toString().trim().split(/\s+/).filter(Boolean);
}

function portableEngineLinkArgs(outDir: string): string[] {
  return [
    "-Wl,--start-group",
    join(outDir, "lib", "libbase.a"),
    join(outDir, "lib", "libmupdf.a"),
    join(outDir, "lib", "liba-gumbo.a"),
    join(outDir, "lib", "libcmark-gfm.a"),
    join(outDir, "lib", "liba-mujs.a"),
    join(outDir, "lib", "liba-extract.a"),
    join(outDir, "lib", "libharfbuzz.a"),
    join(outDir, "lib", "libfreetype.a"),
    join(outDir, "lib", "libbrotli.a"),
    join(outDir, "lib", "liblcms2.a"),
    join(outDir, "lib", "liba-openjpeg.a"),
    join(outDir, "lib", "liba-jbig2dec.a"),
    join(outDir, "lib", "liblibjpeg-turbo.a"),
    join(outDir, "lib", "libdjvudec.a"),
    join(outDir, "lib", "liblibarchive.a"),
    join(outDir, "lib", "liba-zlib.a"),
    "-Wl,--end-group",
    ...libCryptoLinkFlags(),
  ];
}

async function buildGtk4KeyboardHelp(
  outDir: string,
  isRelease: boolean,
  tools: BuildTools,
  jobs: number,
  commonDefines: string[],
  commonFlags: string[],
  cxxFlags: string[],
): Promise<void> {
  console.log("Building GTK 4 keyboard-help viewer...");
  const optFlags = isRelease ? ["-Os"] : ["-O0", "-g"];
  const configDefines = isRelease ? ["NDEBUG"] : ["DEBUG"];
  const defineFlags = [...commonDefines, ...configDefines, "SUMATRA_TEST_UTIL=1"].map((d) => `-D${d}`);
  const gtkCflags = pkgConfigFlags("--cflags", "gtk4");
  const gtkLibs = pkgConfigFlags("--libs", "gtk4");
  const units = GTK4_KEYBOARD_HELP_SOURCES.map((src) => {
    const obj = objPath(outDir, "gtk4-keyboard-help", src);
    return {
      src,
      obj,
      args: [
        tools.cxx,
        ...optFlags,
        ...defineFlags,
        "-Isrc",
        ...gtkCflags,
        ...commonFlags,
        "-w",
        "-std=c++23",
        ...cxxFlags,
        "-ffunction-sections",
        "-fdata-sections",
        "-fno-rtti",
        "-fno-exceptions",
        "-c",
        src,
        "-o",
        obj,
      ],
    };
  });
  await compileAll(units, jobs);

  const exePath = join(outDir, "keyboard_help_gtk4");
  const result = await spawnCmd([
    tools.cxx,
    "-o",
    exePath,
    ...commonFlags,
    "-Wl,--gc-sections",
    ...units.map((unit) => unit.obj),
    join(outDir, "lib", "libbase.a"),
    ...gtkLibs,
  ]);
  if (!result.ok) {
    throw new Error(`link keyboard_help_gtk4 failed: ${result.stderr}`);
  }
  console.log(`  -> ${exePath}`);
}

async function compilePortableSources(
  outDir: string,
  isRelease: boolean,
  tools: BuildTools,
  jobs: number,
  commonDefines: string[],
  commonFlags: string[],
  cxxFlags: string[],
): Promise<void> {
  console.log("Compiling portable source checks...");
  const optFlags = isRelease ? ["-Os"] : ["-O0", "-g"];
  const configDefines = isRelease ? ["NDEBUG"] : ["DEBUG"];
  const defineFlags = [...commonDefines, ...configDefines].map((d) => `-D${d}`);
  const includeFlags = ["-Isrc", "-Iext/mupdf/include", "-Iext/mupdf/generated"];

  const units = PORTABLE_COMPILE_SOURCES.map((src) => {
    const obj = objPath(outDir, "portable", src);
    return {
      src,
      obj,
      args: [
        tools.cxx,
        ...optFlags,
        ...defineFlags,
        ...includeFlags,
        ...commonFlags,
        "-w",
        "-std=c++23",
        ...cxxFlags,
        "-fno-rtti",
        "-fno-exceptions",
        "-c",
        src,
        "-o",
        obj,
      ],
    };
  });

  await compileAll(units, jobs);
}

async function buildTestUtil(
  outDir: string,
  isRelease: boolean,
  tools: BuildTools,
  jobs: number,
  commonDefines: string[],
  commonFlags: string[],
  cxxFlags: string[],
): Promise<void> {
  console.log("Building test_util...");
  const optFlags = isRelease ? ["-Os"] : ["-O0", "-g"];
  const configDefines = isRelease ? ["NDEBUG"] : ["DEBUG"];
  const defineFlags = [...commonDefines, ...configDefines, "SUMATRA_TEST_UTIL=1"].map((d) => `-D${d}`);
  const includeFlags = ["-Isrc"];

  const units = TEST_UTIL_SOURCES.map((src) => {
    const obj = objPath(outDir, "test_util", src);
    return {
      src,
      obj,
      args: [
        tools.cxx,
        ...optFlags,
        ...defineFlags,
        ...includeFlags,
        ...commonFlags,
        "-w",
        "-std=c++23",
        ...cxxFlags,
        "-fno-rtti",
        "-fno-exceptions",
        "-c",
        src,
        "-o",
        obj,
      ],
    };
  });

  await compileAll(units, jobs);

  const exePath = join(outDir, "test_util");
  const linkArgs = [
    tools.cxx,
    "-o",
    exePath,
    ...commonFlags,
    ...units.map((u) => u.obj),
    join(outDir, "lib", "libbase.a"),
  ];
  const res = await spawnCmd(linkArgs);
  if (!res.ok) {
    throw new Error(`link test_util failed: ${res.stderr}`);
  }
  console.log(`  -> ${exePath}`);

  const env = commonFlags.includes("-fsanitize=address")
    ? {
        ...process.env,
        ASAN_OPTIONS: "abort_on_error=1:halt_on_error=1:detect_leaks=0",
      }
    : process.env;
  const run = Bun.spawn([exePath, "-for-ai"], {
    env,
    stdout: "inherit",
    stderr: "inherit",
  });
  const code = await run.exited;
  if (code !== 0) {
    throw new Error(`test_util failed with exit code ${code}`);
  }
}

async function buildTestEngines(
  outDir: string,
  isRelease: boolean,
  tools: BuildTools,
  jobs: number,
  commonDefines: string[],
  commonFlags: string[],
  cxxFlags: string[],
): Promise<void> {
  console.log("Building test_engines...");
  const optFlags = isRelease ? ["-Os"] : ["-O0", "-g"];
  const configDefines = isRelease ? ["NDEBUG"] : ["DEBUG"];
  const defineFlags = [...commonDefines, ...configDefines].map((d) => `-D${d}`);
  const includeFlags = ["-Isrc", "-Iext/djvudec", "-Iext/mupdf/include", "-Iext/mupdf/generated"];

  const units = TEST_ENGINES_SOURCES.map((src) => {
    const obj = objPath(outDir, "test_engines", src);
    return {
      src,
      obj,
      args: [
        tools.cxx,
        ...optFlags,
        ...defineFlags,
        ...includeFlags,
        ...commonFlags,
        "-w",
        "-std=c++23",
        ...cxxFlags,
        "-fno-rtti",
        "-fno-exceptions",
        "-c",
        src,
        "-o",
        obj,
      ],
    };
  });

  await compileAll(units, jobs);

  const exePath = join(outDir, "test_engines");
  // --start-group: harfbuzz's custom allocator symbols live in mupdf's
  // mupdf_load_system_font.c, which follows harfbuzz in the archive list.
  // Archive names must match LibDef.name (libmupdf.a, liba-zlib.a, …).
  const linkArgs = [
    tools.cxx,
    "-o",
    exePath,
    ...commonFlags,
    ...units.map((u) => u.obj),
    ...portableEngineLinkArgs(outDir),
  ];
  const res = await spawnCmd(linkArgs);
  if (!res.ok) {
    throw new Error(`link test_engines failed: ${res.stderr}`);
  }
  console.log(`  -> ${exePath}`);
}
