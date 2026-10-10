// Every buildable target: the app, console tools and one static library per
// third-party dependency. cmd/ng-build.ts resolves names against this list.
//
// A source entry is a glob relative to the repo root. A target compiles on a
// platform when `platforms` is absent or includes it; a source file compiles
// on a platform when its name suffix agrees (_win.cpp, _posix.cpp, _linux.cpp,
// _mac.cpp, _wasm.cpp) or it has no suffix.

import type { Platform } from "./ng-toolchain";

export type TargetKind = "app" | "console" | "staticlib";

// A native build targets the host: `-mac` only runs on a mac, `-linux`
// natively or inside WSL. Third-party libraries with x86 intrinsics or an
// architecture-specific config header ask for this.
export const hostArm64 = process.arch === "arm64";

/** NASM-assembled sources. Only used where the platform has an assembler. */
export type AsmSources = {
  /** globs relative to the repo root */
  sources: string[];
  /** globs removed from the sources (files that are only %included) */
  exclude?: string[];
  includes?: string[];
  defines?: string[];
  /** platforms that assemble these; all when absent */
  platforms?: Platform[];
  /** assembled only for a -profile build */
  profileOnly?: boolean;
};

/**
 * A config header a library expects the build system to produce. Written to
 * out/<cfg>/generated/<target>/<name> before the target compiles; that
 * directory goes first on the include path, so it wins over a checked-in
 * header of the same name in ext/.
 */
export type GeneratedHeader = {
  /** file name, e.g. "config.h" */
  name: string;
  /** copied from this file (relative to the repo root) */
  from?: string;
  /** or written verbatim */
  content?: string;
  /** platforms that need it; all when absent */
  platforms?: Platform[];
};

export type Target = {
  name: string;
  kind: TargetKind;
  /** globs relative to the repo root */
  sources: string[];
  /** globs removed from the sources; for files a glob picks up but we don't build yet */
  exclude?: string[];
  /** include dirs relative to the repo root */
  includes?: string[];
  defines?: string[];
  /** defines added only in debug / only in release builds */
  debugDefines?: string[];
  releaseDefines?: string[];
  /** also gets the build's version as defines (cmd/helper/ng-version.ts) */
  versionDefines?: boolean;
  /** static library targets this one links (apps and console tools only) */
  deps?: string[];
  /** platforms this target builds on; all when absent */
  platforms?: Platform[];
  /** Windows import libraries */
  winLibs?: string[];
  /** native libraries needed only by the archive packer */
  systemDeps?: "archive";
  /** true: our code, /W4 /WX; false: third-party, warnings relaxed */
  strict: boolean;
  /** -profile: compile with /callcap so orig's PerfLog sees every function */
  callcap?: boolean;
  /** compile C++ with exceptions on (orig SumatraPDF default); gpui and mupdf are off */
  exceptions?: boolean;
  /** third-party libs orig compiles optimized even in debug builds */
  alwaysOptimize?: boolean;
  /** per-source overrides matched by glob; `platforms` limits where they apply */
  perSource?: { glob: string; flags: string[]; msvcFlags?: string[]; platforms?: Platform[] }[];
  asm?: AsmSources;
  /** Windows resource script (icons, version, manifest), compiled with rc.exe */
  rc?: string;
  /** link the LzSA archive into executables using this target */
  embedded?: boolean;
  /** config headers written before compiling; see GeneratedHeader */
  generated?: GeneratedHeader[];
  /** files baked into the wasm build's MEMFS: `from` is a glob, `to` a directory in it */
  wasmPreload?: { from: string; to: string }[];
  /** sources, excludes, defines and includes added only on these platforms */
  perPlatform?: Partial<Record<Platform, PlatformExtra>>;
};

export type PlatformExtra = {
  sources?: string[];
  exclude?: string[];
  defines?: string[];
  includes?: string[];
};

/**
 * `t`, with its `perPlatform` entry for `plat` merged in. Platform include
 * dirs go first: they are the ones that shadow a header (dav1d's stdatomic.h
 * shim for cl.exe).
 */
export function forPlatform(t: Target, plat: Platform): Target {
  const extra = t.perPlatform?.[plat];
  if (!extra) return t;
  return {
    ...t,
    sources: [...t.sources, ...(extra.sources ?? [])],
    exclude: [...(t.exclude ?? []), ...(extra.exclude ?? [])],
    defines: [...(t.defines ?? []), ...(extra.defines ?? [])],
    includes: [...(extra.includes ?? []), ...(t.includes ?? [])],
  };
}

const gpuiWinLibs = [
  "d2d1.lib",
  "d3d11.lib",
  "dxgi.lib",
  "dwrite.lib",
  "dwmapi.lib",
  "psapi.lib",
  "ole32.lib",
  "uiautomationcore.lib",
  "windowscodecs.lib",
  "user32.lib",
  "imm32.lib",
  "gdi32.lib",
  "gdiplus.lib",
  "shlwapi.lib",
  "uxtheme.lib",
  "comctl32.lib",
  "oleaut32.lib",
  "shell32.lib",
  "winhttp.lib",
  "advapi32.lib",
  "pdh.lib",
  // base/Win.cpp
  "winspool.lib",
  "wininet.lib",
  "comdlg32.lib",
  "crypt32.lib",
  "urlmon.lib",
  "version.lib",
  "wintrust.lib",
  "msimg32.lib",
];

// the MEMFS directories src/gui/WasmBridge.h names
const kWasmDocsDir = "/docs";

const noSimd: PlatformExtra = {
  defines: ["WITHOUT_SIMD"],
  exclude: ["ext/libjpeg-turbo/simd/**"],
};

// liblzma is compiled into a-libarchive (orig does the same). Only the
// decoder half: libarchive never compresses.
const liblzmaSources = [
  "ext/liblzma/common/alone_decoder.c",
  "ext/liblzma/common/auto_decoder.c",
  "ext/liblzma/common/block_decoder.c",
  "ext/liblzma/common/block_header_decoder.c",
  "ext/liblzma/common/block_util.c",
  "ext/liblzma/common/common.c",
  "ext/liblzma/common/filter_common.c",
  "ext/liblzma/common/filter_decoder.c",
  "ext/liblzma/common/filter_flags_decoder.c",
  "ext/liblzma/common/index.c",
  "ext/liblzma/common/index_decoder.c",
  "ext/liblzma/common/index_hash.c",
  "ext/liblzma/common/stream_decoder.c",
  "ext/liblzma/common/stream_flags_common.c",
  "ext/liblzma/common/stream_flags_decoder.c",
  "ext/liblzma/common/vli_decoder.c",
  "ext/liblzma/common/vli_size.c",
  "ext/liblzma/check/check.c",
  "ext/liblzma/check/crc32_fast.c",
  "ext/liblzma/check/crc64_fast.c",
  "ext/liblzma/lz/lz_decoder.c",
  "ext/liblzma/lzma/lzma_decoder.c",
  "ext/liblzma/lzma/lzma2_decoder.c",
  "ext/liblzma/rangecoder/price_table.c",
  "ext/liblzma/delta/delta_common.c",
  "ext/liblzma/delta/delta_decoder.c",
  "ext/liblzma/simple/simple_coder.c",
  "ext/liblzma/simple/simple_decoder.c",
  "ext/liblzma/simple/x86.c",
];

const liblzmaIncludes = [
  "ext/liblzma/api",
  "ext/liblzma/common",
  "ext/liblzma/check",
  "ext/liblzma/delta",
  "ext/liblzma/lz",
  "ext/liblzma/lzma",
  "ext/liblzma/rangecoder",
  "ext/liblzma/simple",
  "ext/liblzma",
];

const linuxPlatformConfig: PlatformExtra = { defines: ['PLATFORM_CONFIG_H="config_posix.h"'] };

const hbNoVisibility: PlatformExtra = { defines: ["HB_NO_VISIBILITY=1"] };

// What configure would write for libarchive on a POSIX system, plus the two
// typedefs archive_platform.h only reaches through its `#if _WIN32` branch
// (ext/a-libarchive/libarchive.c is amalgamated from a Windows build). No
// Linux uses OpenSSL for encrypted ZIP entries. macOS uses CommonCrypto;
// wasm has no crypto backend.
const libarchivePosixConfig = `/* Generated by cmd/ng-build.ts for libarchive off Windows */
#pragma once
#define __LIBARCHIVE_CONFIG_H_INCLUDED 1

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define ARCHIVE_PLATFORM_STAT_H_INCLUDED
typedef off_t la_seek_t;
typedef struct stat la_seek_stat_t;
#define la_seek_fstat(fd, st) fstat((fd), (st))
#define la_seek_stat(fd, st) stat((fd), (st))

#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_FCNTL 1
#define HAVE_LIMITS_H 1
#define HAVE_FCHDIR 1
#define HAVE_DIRFD 1
#define HAVE_READLINK 1
#define HAVE_LSTAT 1
#ifdef __APPLE__
/* mac spells the sub-second fields st_*timespec and has a creation time */
#define HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC 1
#define HAVE_STRUCT_STAT_ST_BIRTHTIME 1
#define HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC 1
#else
#define HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC 1
#endif
#define HAVE_LOCALE_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_TIME_H 1
#define HAVE_UNISTD_H 1
#define HAVE_WCHAR_H 1
#define HAVE_WCTYPE_H 1
#define HAVE_DIRENT_H 1
#define HAVE_DLFCN_H 1
#define HAVE_PWD_H 1
#define HAVE_GRP_H 1
#define HAVE_POLL_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_WAIT_H 1

#ifdef __linux__
#define HAVE_LIBCRYPTO 1
#define HAVE_PKCS5_PBKDF2_HMAC_SHA1 1
#endif

#define HAVE_FSTAT 1
#define HAVE_STAT 1
#define HAVE_MEMSET 1
#define HAVE_MEMMOVE 1
#define HAVE_SETLOCALE 1
#define HAVE_STRCHR 1
#define HAVE_STRDUP 1
#define HAVE_STRERROR 1
#define HAVE_STRFTIME 1
#define HAVE_STRNLEN 1
#define HAVE_STRRCHR 1
#define HAVE_UTIME 1
#define HAVE_VPRINTF 1
#define HAVE_MBRTOWC 1
#define HAVE_WCRTOMB 1
#define HAVE_WCSCMP 1
#define HAVE_WCSCPY 1
#define HAVE_WCSLEN 1
#define HAVE_WCTOMB 1
#define HAVE_WMEMCMP 1
#define HAVE_WMEMCPY 1
#define HAVE_WMEMMOVE 1
#define HAVE_FTRUNCATE 1
#define HAVE_CHOWN 1
#define HAVE_PIPE 1
#define HAVE_SYMLINK 1
#define HAVE_LINK 1
#define HAVE_OPENAT 1
#define HAVE_FUTIMENS 1
#define HAVE_UTIMENSAT 1

#define HAVE_INTTYPES_H 1
#define HAVE_INTMAX_T 1
#define HAVE_UINTMAX_T 1
#define HAVE_LONG_LONG_INT 1
#define HAVE_UNSIGNED_LONG_LONG 1
#define HAVE_UNSIGNED_LONG_LONG_INT 1
#define HAVE_WCHAR_T 1
#define HAVE_SSIZE_T 1

#define HAVE_DECL_INT32_MAX 1
#define HAVE_DECL_INT32_MIN 1
#define HAVE_DECL_INT64_MAX 1
#define HAVE_DECL_INT64_MIN 1
#define HAVE_DECL_INTMAX_MAX 1
#define HAVE_DECL_INTMAX_MIN 1
#define HAVE_DECL_SIZE_MAX 1
#define HAVE_DECL_SSIZE_MAX 1
#define HAVE_DECL_UINT32_MAX 1
#define HAVE_DECL_UINT64_MAX 1
#define HAVE_DECL_UINTMAX_MAX 1

#define SIZEOF_WCHAR_T 4
#define HAVE_EILSEQ 1
#define ICONV_CONST

#ifndef __EMSCRIPTEN__
#define HAVE_ICONV 1
#define HAVE_ICONV_H 1
#define HAVE_ARC4RANDOM_BUF 1
#define HAVE_CHROOT 1
#define HAVE_FORK 1
#define HAVE_FUTIMES 1
#endif

/* bundled codecs */
#define HAVE_LIBZ 1
#define HAVE_ZLIB_H 1
#define HAVE_BZLIB_H 1
#define HAVE_LZMA_H 1
#define HAVE_LIBLZMA 1
`;

// unrar's own Archive class collides with src/base/Archive.cpp's Archive
// under GNU ld (MSVC picks one COMDAT). Consumers only use the C API in
// dll.hpp, which never names the class, so a TU-local rename is safe.
const unrarPosix: PlatformExtra = {
  defines: ["Archive=UnrarArchive"],
};

// JPEG-XR: WIC on Windows (load-jxr-win.c), mupdf's own loader elsewhere
// (load-jxr.c, a stub without HAVE_JPEGXR). pkcs7-windows.c is wincrypt.
// pkcs7_stub_posix.c satisfies the calls left in murun.c and pdfsign.c.
// unlibarchive.c reaches for a system <archive.h> off Windows; point it at
// the vendored one instead.
const mupdfPosix: PlatformExtra = {
  exclude: ["ext/mupdf/source/fitz/load-jxr-win.c", "src/mupdf/pkcs7-windows.c"],
  includes: ["ext/a-libarchive/libarchive"],
};

const mupdfLinux: PlatformExtra = {
  ...mupdfPosix,
  sources: ["ext/mupdf/source/helpers/pkcs7/pkcs7-openssl.c"],
  defines: ["HAVE_LIBCRYPTO=1"],
};

// The x86-64 asm dav1d builds. Everything else under src/x86 is either
// AVX-512 (orig leaves it out) or %included by these.
const dav1dAsm = [
  "cdef16_avx2",
  "cdef_avx2",
  "cpuid",
  "filmgrain16_avx2",
  "filmgrain_avx2",
  "ipred16_avx2",
  "ipred_avx2",
  "itx16_avx2",
  "itx_avx2",
  "loopfilter16_avx2",
  "loopfilter_avx2",
  "looprestoration16_avx2",
  "looprestoration_avx2",
  "mc16_avx2",
  "mc_avx2",
  // mc_sse.asm: provides mc_warp_filter2 (and related RODATA) used by mc_avx2
  "mc_sse",
  "msac",
  "pal",
  "refmvs",
].map((n) => `ext/dav1d/src/x86/${n}.asm`);

// What meson would write into the build dir. The checked-in
// ext/dav1d/include/config.h is the Windows x86-64 one (HAVE_IO_H, fseeko,
// ...). HAVE_ASM is 0 off Windows: only the x86-64 NASM asm is built here, so
// every other target runs dav1d's portable C.
function dav1dConfig(aarch64: boolean): string {
  const x86 = aarch64 ? 0 : 1;
  return `/* Generated by cmd/ng-build.ts for dav1d off Windows */
#pragma once
#define ARCH_AARCH64 ${aarch64 ? 1 : 0}
#define ARCH_ARM 0
#define ARCH_PPC64LE 0
#define ARCH_X86 ${x86}
#define ARCH_X86_32 0
#define ARCH_X86_64 ${x86}
#define CONFIG_16BPC 1
#define CONFIG_8BPC 1
#define CONFIG_LOG 1
#define ENDIANNESS_BIG 0
#define HAVE_ASM 0
#define HAVE_CLOCK_GETTIME 1
#define HAVE_DLSYM 1
#define HAVE_POSIX_MEMALIGN 1
#define STACK_ALIGNMENT 16
`;
}

// dav1d's cpu-feature detection is one file per architecture and neither
// compiles on the other (x86/cpu.c is cpuid, arm/cpu.c is sysctlbyname on
// mac). With HAVE_ASM=0 nothing calls it, but it still has to compile.
const dav1dArmCpu: PlatformExtra = {
  sources: ["ext/dav1d/src/arm/cpu.c"],
  exclude: ["ext/dav1d/src/x86/cpu.c"],
};

// orig premake5.files.lua engines_files() plus the ebook / document-model
// sources it needs. Listed one by one because src/ will hold the UI too.
const enginesSources = [
  "src/gui/UIModels.cpp",
  "src/ng/gui/PlatformFont.cpp",
  "src/ng/gui/PlatformFont_posix.cpp",
  "src/ng/gui/PlatformText.cpp",
  "src/ng/Annotation.cpp",
  "src/AnnotSearch.cpp",
  "src/PdfDate.cpp",
  "src/ng/AvifReader.cpp",
  "src/ng/CachedObjects.cpp",
  "src/ChapterTable.cpp",
  "src/ng/ChmDump.cpp",
  "src/ng/ChmFile.cpp",
  "src/DocProperties.cpp",
  "src/ng/EbookDoc.cpp",
  "src/EbookFormatter.cpp",
  "src/ng/EngineBase.cpp",
  "src/ng/EngineCreate.cpp",
  "src/ng/EngineDjvuDec.cpp",
  "src/ng/EngineDvi.cpp",
  "src/ng/ExifDump.cpp",
  "src/ng/EngineEbook.cpp",
  "src/ng/EngineImages.cpp",
  "src/ng/EngineMupdf.cpp",
  "src/ng/EnginePs.cpp",
  "src/ng/EutlTrust.cpp",
  "src/FilterUtil.cpp",
  "src/GumboHtmlParser.cpp",
  "src/ng/HtmlFormatter.cpp",
  "src/ng/ImageReader.cpp",
  "src/ng/JxlReader.cpp",
  "src/ng/LitDoc.cpp",
  "src/ng/MobiDoc.cpp",
  "src/PalmDbReader.cpp",
  "src/ng/PdfCad.cpp",
  "src/ng/PdfCreator.cpp",
  "src/PdfDarkModeColor.cpp",
  "src/PdfDarkModeImageStats.cpp",
  "src/PdfDarkModeProfile.cpp",
  "src/ng/PdfSign.cpp",
  "src/ng/PdfSync.cpp",
  "src/ng/PngOptimizer.cpp",
  "src/TextSearch.cpp",
  "src/ng/TextSelection.cpp",
  "src/ng/WebpReader.cpp",
];

// orig's sumatrapdf_files() minus everything that needs a window. Listed one by
// one because src/ holds the UI too.
const appSources = [
  "src/ng/Accelerators.cpp",
  "src/ng/AIChatProcess.cpp",
  "src/ng/AppSettings.cpp",
  "src/ng/AppTools.cpp",
  "src/ng/AppTools_posix.cpp",
  "src/ng/ChmModel.cpp",
  "src/ng/CommandAvailability.cpp",
  "src/ng/Commands.cpp",
  "src/ng/DisplayMode.cpp",
  "src/ng/DisplayModel.cpp",
  "src/DocController.cpp",
  "src/ng/DocumentLayout.cpp",
  "src/ng/EmbeddedResources.cpp",
  "src/ng/FileHistory.cpp",
  "src/ng/FileThumbnails.cpp",
  "src/ng/Flags.cpp",
  "src/ng/MarkdownModel.cpp",
  "src/ng/MarkdownToc.cpp",
  "src/ng/NavFilesInFolder.cpp",
  "src/ng/PagePosition.cpp",
  "src/ng/RefHoverDetect.cpp",
  "src/RefHoverInternal.cpp",
  "src/RefHoverText.cpp",
  "src/ng/RefHoverTextDetect.cpp",
  "src/ng/RenderCache.cpp",
  "src/ng/ShortcutParse.cpp",
  "src/ng/SystemFonts.cpp",
  "src/ng/Theme.cpp",
  "src/ng/TranslationLangs.cpp",
  "src/ng/Translations.cpp",
  // the browser's file picker, downloads and OPFS write-back (wasm only,
  // and no gpui in it, so it belongs here rather than in the app target)
  "src/ng/gui/WasmBridge_wasm.cpp",
];

const archiveWinLibs = [
  "advapi32.lib",
  "comctl32.lib",
  "crypt32.lib",
  "gdi32.lib",
  "gdiplus.lib",
  "ole32.lib",
  "oleaut32.lib",
  "shell32.lib",
  "shlwapi.lib",
  "user32.lib",
  "version.lib",
  "wininet.lib",
  "winspool.lib",
  "wintrust.lib",
];

export const targets: Target[] = [
  {
    name: "test_embedded",
    kind: "console",
    sources: [
      "src/ng/tools/test_embedded.cpp",
      "src/ng/EmbeddedResources.cpp",
      "src/base/LogNoOp.cpp",
      "src/shared/CrashHandlerNoOp.cpp",
    ],
    includes: ["src/ng"],
    embedded: true,
    deps: ["base"],
    winLibs: archiveWinLibs,
    strict: true,
    exceptions: true,
  },
  {
    name: "MakeLZSA",
    kind: "console",
    sources: [
      "src/shared/tools/MakeLzSA.cpp",
      "src/shared/CrashHandlerNoOp.cpp",
      "src/base/LogNoOp.cpp",
      "ext/lzma/C/LzFind.c",
      "ext/lzma/C/LzmaEnc.c",
    ],
    includes: ["src/ng", "ext/lzma/C", "ext/a-zlib"],
    defines: ["_7ZIP_ST"],
    deps: ["base"],
    winLibs: archiveWinLibs,
    systemDeps: "archive",
    strict: false,
    exceptions: true,
  },
  {
    name: "gpui",
    kind: "staticlib",
    sources: ["ext/gpui/gpui.cpp", "ext/gpui/quickjs/quickjs.c"],
    includes: ["ext/gpui"],
    // gpui.cpp compiles without exceptions and RTTI; the platform half it
    // selects comes from GPUI_OS_* which it derives from the compiler macros.
    // js_*: quickjs and mupdf's mujs export the same four allocator names, so
    // linking both is a duplicate-symbol error. Nothing outside this target
    // includes quickjs.h, so renaming them here is contained
    defines: [
      "WIN_BACKEND_DIRECT2D=1",
      "js_malloc=qjs_malloc",
      "js_free=qjs_free",
      "js_realloc=qjs_realloc",
      "js_strdup=qjs_strdup",
    ],
    strict: false,
    exceptions: false,
    perSource: [
      { glob: "ext/gpui/gpui.cpp", flags: [], msvcFlags: ["/bigobj"] },
      {
        glob: "ext/gpui/quickjs/quickjs.c",
        flags: ["-std=gnu11", "-D_GNU_SOURCE", "-funsigned-char", "-Wno-implicit-fallthrough"],
        msvcFlags: [
          "/std:c11",
          "/experimental:c11atomics",
          "/D_CRT_SECURE_NO_WARNINGS",
          "/D_CRT_NONSTDC_NO_DEPRECATE",
          "/DWIN32_LEAN_AND_MEAN",
          "/D_WIN32_WINNT=0x0601",
        ],
      },
    ],
  },
  // ── third-party, group A: mupdf and its dependencies ──────────────────
  // defines and include dirs come from orig premake5.lua (the MSVC build);
  // the file lists from premake5.files.lua, as globs minus what orig leaves out
  {
    name: "a-zlib",
    kind: "staticlib",
    sources: ["ext/a-zlib/zlib.c"],
    includes: ["ext/a-zlib"],
    strict: false,
    alwaysOptimize: true,
    // gzlib.c calls lseek without a prototype (zlib.h only includes
    // <unistd.h> when Z_HAVE_UNISTD_H is on, which would also change z_off_t
    // for this lib alone). wasm-ld turns a prototype-less call into a real
    // signature mismatch, so give it the declaration instead
    perSource: [{ glob: "ext/a-zlib/zlib.c", flags: ["-include", "unistd.h"], platforms: ["wasm"] }],
  },
  {
    name: "a-brotli",
    kind: "staticlib",
    sources: ["ext/a-brotli/brotli.c"],
    includes: ["ext/a-brotli"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-freetype",
    kind: "staticlib",
    sources: ["ext/a-freetype/freetype.c"],
    defines: ["FT2_BUILD_LIBRARY", 'FT_CONFIG_MODULES_H="slimftmodules.h"', 'FT_CONFIG_OPTIONS_H="slimftoptions.h"'],
    includes: ["ext/mupdf/scripts/freetype", "ext/a-freetype/include", "ext/a-brotli", "ext/a-zlib"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-harfbuzz",
    kind: "staticlib",
    sources: ["ext/a-harfbuzz/harfbuzz.cc"],
    // hb_*_impl: plain malloc wrappers in src/mupdf/mupdf_load_system_font.c,
    // so harfbuzz allocation does not need mupdf's thread-local fz_hb_secret
    defines: [
      "HAVE_FALLBACK=1",
      "HAVE_OT",
      "HAVE_FREETYPE",
      "hb_malloc_impl=sumatra_hb_malloc",
      "hb_calloc_impl=sumatra_hb_calloc",
      "hb_realloc_impl=sumatra_hb_realloc",
      "hb_free_impl=sumatra_hb_free",
    ],
    // frees the harfbuzz singletons at exit; only matters for leak detection
    debugDefines: ["HAVE_ATEXIT"],
    includes: ["ext/a-harfbuzz", "ext/mupdf/scripts/freetype", "ext/a-freetype/include"],
    strict: false,
    exceptions: false,
    alwaysOptimize: true,
    // the amalgamation is ordered for a compiler without symbol visibility
    // (MSVC): the tables it wraps in `#ifdef HB_NO_VISIBILITY` come before
    // their uses. gcc/clang have visibility, so define it for them too;
    // HB_INTERNAL then expands to nothing, which is right for a static lib
    perPlatform: {
      linux: hbNoVisibility,
      mac: hbNoVisibility,
      wasm: hbNoVisibility,
    },
    // one translation unit of templated OpenType tables exceeds the 64k
    // section limit of the default object file format
    perSource: [{ glob: "ext/a-harfbuzz/harfbuzz.cc", flags: [], msvcFlags: ["/bigobj"] }],
  },
  {
    name: "a-jbig2dec",
    kind: "staticlib",
    sources: ["ext/a-jbig2dec/jbig2dec.c"],
    defines: ["HAVE_STRING_H=1", "JBIG_NO_MEMENTO"],
    includes: ["ext/a-jbig2dec"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-openjpeg",
    kind: "staticlib",
    sources: ["ext/a-openjpeg/openjpeg.c"],
    defines: ["USE_JPIP", "OPJ_STATIC", "OPJ_EXPORTS"],
    includes: ["ext/a-openjpeg"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-lcms2",
    kind: "staticlib",
    sources: ["ext/a-lcms2/lcms2.c"],
    includes: ["ext/a-lcms2"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-mujs",
    kind: "staticlib",
    sources: ["ext/a-mujs/mujs.c"],
    includes: ["ext/a-mujs"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-extract",
    kind: "staticlib",
    sources: ["ext/a-extract/extract.c"],
    // mupdf provides memento.obj; skip extract's copy so the link has one
    defines: ["EXTRACT_NO_OWN_MEMENTO"],
    includes: ["ext/a-extract", "ext/a-zlib"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-gumbo",
    kind: "staticlib",
    sources: ["ext/a-gumbo/gumbo.c"],
    includes: ["ext/a-gumbo"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "a-libwebp",
    kind: "staticlib",
    sources: ["ext/a-libwebp/libwebp.c"],
    includes: ["ext/a-libwebp"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    // libjpeg-turbo 3.x: core sources plus the 8-bit precision wrappers
    // (12/16-bit is unused: mupdf only calls the 8-bit API). The x86-64 NASM
    // SIMD is assembled on Windows; elsewhere WITHOUT_SIMD forces the C path.
    name: "libjpeg-turbo",
    kind: "staticlib",
    sources: [
      // precision-independent core
      "ext/libjpeg-turbo/src/jaricom.c",
      "ext/libjpeg-turbo/src/jcapimin.c",
      "ext/libjpeg-turbo/src/jcarith.c",
      "ext/libjpeg-turbo/src/jchuff.c",
      "ext/libjpeg-turbo/src/jcicc.c",
      "ext/libjpeg-turbo/src/jcinit.c",
      "ext/libjpeg-turbo/src/jclhuff.c",
      "ext/libjpeg-turbo/src/jcmarker.c",
      "ext/libjpeg-turbo/src/jcmaster.c",
      "ext/libjpeg-turbo/src/jcomapi.c",
      "ext/libjpeg-turbo/src/jcparam.c",
      "ext/libjpeg-turbo/src/jcphuff.c",
      "ext/libjpeg-turbo/src/jctrans.c",
      "ext/libjpeg-turbo/src/jdapimin.c",
      "ext/libjpeg-turbo/src/jdarith.c",
      "ext/libjpeg-turbo/src/jdatadst.c",
      "ext/libjpeg-turbo/src/jdatasrc.c",
      "ext/libjpeg-turbo/src/jdhuff.c",
      "ext/libjpeg-turbo/src/jdicc.c",
      "ext/libjpeg-turbo/src/jdinput.c",
      "ext/libjpeg-turbo/src/jdlhuff.c",
      "ext/libjpeg-turbo/src/jdmarker.c",
      "ext/libjpeg-turbo/src/jdmaster.c",
      "ext/libjpeg-turbo/src/jdphuff.c",
      "ext/libjpeg-turbo/src/jdtrans.c",
      "ext/libjpeg-turbo/src/jerror.c",
      "ext/libjpeg-turbo/src/jfdctflt.c",
      "ext/libjpeg-turbo/src/jmemmgr.c",
      "ext/libjpeg-turbo/src/jmemnobs.c",
      "ext/libjpeg-turbo/src/jpeg_nbits.c",
      // 8-bit precision wrappers; each #includes ../<name>.c
      "ext/libjpeg-turbo/src/wrapper/*-8.c",
      "ext/libjpeg-turbo/simd/x86_64/jsimd.c",
    ],
    // the readers/writers belong to cjpeg/djpeg, not to the library
    exclude: [
      "ext/libjpeg-turbo/src/wrapper/rdcolmap-8.c",
      "ext/libjpeg-turbo/src/wrapper/rdppm-8.c",
      "ext/libjpeg-turbo/src/wrapper/wrgif-8.c",
      "ext/libjpeg-turbo/src/wrapper/wrppm-8.c",
    ],
    includes: ["ext/libjpeg-turbo/src"],
    strict: false,
    alwaysOptimize: true,
    asm: {
      sources: ["ext/libjpeg-turbo/simd/x86_64/*.asm"],
      // *ext-*.asm are %included by the files above, never assembled alone
      exclude: ["ext/libjpeg-turbo/simd/x86_64/*ext-*.asm"],
      includes: ["ext/libjpeg-turbo/simd/nasm", "ext/libjpeg-turbo/simd/x86_64"],
      defines: ["WIN64", "__x86_64__"],
      platforms: ["win"],
    },
    // no assembler: jconfig.h would still turn WITH_SIMD on for x86-64
    perPlatform: {
      linux: noSimd,
      mac: noSimd,
      wasm: noSimd,
    },
  },
  {
    name: "cmark-gfm",
    kind: "staticlib",
    sources: ["ext/cmark-gfm/src/*.c", "ext/cmark-gfm/extensions/*.c"],
    defines: ["CMARK_GFM_STATIC_DEFINE"],
    includes: ["ext/cmark-gfm/src", "ext/cmark-gfm/extensions", "ext/mupdf/scripts/cmark-gfm"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    // mupdf plus our own files that are compiled into it (src/mupdf/)
    name: "mupdf",
    kind: "staticlib",
    sources: [
      "src/ng/mupdf/*.c",
      // JPEG XL images and PDF /JXLDecode streams through jxldec
      "src/mupdf/load-jxl.cpp",
      "ext/mupdf/source/cbz/*.c",
      "ext/mupdf/source/fitz/*.c",
      "ext/mupdf/source/html/*.c",
      "ext/mupdf/source/pdf/*.c",
      "ext/mupdf/source/svg/*.c",
      "ext/mupdf/source/xps/*.c",
      "ext/mupdf/source/reflow/*.c",
      "ext/mupdf/source/tools/*.c",
      "ext/mupdf/source/helpers/mu-threads/mu-threads.c",
    ],
    exclude: [
      "ext/mupdf/source/fitz/leptonica-wrap.c", // no OCR
      "ext/mupdf/source/fitz/noto.c", // src/mupdf/noto_sumatra.c instead
      "ext/mupdf/source/tools/cmapdump.c",
      "ext/mupdf/source/tools/mubar.c",
      // both define main(); nothing calls muraster and GNU ld rejects the
      // second definition (cl.exe just kept the first)
      "ext/mupdf/source/tools/muraster.c",
      "ext/mupdf/source/tools/mutool.c",
    ],
    defines: [
      "USE_JPIP",
      "OPJ_EXPORTS",
      "OPJ_STATIC",
      "HAVE_LCMS2MT=1",
      "HAVE_WEBP=1",
      "SHARE_JPEG",
      // built-in fonts come from the loader (embedded pak, then cache, then
      // fonts_map.c). Source Han is not packed: skip its table entries
      "TOFU_CJK_LANG",
      "FZ_ENABLE_PDF=1",
      "FZ_ENABLE_SVG=1",
      "FZ_ENABLE_BROTLI=1",
      "FZ_ENABLE_BARCODE=0",
      "FZ_ENABLE_JS=1",
      "FZ_ENABLE_HYPHEN=0",
      "FZ_ENABLE_MD=1",
      "CMARK_GFM_STATIC_DEFINE",
      // cbz/archive.c reads cbz/cbt/tar through libarchive
      "HAVE_LIBARCHIVE",
      "LIBARCHIVE_STATIC",
      // "NAN" is not a constant in some versions of the UCRT headers
      "_UCRT_NOISY_NAN",
    ],
    includes: [
      "src/ng",
      "src/ng/mupdf",
      "ext/mupdf/source/fitz",
      "ext/jxldec",
      "ext/mupdf/include",
      "ext/a-jbig2dec",
      "ext/libjpeg-turbo/src",
      "ext/a-openjpeg",
      "ext/mupdf/scripts/freetype",
      "ext/a-freetype/include",
      "ext/a-mujs",
      "ext/a-brotli",
      "ext/cmark-gfm/src",
      "ext/cmark-gfm/extensions",
      "ext/mupdf/scripts/cmark-gfm",
      "ext/a-harfbuzz",
      "ext/a-lcms2",
      "ext/a-gumbo",
      "ext/a-extract",
      "ext/a-libwebp",
      "ext/a-libarchive",
      "ext/a-zlib",
    ],
    // reverse of the link order we want: depsOf() reverses this list
    deps: [
      "jxldec",
      "a-zlib",
      "a-libarchive",
      "a-libwebp",
      "libjpeg-turbo",
      "a-jbig2dec",
      "a-openjpeg",
      "a-lcms2",
      "a-brotli",
      "a-freetype",
      "a-harfbuzz",
      "a-extract",
      "a-mujs",
      "a-gumbo",
      "cmark-gfm",
    ],
    strict: false,
    exceptions: false,
    perPlatform: {
      win: { exclude: ["ext/mupdf/source/fitz/load-jxr.c"] },
      linux: mupdfLinux,
      mac: mupdfPosix,
      wasm: mupdfPosix,
    },
  },
  // ── third-party, group B: archives, image codecs, misc ────────────────
  {
    // libarchive with the bundled bzip2 and the liblzma decoder, as orig's
    // a-libarchive project compiles them: one static lib, one config header
    // per platform (config_windows.h / config_linux.h via PLATFORM_CONFIG_H,
    // liblzma's own config.h via HAVE_CONFIG_H)
    name: "a-libarchive",
    kind: "staticlib",
    sources: [
      "ext/a-libarchive/libarchive.c",
      // the POSIX entry points the Windows-flavoured amalgamation is missing
      "src/ng/libarchive/libarchive_posix.c",
      "ext/a-bzip2/bzip2.c",
      ...liblzmaSources,
    ],
    defines: [
      "LIBARCHIVE_STATIC",
      'PLATFORM_CONFIG_H="config_windows.h"',
      "BZ_NO_STDIO",
      "HAVE_CONFIG_H",
      "LZMA_API_STATIC",
    ],
    includes: ["ext/a-libarchive", "ext/a-libarchive/libarchive", "ext/a-zlib", "ext/a-bzip2", ...liblzmaIncludes],
    deps: ["a-zlib"],
    strict: false,
    alwaysOptimize: true,
    // ext/liblzma/config.h is the Windows one; off Windows the generated copy
    // shadows it
    generated: [
      { name: "config.h", from: "ext/liblzma/config_linux.h", platforms: ["linux", "wasm"] },
      { name: "config.h", from: "ext/liblzma/config_macos.h", platforms: ["mac"] },
      { name: "config_posix.h", content: libarchivePosixConfig, platforms: ["linux", "mac", "wasm"] },
    ],
    perPlatform: {
      linux: linuxPlatformConfig,
      mac: linuxPlatformConfig,
      wasm: linuxPlatformConfig,
    },
  },
  {
    // unrar amalgamation. C++ with throw/catch, so exceptions stay on.
    name: "a-unrar",
    kind: "staticlib",
    sources: ["ext/a-unrar/unrar.cpp"],
    defines: ["UNRAR", "RARDLL", "SILENT"],
    includes: ["ext/a-unrar"],
    strict: false,
    exceptions: true,
    alwaysOptimize: true,
    perPlatform: {
      linux: unrarPosix,
      mac: unrarPosix,
      wasm: unrarPosix,
    },
  },
  {
    name: "chmdec",
    kind: "staticlib",
    sources: ["ext/chmdec/*.c"],
    includes: ["ext/chmdec"],
    strict: false,
    alwaysOptimize: true,
    // chm.c calls the MSVC spellings and gets PATH_MAX from <limits.h>
    perPlatform: {
      linux: { defines: ["_stricmp=strcasecmp", "_strnicmp=strncasecmp"] },
      mac: { defines: ["_stricmp=strcasecmp", "_strnicmp=strncasecmp"] },
      wasm: { defines: ["_stricmp=strcasecmp", "_strnicmp=strncasecmp"] },
    },
    perSource: [{ glob: "ext/chmdec/*.c", flags: ["-include", "limits.h"] }],
  },
  {
    name: "msdes",
    kind: "staticlib",
    sources: ["ext/msdes/*.c"],
    includes: ["ext/msdes"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    name: "djvudec",
    kind: "staticlib",
    sources: ["ext/djvudec/djvu.c"],
    includes: ["ext/djvudec"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    // AV1 decoder, used by heicdec for AVIF. x86-64 NASM asm on Windows;
    // elsewhere the portable C with HAVE_ASM=0 from the generated config.
    name: "dav1d",
    kind: "staticlib",
    sources: [
      "ext/dav1d/src/cdf.c",
      "ext/dav1d/src/cpu.c",
      "ext/dav1d/src/ctx.c",
      "ext/dav1d/src/data.c",
      "ext/dav1d/src/decode.c",
      "ext/dav1d/src/dequant_tables.c",
      "ext/dav1d/src/getbits.c",
      "ext/dav1d/src/intra_edge.c",
      "ext/dav1d/src/itx_1d.c",
      "ext/dav1d/src/lf_mask.c",
      "ext/dav1d/src/lib.c",
      "ext/dav1d/src/log.c",
      "ext/dav1d/src/mem.c",
      "ext/dav1d/src/msac.c",
      "ext/dav1d/src/obu.c",
      "ext/dav1d/src/pal.c",
      "ext/dav1d/src/picture.c",
      "ext/dav1d/src/qm.c",
      "ext/dav1d/src/ref.c",
      "ext/dav1d/src/refmvs.c",
      "ext/dav1d/src/scan.c",
      "ext/dav1d/src/tables.c",
      "ext/dav1d/src/thread_task.c",
      "ext/dav1d/src/warpmv.c",
      "ext/dav1d/src/wedge.c",
      // one TU per bitdepth: the *_tmpl.c sources are compiled twice
      "ext/dav1d/src/sumatra_bitdepth_8.c",
      "ext/dav1d/src/sumatra_bitdepth_8_2.c",
      "ext/dav1d/src/sumatra_bitdepth_16.c",
      "ext/dav1d/src/sumatra_bitdepth_16_2.c",
      "ext/dav1d/src/x86/cpu.c",
    ],
    // compat/msvc holds a stdatomic.h for cl.exe; gcc/clang have the real one
    // and must not see the shim, so it is added per platform below
    includes: ["ext/dav1d", "ext/dav1d/include"],
    strict: false,
    alwaysOptimize: true,
    asm: {
      sources: dav1dAsm,
      includes: ["ext/dav1d/src", "ext/dav1d/include"],
      defines: ["ARCH_X86_64=1", "ARCH_X86_32=0", "__x86_64__", "WIN64", "MSVC"],
      platforms: ["win"],
    },
    generated: [
      { name: "config.h", content: dav1dConfig(hostArm64), platforms: ["linux", "mac"] },
      { name: "config.h", content: dav1dConfig(false), platforms: ["wasm"] },
    ],
    // off Windows HAVE_ASM (=0) and the arch macros come from the generated
    // config.h; threads are pthreads, so win32/thread.c is Windows-only.
    // The checked-in config.h leaves ARCH_X86_* to the build system, as orig's
    // premake does
    perPlatform: {
      win: {
        sources: ["ext/dav1d/src/win32/thread.c"],
        defines: ["ARCH_X86_32=0", "ARCH_X86_64=1", "HAVE_ASM=1"],
        includes: ["ext/dav1d/include/compat/msvc"],
      },
      ...(hostArm64 ? { linux: dav1dArmCpu, mac: dav1dArmCpu } : {}),
    },
  },
  {
    // HEIC/HEIF/AVIF decoder amalgamation (replaces libheif): HEVC in C,
    // AV1 through dav1d, unci compression through zlib / brotli
    name: "heicdec",
    kind: "staticlib",
    sources: ["ext/heicdec/heic.c"],
    defines: ["HEIC_HAVE_DAV1D", "HEIC_HAVE_ZLIB", "HEIC_HAVE_BROTLI"],
    includes: ["ext/heicdec", "ext/dav1d/include", "ext/a-zlib", "ext/a-brotli"],
    deps: ["a-zlib", "a-brotli", "dav1d"],
    strict: false,
    alwaysOptimize: true,
    // heic.c uses SSE4.1 intrinsics behind an x86 guard; cl.exe enables them
    // on x64 by itself, clang rejects -msse4.1 outright on arm64 and em++
    // rejects it without -msimd128
    perSource: [
      {
        glob: "ext/heicdec/heic.c",
        flags: hostArm64 ? [] : ["-msse4.1"],
        platforms: ["win", "linux", "mac"],
      },
    ],
  },
  {
    // JPEG XL decoder amalgamation (replaces libjxl + highway + skcms)
    name: "jxldec",
    kind: "staticlib",
    sources: ["ext/jxldec/jxl.c"],
    includes: ["ext/jxldec"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    // zopfli / zopflipng: lossless PNG recompression of images we save
    name: "a-zopfli",
    kind: "staticlib",
    sources: ["ext/a-zopfli/zopfli.cpp"],
    includes: ["ext/a-zopfli"],
    strict: false,
    exceptions: false,
    alwaysOptimize: true,
  },
  {
    // synctex: TeX source <-> PDF position mapping (orig compiles it into the
    // app; a static lib keeps the warning relaxation out of our sources)
    name: "synctex",
    kind: "staticlib",
    sources: ["ext/synctex/synctex_parser.c", "ext/synctex/synctex_parser_utils.c"],
    includes: ["ext/synctex", "ext/a-zlib"],
    deps: ["a-zlib"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    // LzSA decoder for src/base/LzmaSimpleArchive.cpp. Deliberately not
    // liblzma: orig keeps this copy out of libarchive so the installer can
    // extract without the delay-loaded DLL.
    name: "lzma",
    kind: "staticlib",
    sources: ["ext/lzma/C/LzmaDec.c", "ext/lzma/C/Bra86.c"],
    includes: ["ext/lzma/C"],
    strict: false,
    alwaysOptimize: true,
  },
  {
    // orig's "base" project: src/base plus gui/Dpi. No gpui.
    name: "base",
    kind: "staticlib",
    // PerfLog.cpp: the profiler's hooks must not be instrumented themselves
    sources: ["src/ng/base/*.cpp", "src/ng/gui/Dpi*.cpp", "src/ng/PerfLog.cpp"],
    exclude: [
      // not in orig's base project: the app links one of CrashHandler.cpp /
      // CrashHandlerNoOp.cpp and one of SumatraLog.cpp / LogNoOp.cpp
      "src/ng/base/CrashHandler.cpp",
      "src/ng/base/CrashHandler_mac.cpp",
      "src/ng/base/CrashHandler_posix.cpp",
      "src/base/LogNoOp.cpp",
    ],
    defines: ["LIBARCHIVE_STATIC"],
    includes: ["src/ng", "ext/a-zlib", "ext/a-libarchive", "ext/lzma/C"],
    deps: ["a-zlib", "lzma", "a-unrar", "a-libarchive"],
    strict: true,
    exceptions: true,
    // _CAP_Enter_Function / _CAP_Exit_Function, the /callcap hooks
    asm: { sources: ["src/PerfLog_x64.asm"], profileOnly: true },
    // Arena_wasm.cpp replaces the mmap/mprotect half, Launch_wasm.cpp the
    // posix_spawnp one
    perPlatform: {
      mac: { defines: ["GPUI_HAVE_CURL=1"] },
      wasm: { exclude: ["src/ng/base/Arena_posix.cpp", "src/ng/base/Launch_posix.cpp"] },
    },
  },
  {
    // orig's engines_files() (compiled into SumatraPDF there) as its own lib:
    // the document engines and the model code they need. No UI.
    name: "engines",
    kind: "staticlib",
    callcap: true,
    sources: enginesSources,
    defines: [
      "LIBARCHIVE_STATIC",
      "CMARK_GFM_STATIC_DEFINE",
      // orig's SumatraPDF project: don't honor a PDF's "don't copy text" flag
      "DISABLE_DOCUMENT_RESTRICTIONS",
    ],
    includes: [
      "src/ng",
      "ext/mupdf/include",
      "ext/a-zlib",
      "ext/a-libarchive",
      "ext/a-unrar",
      "ext/a-libwebp",
      "ext/chmdec",
      "ext/djvudec",
      "ext/heicdec",
      "ext/jxldec",
      "ext/msdes",
      "ext/synctex",
      "ext/a-zopfli",
    ],
    // reverse of the link order we want: depsOf() reverses this list
    deps: ["synctex", "a-zopfli", "a-libwebp", "heicdec", "jxldec", "djvudec", "msdes", "chmdec", "mupdf", "base"],
    strict: true,
    exceptions: true,
  },
  {
    // The one file compiled with the version defines. They change with the
    // day and the commit, and a change of flags recompiles a whole target.
    name: "appver",
    kind: "staticlib",
    sources: ["src/ng/SumatraConfig.cpp"],
    defines: ["DISABLE_DOCUMENT_RESTRICTIONS"],
    versionDefines: true,
    includes: ["src/ng"],
    strict: true,
    exceptions: true,
  },
  {
    // orig's application layer minus the UI: settings, commands, shortcuts,
    // translations, theme colors. Step 5b adds the document model.
    name: "app",
    kind: "staticlib",
    embedded: true,
    callcap: true,
    sources: appSources,
    defines: ["LIBARCHIVE_STATIC", "CMARK_GFM_STATIC_DEFINE", "DISABLE_DOCUMENT_RESTRICTIONS"],
    perSource: [
      { glob: "src/shared/Commands.cpp", flags: ["-DNO_THUMBNAIL_STATE_ARG"], msvcFlags: ["/DNO_THUMBNAIL_STATE_ARG"] },
    ],
    // MarkdownToc.cpp renders markdown with cmark-gfm (linked in through mupdf)
    includes: [
      "src/ng",
      "ext/mupdf/include",
      "ext/cmark-gfm/src",
      "ext/cmark-gfm/extensions",
      "ext/mupdf/scripts/cmark-gfm",
    ],
    deps: ["engines", "appver"],
    strict: true,
    exceptions: true,
  },
  {
    name: "test_util",
    kind: "console",
    sources: [
      "src/ng/tools/test_util.cpp",
      "src/ng/tools/AppStubs.cpp",
      "src/ng/gui/BrowserViewNoOp.cpp",
      "src/ng/base/tests/*.cpp",
      "src/ng/tests/*.cpp",
      "src/ng/TipMarkup.cpp",
      // ReadAloudHighlight_ut needs the text half of read aloud (step 14c)
      "src/ng/ReadAloudHighlight.cpp",
      "src/ng/SumatraLog.cpp",
      "src/ng/CrashHandlerNoOp.cpp",
    ],
    includes: ["src/ng", "ext/mupdf/include", "ext/djvudec"],
    deps: ["app"],
    winLibs: [
      "advapi32.lib",
      "comctl32.lib",
      "comdlg32.lib",
      "crypt32.lib",
      "gdi32.lib",
      "gdiplus.lib",
      "kernel32.lib",
      "msimg32.lib",
      "ole32.lib",
      "oleaut32.lib",
      "shell32.lib",
      "shlwapi.lib",
      "urlmon.lib",
      "user32.lib",
      "version.lib",
      "windowscodecs.lib",
      "wininet.lib",
      "winspool.lib",
      "wintrust.lib",
    ],
    strict: true,
    exceptions: true,
  },
  {
    // minimal host for SumatraPDF -plugin mode (orig src/tools/plugin-test.cpp)
    name: "plugin-test",
    kind: "app",
    sources: ["src/ng/tools/plugin-test.cpp"],
    includes: ["src/ng"],
    deps: ["base"],
    platforms: ["win"],
    winLibs: [
      "advapi32.lib",
      "comctl32.lib",
      "comdlg32.lib",
      "crypt32.lib",
      "gdi32.lib",
      "gdiplus.lib",
      "kernel32.lib",
      "msimg32.lib",
      "ole32.lib",
      "oleaut32.lib",
      "shell32.lib",
      "shlwapi.lib",
      "urlmon.lib",
      "user32.lib",
      "version.lib",
      "windowscodecs.lib",
      "wininet.lib",
      "winspool.lib",
      "wintrust.lib",
    ],
    strict: true,
    exceptions: true,
  },
  {
    // smoke test for the mupdf lib: opens a document, prints the page count
    name: "test_mupdf",
    kind: "console",
    sources: ["src/ng/tools/test_mupdf.cpp"],
    includes: ["ext/mupdf/include"],
    deps: ["mupdf"],
    winLibs: [
      "advapi32.lib",
      "crypt32.lib",
      "gdi32.lib",
      "kernel32.lib",
      "ole32.lib",
      "oleaut32.lib",
      "shell32.lib",
      "shlwapi.lib",
      "user32.lib",
      "windowscodecs.lib",
    ],
    strict: true,
    exceptions: false,
  },
  {
    // loads a document with the engines, prints page count and properties,
    // renders a page to a PNG
    name: "test_engines",
    kind: "console",
    sources: [
      "src/ng/tools/test_engines.cpp",
      "src/ng/tools/AppStubs.cpp",
      "src/ng/gui/BrowserViewNoOp.cpp",
      "src/ng/CrashHandlerNoOp.cpp",
    ],
    includes: ["src/ng", "ext/mupdf/include"],
    deps: ["app"],
    winLibs: [
      "advapi32.lib",
      "comctl32.lib",
      "comdlg32.lib",
      "crypt32.lib",
      "gdi32.lib",
      "gdiplus.lib",
      "kernel32.lib",
      "msimg32.lib",
      "ole32.lib",
      "oleaut32.lib",
      "shell32.lib",
      "shlwapi.lib",
      "urlmon.lib",
      "user32.lib",
      "version.lib",
      "windowscodecs.lib",
      "wininet.lib",
      "winspool.lib",
      "wintrust.lib",
    ],
    strict: true,
    exceptions: true,
  },
  {
    name: "SumatraPDF",
    kind: "app",
    callcap: true,
    sources: [
      "src/ng/SumatraPDF.cpp",
      "src/ng/MainWindow.cpp",
      "src/ng/WindowTab.cpp",
      "src/ng/Tabs.cpp",
      "src/ng/TabGroupsManage.cpp",
      "src/ng/SessionState.cpp",
      "src/ng/Menu.cpp",
      "src/ng/TipMarkup.cpp",
      "src/ng/Notifications.cpp",
      "src/ng/Selection.cpp",
      "src/ng/SelectionToolbar.cpp",
      "src/ng/SelectTextKeyboard.cpp",
      "src/ng/LinkFollow.cpp",
      "src/ng/SearchAndDDE.cpp",
      "src/ng/FindBar.cpp",
      "src/ng/FindWindow.cpp",
      "src/ng/Toolbar.cpp",
      "src/ng/OverlayScrollbar.cpp",
      "src/ng/ReadingBar.cpp",
      "src/ng/ReadingAutoScroll.cpp",
      "src/ng/ReadAloud.cpp",
      "src/ng/ReadAloud_linux.cpp",
      "src/ng/ReadAloud_win.cpp",
      "src/ng/ReadAloud_mac.mm",
      "src/ng/ReadAloud_wasm.cpp",
      "src/ng/ReadAloudHighlight.cpp",
      "src/ng/SvgIcons.cpp",
      "src/ng/AnnotEditToolbar.cpp",
      "src/ng/AnnotFilterToolbar.cpp",
      "src/ng/AnnotPlacement.cpp",
      "src/ng/AnnotTextPopup.cpp",
      "src/ng/FilterHighlightDraw.cpp",
      "src/ng/RefHover.cpp",
      "src/ng/RefHoverCanvas.cpp",
      "src/ng/RefHoverPopup.cpp",
      "src/ng/RefHoverRender.cpp",
      "src/ng/RefHoverShow.cpp",
      "src/ng/FormFields.cpp",
      "src/ng/SavePathDialog.cpp",
      "src/ng/GoToPageDialog.cpp",
      "src/ng/SumatraDialogs.cpp",
      "src/ng/GetPasswordDialog.cpp",
      "src/ng/CustomZoomDialog.cpp",
      "src/ng/ChangeScrollbarDialog.cpp",
      "src/ng/ChangeLanguageDialog.cpp",
      "src/ng/ChangeThemeDialog.cpp",
      "src/ng/ChangeColorDialog.cpp",
      "src/ng/InverseSearchDialog.cpp",
      "src/ng/EbookSettingsDialog.cpp",
      "src/ng/SettingsDialog.cpp",
      "src/ng/AdvancedSettingsDialog.cpp",
      "src/ng/PageGridDialog.cpp",
      "src/ng/KeyboardHelp.cpp",
      "src/ng/SignDocumentDialog.cpp",
      "src/ng/ImageSaveCropResize.cpp",
      "src/ng/ImageEditHostSumatra.cpp",
      "src/ng/Screenshot.cpp",
      "src/ng/PdfTools.cpp",
      "src/ng/SelectionHandlers.cpp",
      "src/ng/SelectionTranslate.cpp",
      "src/ng/GoogleLens.cpp",
      "src/ng/AIChatCommon.cpp",
      "src/ng/AIChatPanel.cpp",
      "src/ng/AIClaudeCode.cpp",
      "src/ng/AIGrokBuild.cpp",
      "src/ng/AICodexBuild.cpp",
      "src/ng/AIAntiGravity.cpp",
      "src/ng/SimpleBrowserWindow.cpp",
      "src/ng/TextViewWnd.cpp",
      "src/ng/gui/DialogWidgets.cpp",
      "src/ng/DocumentProperties.cpp",
      "src/ng/HomePage.cpp",
      "src/ng/AddFavoriteDialog.cpp",
      "src/ng/Favorites.cpp",
      "src/ng/TableOfContents.cpp",
      "src/ng/gui/AppShell.cpp",
      "src/ng/gui/GpuiTheme.cpp",
      "src/ng/gui/DocCanvas.cpp",
      "src/ng/gui/Sidebar.cpp",
      "src/ng/gui/TabsUI.cpp",
      "src/ng/gui/TabSwitcher.cpp",
      "src/ng/CommandPalette.cpp",
      "src/ng/gui/BrowserView.cpp",
      "src/ng/gui/NavFilesUI.cpp",
      "src/ng/gui/ToolWindow.cpp",
      "src/ng/gui/ToolWindow_mac.mm",
      "src/ng/gui/ToolWindow_linux.cpp",
      "src/ng/ExternalViewers.cpp",
      "src/ng/gui/NativeWindow.cpp",
      "src/ng/gui/NativeWindow_mac.mm",
      "src/ng/gui/NativeWindow_wasm.cpp",
      "src/ng/gui/OleDragDrop_win.cpp",
      "src/ng/gui/NativeFileDlg_win.cpp",
      "src/ng/gui/NativeMsgBox_win.cpp",
      "src/ng/gui/NativeCursors_win.cpp",
      "src/ng/gui/TouchGestures_win.cpp",
      "src/ng/GlobalHotkeys.cpp",
      "src/ng/Print.cpp",
      "src/ng/Print_posix.cpp",
      "src/ng/PrintWin11.cpp",
      "src/ng/HangDetector.cpp",
      "src/ng/ExplorerQuickLook.cpp",
      "src/ng/UpdateCheck.cpp",
      "src/ng/UpdateTemp.cpp",
      "src/ng/Installer.cpp",
      "src/ng/InstallerCommon.cpp",
      "src/ng/Uninstaller.cpp",
      "src/ng/RegistryInstaller.cpp",
      "src/ng/StressTesting.cpp",
      "src/ng/SumatraControl.cpp",
      "src/ng/gui/GpuiLog.cpp",
      "src/ng/SumatraLog.cpp",
      // ng: what the shell still owes the layers below it (step 9/12/14/17)
      "src/ng/ShellStubs.cpp",
      "src/ng/SumatraCrashHandler.cpp",
      // the app links the real crash handler; the console tools link
      // src/CrashHandlerNoOp.cpp instead, as orig does
      "src/ng/base/CrashHandler.cpp",
      "src/ng/base/CrashHandler_mac.cpp",
      "src/ng/base/CrashHandler_posix.cpp",
    ],
    includes: ["src/ng", "ext/gpui", "ext/mupdf/include", "src"],
    // the app icon, the document-type icons, the version resource and the
    // compatibility manifest (Windows only; other platforms ignore it)
    rc: "src/ng/SumatraPDF.rc",
    deps: ["gpui", "app"],
    winLibs: gpuiWinLibs,
    strict: true,
    exceptions: true,
    // baked into the wasm build's MEMFS: a sample document so the page shows
    // something without a file picker. The fonts used to be preloaded here; they
    // are linked into the embedded LzSA archive
    wasmPreload: [
      { from: "docs/test/zlib.3.pdf", to: kWasmDocsDir },
      { from: "docs/test/test.epub", to: kWasmDocsDir },
    ],
    // orig's desktop screenshot picker. Win32 only; other platforms keep the
    // page render in src/ng/Screenshot.cpp.
    perPlatform: {
      win: { sources: ["src/ScreenshotCapture.cpp"] },
    },
  },
];

export function findTarget(name: string): Target | undefined {
  return targets.find((t) => t.name === name);
}

export function targetsFor(plat: Platform): Target[] {
  return targets.filter((t) => !t.platforms || t.platforms.includes(plat));
}

/** The default build: the app plus everything it depends on. */
export const defaultTarget = "SumatraPDF";

// Suffix a platform-specific source carries and the platforms it builds on.
// A unit test of a platform file carries the same suffix before `_ut`
// (FileWatcher_posix_ut.cpp).
const sourceSuffixes: { re: RegExp; plats: Platform[] }[] = [
  { re: /_win(_ut)?\.(cpp|c|mm|m)$/, plats: ["win"] },
  { re: /_posix(_ut)?\.(cpp|c)$/, plats: ["linux", "mac", "wasm"] },
  { re: /_linux(_ut)?\.(cpp|c)$/, plats: ["linux"] },
  { re: /_mac(_ut)?\.(cpp|c|mm|m)$/, plats: ["mac"] },
  { re: /_wasm(_ut)?\.(cpp|c)$/, plats: ["wasm"] },
  { re: /\.(mm|m)$/, plats: ["mac"] },
];

export function sourceBuildsOn(path: string, plat: Platform): boolean {
  for (const s of sourceSuffixes) {
    if (s.re.test(path)) return s.plats.includes(plat);
  }
  return true;
}
