/**
 * Build SumatraPDF with mingw-w64 cross-compiler.
 * Usage: bun cmd/build-with-mingw.ts -debug
 *        bun cmd/build-with-mingw.ts -release
 *
 * Replicates build logic from premake5.lua / premake5.files.lua
 * for x64 targets using x86_64-w64-mingw32-g++.
 *
 * NOTE: .asm (NASM) files are skipped; C fallbacks are used instead.
 * NOTE: Font embedding (.cff/.ttf/.otf) uses objcopy.
 * NOTE: WebView2 may not be available for mingw builds.
 */

import { Glob } from "bun";
import { mkdirSync, existsSync, statSync, rmSync } from "node:fs";
import { readFile, writeFile } from "node:fs/promises";
import { join, extname, dirname, basename } from "node:path";
import { cpus } from "node:os";

// ── Tool paths ──────────────────────────────────────────────────────────────
const CC = "x86_64-w64-mingw32-gcc";
const CXX = "x86_64-w64-mingw32-g++";
const AR = "x86_64-w64-mingw32-ar";
const WINDRES = "x86_64-w64-mingw32-windres";
const OBJCOPY = "x86_64-w64-mingw32-objcopy";
const JOBS = Math.max(1, cpus().length);

// ── Common defines (workspace-level from premake5.lua) ─────────────────────
const COMMON_DEFINES = ["WIN32", "_WIN32", "WINVER=0x0605", "_WIN32_WINNT=0x0603"];

// ── Types ───────────────────────────────────────────────────────────────────
interface FileGroup {
  dir: string;
  patterns: string[];
}

interface LibDef {
  name: string;
  /** file groups to resolve */
  files: FileGroup[];
  /** extra defines beyond COMMON_DEFINES */
  defines: string[];
  /** include directories */
  includes: string[];
  /** true = always -Os -DNDEBUG (optimized_conf); false = mixed_dbg_rel_conf */
  alwaysOptimize: boolean;
  /** extra defines only in debug (added after base defines) */
  debugExtraDefines?: string[];
  /** extra defines only in release */
  releaseExtraDefines?: string[];
  /** enable RTTI for C++ files */
  rtti?: boolean;
  /** enable exceptions for C++ files */
  exceptions?: boolean;
  /** extra compiler flags (e.g. -msse4.1) */
  extraCflags?: string[];
}

// ── Helpers ─────────────────────────────────────────────────────────────────

/** Resolve file patterns, returning only .c/.cpp/.cc source files */
async function resolveSources(groups: FileGroup[]): Promise<string[]> {
  const compilableExts = new Set(["c", "cpp", "cc"]);
  const result = new Set<string>();
  for (const { dir, patterns } of groups) {
    for (const pat of patterns) {
      const fullPat = join(dir, pat);
      const glob = new Glob(fullPat);
      for await (const path of glob.scan(".")) {
        const ext = extname(path).slice(1).toLowerCase();
        if (compilableExts.has(ext)) {
          result.add(path);
        }
      }
    }
  }
  return [...result].sort();
}

/** Create a unique .o path from a source path */
function objPath(outDir: string, libName: string, src: string): string {
  const flat = src.replace(/[\\/]/g, "__").replace(/\.[^.]+$/, ".o");
  return join(outDir, "obj", libName, flat);
}

/** Spawn a compilation command; returns success/failure + stderr */
async function spawnCmd(args: string[], opts?: { cwd?: string }): Promise<{ ok: boolean; stderr: string }> {
  const proc = Bun.spawn(args, { stdout: "ignore", stderr: "pipe", cwd: opts?.cwd });
  const code = await proc.exited;
  const stderr = await new Response(proc.stderr).text();
  return { ok: code === 0, stderr };
}

/** Compile a list of {src, obj, args} units in parallel */
async function compileAll(
  units: { src: string; obj: string; args: string[] }[],
  jobs: number,
): Promise<void> {
  let idx = 0;
  let failed = 0;
  const total = units.length;

  async function worker() {
    while (true) {
      const i = idx++;
      if (i >= total) break;
      const u = units[i];
      mkdirSync(dirname(u.obj), { recursive: true });

      // simple incremental: skip if .o newer than source
      if (existsSync(u.obj)) {
        try {
          const srcMtime = statSync(u.src).mtimeMs;
          const objMtime = statSync(u.obj).mtimeMs;
          if (objMtime > srcMtime) {
            continue; // up to date
          }
        } catch {}
      }

      const res = await spawnCmd(u.args);
      if (!res.ok) {
        console.error(`FAILED: ${u.src}`);
        if (res.stderr) console.error(res.stderr.trimEnd().slice(0, 1000));
        failed++;
      }
      const done = i + 1;
      if (done % 100 === 0 || done === total) {
        process.stdout.write(`\r  [${done}/${total}]`);
      }
    }
  }

  await Promise.all(Array.from({ length: Math.min(jobs, total) }, () => worker()));
  if (total > 0) process.stdout.write("\n");
  if (failed > 0) {
    throw new Error(`${failed} file(s) failed to compile`);
  }
}

/** Create a static .a archive from object files */
async function createArchive(archivePath: string, objFiles: string[]): Promise<void> {
  if (objFiles.length === 0) return;
  mkdirSync(dirname(archivePath), { recursive: true });
  // batch to avoid arg-too-long
  const batch = 200;
  for (let i = 0; i < objFiles.length; i += batch) {
    const chunk = objFiles.slice(i, i + batch);
    const flag = i === 0 ? "rcs" : "rs";
    const res = await spawnCmd([AR, flag, archivePath, ...chunk]);
    if (!res.ok) throw new Error(`ar failed: ${res.stderr}`);
  }
}

/** Convert a binary file to a .o with a specific symbol using objcopy.
 *  Produces symbols: _binary_<symbolPrefix>, _binary_<symbolPrefix>_size
 *  matching what mupdf's noto.c expects (non-HAVE_OBJCOPY path). */
async function embedBinaryFile(
  inputFile: string,
  outputObj: string,
  symbolPrefix: string,
): Promise<void> {
  mkdirSync(dirname(outputObj), { recursive: true });
  const outAbsolute = join(process.cwd(), outputObj);

  // Copy file to a temp dir with a clean name so objcopy derives predictable symbols.
  // objcopy -I binary derives symbols from the filename: _binary_<filename_with_dots_as_underscores>_{start,end,size}
  // We need the filename to match symbolPrefix (e.g. "NimbusMonoPS_Regular_cff")
  // so objcopy generates _binary_NimbusMonoPS_Regular_cff_{start,end,size}.
  const cleanFileName = symbolPrefix.replace(/^_binary_/, "");
  const tmpDir = join(dirname(outputObj), "_fonttmp");
  mkdirSync(tmpDir, { recursive: true });
  const tmpInput = join(tmpDir, cleanFileName);
  const data = await readFile(inputFile);
  await writeFile(tmpInput, data);

  // Run objcopy from the tmpDir so symbols are based on just the filename
  const res = await spawnCmd([
    OBJCOPY,
    "-I", "binary",
    "-O", "pe-x86-64",
    "-B", "i386:x86-64",
    "--rename-section", ".data=.rodata,CONTENTS,ALLOC,LOAD,READONLY,DATA",
    // Rename _start symbol to match the array name expected by noto.c
    "--redefine-sym", `_binary_${cleanFileName}_start=${symbolPrefix}`,
    cleanFileName,
    outAbsolute,
  ], { cwd: tmpDir });
  if (!res.ok) {
    console.error(`Failed to embed ${inputFile}: ${res.stderr}`);
  }
  // cleanup temp file
  try { await Bun.write(tmpInput, ""); } catch {}
}

// ── Library definitions ─────────────────────────────────────────────────────
// Each matches a project in premake5.lua / premake5.files.lua

const zlib: LibDef = {
  name: "zlib",
  alwaysOptimize: true,
  defines: [],
  includes: [],
  files: [
    {
      dir: "ext/zlib",
      patterns: [
        "adler32.c",
        "compress.c",
        "crc32.c",
        "deflate.c",
        "inffast.c",
        "inflate.c",
        "inftrees.c",
        "trees.c",
        "zutil.c",
        "gzlib.c",
        "gzread.c",
        "gzwrite.c",
        "gzclose.c",
      ],
    },
  ],
};

const unrar: LibDef = {
  name: "unrar",
  alwaysOptimize: true,
  // MSVC compiles throw/catch with exceptions disabled (warning 4530);
  // GCC requires -fexceptions for code that uses throw/catch
  exceptions: true,
  defines: ["UNRAR", "RARDLL", "SILENT"],
  includes: ["ext/unrar"],
  files: [
    {
      dir: "ext/unrar",
      patterns: [
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
        "global.cpp",
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
      ],
    },
  ],
};

const libdjvu: LibDef = {
  name: "libdjvu",
  alwaysOptimize: true,
  // libdjvu uses exceptions (GException.cpp, etc.)
  exceptions: true,
  defines: [
    "_CRT_SECURE_NO_WARNINGS",
    "NEED_JPEG_DECODER",
    "WINTHREADS=1",
    "DDJVUAPI=",
    "MINILISPAPI=",
    "DEBUGLVL=0",
    "DISABLE_MMX",
  ],
  includes: ["ext/libjpeg-turbo"],
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

const chm: LibDef = {
  name: "chm",
  alwaysOptimize: true,
  defines: ["_CRT_SECURE_NO_WARNINGS"],
  includes: [],
  files: [{ dir: "ext/CHMLib", patterns: ["chm_lib.c", "lzx.c"] }],
};

const unarrlib: LibDef = {
  name: "unarrlib",
  alwaysOptimize: true,
  defines: ["HAVE_ZLIB", "HAVE_BZIP2", "HAVE_7Z", "BZ_NO_STDIO", "_7ZIP_PPMD_SUPPPORT"],
  includes: ["ext/zlib", "ext/bzip2", "ext/lzma/C"],
  files: [
    { dir: "ext/unarr/common", patterns: ["*.c"] },
    { dir: "ext/unarr/rar", patterns: ["*.c"] },
    { dir: "ext/unarr/zip", patterns: ["*.c"] },
    { dir: "ext/unarr/tar", patterns: ["*.c"] },
    { dir: "ext/unarr/_7z", patterns: ["*.c"] },
    { dir: "ext/bzip2", patterns: ["bzip_all.c"] },
    {
      dir: "ext/unarr/lzmasdk",
      patterns: ["CpuArch.c", "Ppmd7.c", "Ppmd7Dec.c", "Ppmd8.c", "Ppmd8Dec.c"],
    },
    {
      dir: "ext/lzma/C",
      patterns: [
        "LzmaDec.c",
        "Bra86.c",
        "LzmaEnc.c",
        "LzFind.c",
        "LzFindMt.c",
        "Threads.c",
        "7zBuf.c",
        "7zDec.c",
        "7zIn.c",
        "7zStream.c",
        "Bcj2.c",
        "Bra.c",
        "Lzma2Dec.c",
      ],
    },
  ],
};

const libwebp: LibDef = {
  name: "libwebp",
  alwaysOptimize: true,
  defines: [],
  includes: ["ext/libwebp"],
  files: [
    { dir: "ext/libwebp/src/dec", patterns: ["*.c"] },
    {
      dir: "ext/libwebp/src/dsp",
      patterns: [
        "alpha_processing.c",
        "alpha_processing_sse2.c",
        "alpha_processing_sse41.c",
        "alpha_processing_neon.c",
        "cost.c",
        "cpu.c",
        "dec.c",
        "dec_clip_tables.c",
        "dec_sse2.c",
        "dec_sse41.c",
        "dec_neon.c",
        "filters.c",
        "filters_sse2.c",
        "filters_neon.c",
        "lossless.c",
        "lossless_sse2.c",
        "lossless_sse41.c",
        "lossless_neon.c",
        "rescaler.c",
        "rescaler_sse2.c",
        "rescaler_neon.c",
        "ssim.c",
        "ssim_sse2.c",
        "upsampling.c",
        "upsampling_sse2.c",
        "upsampling_sse41.c",
        "upsampling_neon.c",
        "yuv.c",
        "yuv_sse2.c",
        "yuv_sse41.c",
        "yuv_neon.c",
      ],
    },
    { dir: "ext/libwebp/src/sharpyuv", patterns: ["*.c"] },
    { dir: "ext/libwebp/src/utils", patterns: ["*.c"] },
  ],
};

const dav1d: LibDef = {
  name: "dav1d",
  alwaysOptimize: true,
  defines: ["_CRT_SECURE_NO_WARNINGS", "ARCH_X86_32=0", "ARCH_X86_64=1", "HAVE_ASM=0"],
  // NOTE: do NOT include ext/dav1d/include/compat/msvc — it shadows GCC's
  // native <stdatomic.h> with an MSVC-only version
  includes: ["ext/dav1d", "ext/dav1d/include"],
  files: [
    {
      dir: "ext/dav1d/src",
      patterns: [
        "lib.c",
        "thread_task.c",
        "cdf.c",
        "cpu.c",
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
        "picture.c",
        "qm.c",
        "ref.c",
        "refmvs.c",
        "scan.c",
        "tables.c",
        "warpmv.c",
        "wedge.c",
        "win32/thread.c",
        "sumatra_bitdepth_8.c",
        "sumatra_bitdepth_8_2.c",
        "sumatra_bitdepth_16.c",
        "sumatra_bitdepth_16_2.c",
      ],
    },
    // x86 C files — only cpu.c; skip msac_init.c and refmvs_init.c as they
    // reference asm symbols and we build with HAVE_ASM=0 (no NASM)
    {
      dir: "ext/dav1d/src/x86",
      patterns: ["cpu.c"],
    },
  ],
};

const libheif: LibDef = {
  name: "libheif",
  alwaysOptimize: true,
  rtti: true,
  exceptions: true,
  defines: ["_CRT_SECURE_NO_WARNINGS", "HAVE_DAV1D", "LIBHEIF_STATIC_BUILD"],
  includes: ["ext/libheif/libheif", "ext/libheif/libheif/api", "ext/dav1d/include"],
  files: [
    {
      dir: "ext/libheif/libheif",
      patterns: [
        "bitstream.*",
        "box.*",
        "brands.*",
        "common_utils.*",
        "context.*",
        "error.*",
        "file.*",
        "file_layout.*",
        "init.*",
        "logging.*",
        "nclx.*",
        "pixelimage.*",
        "plugin_registry.*",
        "region.*",
        "security_limits.*",
        "text.*",
      ],
    },
    {
      dir: "ext/libheif/libheif/image-items",
      patterns: [
        "avc.*",
        "avif.*",
        "grid.*",
        "hevc.*",
        "iden.*",
        "image_item.*",
        "jpeg.*",
        "jpeg2000.*",
        "mask_image.*",
        "overlay.*",
        "tiled.*",
        "vvc.*",
      ],
    },
    {
      dir: "ext/libheif/libheif/codecs",
      patterns: [
        "avif_boxes.*",
        "avif_dec.*",
        "avif_enc.*",
        "avc_boxes.*",
        "avc_dec.*",
        "avc_enc.*",
        "decoder.*",
        "encoder.*",
        "hevc_boxes.*",
        "hevc_dec.*",
        "hevc_enc.*",
        "jpeg_boxes.*",
        "jpeg_dec.*",
        "jpeg_enc.*",
        "jpeg2000_boxes.*",
        "jpeg2000_dec.*",
        "jpeg2000_enc.*",
        "vvc_boxes.*",
        "vvc_enc.*",
        "vvc_dec.*",
      ],
    },
    {
      dir: "ext/libheif/libheif/color-conversion",
      patterns: [
        "alpha.*",
        "chroma_sampling.*",
        "colorconversion.*",
        "hdr_sdr.*",
        "monochrome.*",
        "rgb2yuv.*",
        "rgb2yuv_sharp.*",
        "rgb2rgb.*",
        "yuv2rgb.*",
      ],
    },
    {
      dir: "ext/libheif/libheif/plugins",
      patterns: ["decoder_dav1d.*", "encoder_mask.*"],
    },
    {
      dir: "ext/libheif/libheif/sequences",
      patterns: ["chunk.*", "seq_boxes.*", "track.*", "track_metadata.*", "track_visual.*"],
    },
    {
      dir: "ext/libheif/libheif/api/libheif",
      patterns: [
        "heif.*",
        "heif_brands.*",
        "heif_color.*",
        "heif_context.*",
        "heif_decoding.*",
        "heif_encoding.*",
        "heif_image.*",
        "heif_image_handle.*",
        "heif_plugin.*",
        "heif_security.*",
        "heif_sequences.*",
        "heif_tai_timestamps.*",
      ],
    },
  ],
};

// mupdf-libs: combined library (libjpeg-turbo + jbig2dec + openjpeg + freetype
// + lcms2 + harfbuzz + mujs + gumbo + extract + brotli)
const mupdfLibs: LibDef = {
  name: "mupdf-libs",
  alwaysOptimize: true,
  defines: [
    "_CRT_SECURE_NO_WARNINGS",
    // jbig2dec
    "HAVE_STRING_H=1",
    "JBIG_NO_MEMENTO",
    // openjpeg
    "USE_JPIP",
    "OPJ_STATIC",
    "OPJ_EXPORTS",
    // freetype
    "FT2_BUILD_LIBRARY",
    'FT_CONFIG_MODULES_H="slimftmodules.h"',
    'FT_CONFIG_OPTIONS_H="slimftoptions.h"',
    "FT_CONFIG_OPTION_USE_BROTLI",
    // harfbuzz
    "HAVE_FALLBACK=1",
    "HAVE_OT",
    "HAVE_UCDN",
    "HAVE_FREETYPE",
    // libheif interop
    "LIBHEIF_STATIC_BUILD",
  ],
  includes: [
    "ext/libjpeg-turbo",
    "ext/libjpeg-turbo/simd",
    "ext/jbig2dec",
    "mupdf/scripts/freetype",
    "ext/freetype/include",
    "ext/lcms2/include",
    "ext/harfbuzz/src/hb-ucdn",
    "ext/mujs",
    "ext/gumbo-parser/include",
    "ext/gumbo-parser/visualc/include",
    "ext/extract/include",
    "ext/brotli/c/include",
    "ext/zlib",
  ],
  // harfbuzz allocator defines differ between debug and release
  debugExtraDefines: [
    "HAVE_ATEXIT",
    "hb_malloc_impl=sumatra_hb_malloc",
    "hb_calloc_impl=sumatra_hb_calloc",
    "hb_realloc_impl=sumatra_hb_realloc",
    "hb_free_impl=sumatra_hb_free",
  ],
  releaseExtraDefines: [
    "hb_malloc_impl=fz_hb_malloc",
    "hb_calloc_impl=fz_hb_calloc",
    "hb_realloc_impl=fz_hb_realloc",
    "hb_free_impl=fz_hb_free",
  ],
  files: [
    // ── libjpeg-turbo (skip .asm, use jsimd_none.c as SIMD fallback) ──
    {
      dir: "ext/libjpeg-turbo",
      patterns: [
        "jcomapi.c",
        "jdapimin.c",
        "jdapistd.c",
        "jdatadst.c",
        "jdatasrc.c",
        "jdcoefct.c",
        "jdcolor.c",
        "jddctmgr.c",
        "jdhuff.c",
        "jdinput.c",
        "jdmainct.c",
        "jdmarker.c",
        "jdmaster.c",
        "jdmerge.c",
        "jdpostct.c",
        "jdsample.c",
        "jdtrans.c",
        "jerror.c",
        "jfdctflt.c",
        "jfdctint.c",
        "jidctflt.c",
        "jidctfst.c",
        "jidctint.c",
        "jquant1.c",
        "jquant2.c",
        "jutils.c",
        "jmemmgr.c",
        "jmemnobs.c",
        "jaricom.c",
        "jdarith.c",
        "jfdctfst.c",
        "jdphuff.c",
        "jidctred.c",
        "jcapimin.c",
        "jcapistd.c",
        "jcarith.c",
        "jccoefct.c",
        "jccolor.c",
        "jcdctmgr.c",
        "jchuff.c",
        "jcinit.c",
        "jcmainct.c",
        "jcmarker.c",
        "jcmaster.c",
        "jcparam.c",
        "jcprepct.c",
        "jcsample.c",
        "jcphuff.c",
        "jsimd_none.c", // fallback: no SIMD (replaces .asm files)
      ],
    },
    // ── jbig2dec ──
    {
      dir: "ext/jbig2dec",
      patterns: [
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
      ],
    },
    // ── openjpeg ──
    {
      dir: "ext/openjpeg/src/lib/openjp2",
      patterns: [
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
      ],
    },
    // ── freetype ──
    {
      dir: "ext/freetype/src/base",
      patterns: [
        "ftbase.c",
        "ftbbox.c",
        "ftbitmap.c",
        "ftdebug.c",
        "ftfstype.c",
        "ftgasp.c",
        "ftglyph.c",
        "ftinit.c",
        "ftstroke.c",
        "ftsynth.c",
        "ftsystem.c",
        "fttype1.c",
        "ftotval.c",
      ],
    },
    {
      dir: "ext/freetype/src",
      patterns: [
        "cff/cff.c",
        "psaux/psaux.c",
        "pshinter/pshinter.c",
        "psnames/psnames.c",
        "raster/raster.c",
        "sfnt/sfnt.c",
        "smooth/smooth.c",
        "truetype/truetype.c",
        "type1/type1.c",
        "cid/type1cid.c",
      ],
    },
    // ── lcms2 ──
    { dir: "ext/lcms2/src", patterns: ["*.c"] },
    // ── harfbuzz (.cc files) ──
    {
      dir: "ext/harfbuzz/src",
      patterns: [
        "hb-aat-layout.cc",
        "hb-aat-map.cc",
        "hb-blob.cc",
        "hb-buffer.cc",
        "hb-buffer-verify.cc",
        "hb-buffer-serialize.cc",
        "hb-common.cc",
        "hb-face.cc",
        "hb-fallback-shape.cc",
        "hb-font.cc",
        "hb-ft.cc",
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
        "hb-ot-shape-fallback.cc",
        "hb-ot-shape-normalize.cc",
        "hb-ot-tag.cc",
        "hb-ot-var.cc",
        "hb-set.cc",
        "hb-shape.cc",
        "hb-shape-plan.cc",
        "hb-shaper.cc",
        "hb-static.cc",
        "hb-unicode.cc",
        "hb-subset-cff-common.cc",
        "hb-subset-cff1.cc",
        "hb-subset-cff2.cc",
        "hb-subset-input.cc",
        "hb-subset-plan.cc",
        "hb-subset.cc",
        "hb-ucd.cc",
      ],
    },
    // ── mujs ──
    { dir: "ext/mujs", patterns: ["one.c"] },
    // ── gumbo ──
    { dir: "ext/gumbo-parser/src", patterns: ["*.c"] },
    // ── extract ──
    {
      dir: "ext/extract/src",
      patterns: [
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
      ],
    },
    // ── brotli ──
    { dir: "ext/brotli/c/common", patterns: ["*.c"] },
    { dir: "ext/brotli/c/dec", patterns: ["*.c"] },
    { dir: "ext/brotli/c/enc", patterns: ["*.c"] },
  ],
};

// mupdf core library (mixed debug/release optimization)
const mupdf: LibDef = {
  name: "mupdf",
  alwaysOptimize: false,
  // deskew.c/skew.c use SSE4.1 intrinsics
  extraCflags: ["-msse4.1"],
  defines: [
    "USE_JPIP",
    "OPJ_EXPORTS",
    "HAVE_LCMS2MT=1",
    "OPJ_STATIC",
    "SHARE_JPEG",
    "TOFU_NOTO",
    "TOFU_CJK_LANG",
    "TOFU_NOTO_SUMATRA",
    "FZ_ENABLE_SVG=1",
    "FZ_ENABLE_BROTLI=1",
    "FZ_ENABLE_BARCODE=0",
    "FZ_ENABLE_JS=0",
    "FZ_ENABLE_HYPHEN=0",
  ],
  includes: [
    "mupdf/include",
    "mupdf/generated",
    "ext/jbig2dec",
    "ext/libjpeg-turbo",
    "ext/openjpeg/src/lib/openjp2",
    "mupdf/scripts/freetype",
    "ext/freetype/include",
    "ext/mujs",
    "ext/brotli/c/include",
    "ext/harfbuzz/src",
    "ext/lcms2/include",
    "ext/gumbo-parser/src",
    "ext/extract/include",
    "ext/zlib",
  ],
  files: [
    { dir: "ext", patterns: ["mupdf_load_system_font.c"] },
    { dir: "mupdf/source/cbz", patterns: ["mucbz.c", "muimg.c"] },
    {
      dir: "mupdf/source/fitz",
      patterns: [
        "archive.c",
        "bbox-device.c",
        "bidi.c",
        "bidi-std.c",
        "bitmap.c",
        "brotli.c",
        "buffer.c",
        "color-fast.c",
        "color-icc-create.c",
        "color-lcms.c",
        "colorspace.c",
        "compress.c",
        "compressed-buffer.c",
        "context.c",
        "crypt-aes.c",
        "crypt-arc4.c",
        "crypt-md5.c",
        "crypt-sha2.c",
        "deskew.c",
        "device.c",
        "directory.c",
        "document.c",
        "document-all.c",
        "draw-affine.c",
        "draw-blend.c",
        "draw-device.c",
        "draw-edge.c",
        "draw-edgebuffer.c",
        "draw-glyph.c",
        "draw-mesh.c",
        "draw-paint.c",
        "draw-path.c",
        "draw-rasterize.c",
        "draw-scale-simple.c",
        "draw-unpack.c",
        "encode-basic.c",
        "encode-fax.c",
        "encode-jpx.c",
        "encodings.c",
        "error.c",
        "filter-basic.c",
        "filter-brotli.c",
        "filter-dct.c",
        "filter-fax.c",
        "filter-flate.c",
        "filter-jbig2.c",
        "filter-leech.c",
        "filter-lzw.c",
        "filter-predict.c",
        "filter-sgi.c",
        "filter-thunder.c",
        "font.c",
        "ftoa.c",
        "geometry.c",
        "getopt.c",
        "glyph.c",
        "glyphbox.c",
        "gz-doc.c",
        "halftone.c",
        "harfbuzz.c",
        "hash.c",
        "heap.c",
        "hyphen.c",
        "image.c",
        "jmemcust.c",
        "json.c",
        "link.c",
        "list-device.c",
        "load-bmp.c",
        "load-gif.c",
        "load-jbig2.c",
        "load-jpeg.c",
        "load-jpx.c",
        "load-jxr-win.c",
        "load-png.c",
        "load-pnm.c",
        "load-psd.c",
        "load-tiff.c",
        "log.c",
        "memento.c",
        "memory.c",
        "noto.c",
        "outline.c",
        "output.c",
        "output-cbz.c",
        "output-csv.c",
        "output-docx.c",
        "output-jpeg.c",
        "output-pcl.c",
        "output-pclm.c",
        "output-pdfocr.c",
        "output-png.c",
        "output-pnm.c",
        "output-ps.c",
        "output-psd.c",
        "output-pwg.c",
        "output-svg.c",
        "path.c",
        "pixmap.c",
        "pool.c",
        "printf.c",
        "random.c",
        "skew.c",
        "separation.c",
        "shade.c",
        "stext-boxer.c",
        "stext-classify.c",
        "stext-device.c",
        "stext-iterator.c",
        "stext-output.c",
        "stext-para.c",
        "stext-search.c",
        "stext-table.c",
        "store.c",
        "stream-open.c",
        "stream-read.c",
        "string.c",
        "strtof.c",
        "svg-device.c",
        "test-device.c",
        "text.c",
        "text-decoder.c",
        "time.c",
        "trace-device.c",
        "track-usage.c",
        "transition.c",
        "tree.c",
        "ucdn.c",
        "uncfb.c",
        "unlibarchive.c",
        "subset-cff.c",
        "subset-ttf.c",
        "untar.c",
        "unzip.c",
        "util.c",
        "writer.c",
        "xml-write.c",
        "xml.c",
        "zip.c",
      ],
    },
    {
      dir: "mupdf/source/html",
      patterns: [
        "css-apply.c",
        "css-parse.c",
        "epub-doc.c",
        "html-doc.c",
        "html-font.c",
        "html-layout.c",
        "html-outline.c",
        "html-parse.c",
        "mobi.c",
        "office.c",
        "story-writer.c",
        "txt.c",
        "xml-dom.c",
      ],
    },
    {
      dir: "mupdf/source/pdf",
      patterns: [
        "pdf-af.c",
        "pdf-annot.c",
        "pdf-appearance.c",
        "pdf-clean.c",
        "pdf-clean-file.c",
        "pdf-cmap.c",
        "pdf-cmap-load.c",
        "pdf-cmap-parse.c",
        "pdf-colorspace.c",
        "pdf-crypt.c",
        "pdf-device.c",
        "pdf-event.c",
        "pdf-font.c",
        "pdf-font-add.c",
        "pdf-form.c",
        "pdf-function.c",
        "pdf-graft.c",
        "pdf-image.c",
        "pdf-image-rewriter.c",
        "pdf-interpret.c",
        "pdf-js.c",
        "pdf-label.c",
        "pdf-layer.c",
        "pdf-layout.c",
        "pdf-lex.c",
        "pdf-link.c",
        "pdf-metrics.c",
        "pdf-nametree.c",
        "pdf-object.c",
        "pdf-op-buffer.c",
        "pdf-op-filter.c",
        "pdf-op-run.c",
        "pdf-op-vectorize.c",
        "pdf-outline.c",
        "pdf-page.c",
        "pdf-parse.c",
        "pdf-pattern.c",
        "pdf-recolor.c",
        "pdf-repair.c",
        "pdf-resources.c",
        "pdf-run.c",
        "pdf-shade.c",
        "pdf-signature.c",
        "pdf-store.c",
        "pdf-stream.c",
        "pdf-subset.c",
        "pdf-type3.c",
        "pdf-unicode.c",
        "pdf-util.c",
        "pdf-write.c",
        "pdf-xobject.c",
        "pdf-xref.c",
        "pdf-zugferd.c",
      ],
    },
    {
      dir: "mupdf/source/svg",
      patterns: ["svg-color.c", "svg-doc.c", "svg-parse.c", "svg-run.c"],
    },
    {
      dir: "mupdf/source/xps",
      patterns: [
        "xps-common.c",
        "xps-doc.c",
        "xps-glyphs.c",
        "xps-gradient.c",
        "xps-image.c",
        "xps-link.c",
        "xps-outline.c",
        "xps-path.c",
        "xps-resource.c",
        "xps-tile.c",
        "xps-util.c",
        "xps-zip.c",
      ],
    },
    { dir: "mupdf/source/reflow", patterns: ["*.c"] },
  ],
};

// utils static library (mixed debug/release)
const utils: LibDef = {
  name: "utils",
  alwaysOptimize: false,
  defines: ["LIBHEIF_STATIC_BUILD"],
  includes: [
    "src",
    "ext/lzma/C",
    "ext/libheif/libheif/api",
    "ext/libwebp/src",
    "ext/dav1d/include",
    "ext/unarr",
    "mupdf/include",
    "ext/zlib",
  ],
  files: [
    {
      dir: "src/utils",
      patterns: [
        "AvifReader.*",
        "ApiHook.*",
        "Archive.*",
        "BaseUtil.*",
        "BitReader.*",
        "ByteOrderDecoder.*",
        "ByteReader.*",
        "ByteWriter.*",
        "CmdLineArgsIter.*",
        "ColorUtil.*",
        "CryptoUtil.*",
        "CssParser.*",
        "DbgHelpDyn.*",
        "Dict.*",
        "DirIter.*",
        "Dpi.*",
        "GeomUtil.*",
        "GuessFileType.*",
        "FileUtil.*",
        "FileWatcher.*",
        "FzImgReader.*",
        "GdiPlusUtil.*",
        "HtmlParserLookup.*",
        "HtmlPullParser.*",
        "HtmlPrettyPrint.*",
        "HttpUtil.*",
        "JsonParser.*",
        "Log.*",
        "LzmaSimpleArchive.*",
        "RegistryPaths.*",
        "SettingsUtil.*",
        "SquareTreeParser.*",
        "StrconvUtil.*",
        "StrFormat.*",
        "StrUtil.*",
        "StrVec.*",
        "StrQueue.*",
        "TempAllocator.*",
        "ThreadUtil.*",
        "TgaReader.*",
        "TrivialHtmlParser.*",
        "TxtParser.*",
        "UITask.*",
        "WebpReader.*",
        "WinDynCalls.*",
        "WinUtil.*",
        "ZipUtil.*",
      ],
    },
  ],
};

// Debug-only extra files for utils
const utilsDebugExtra: FileGroup[] = [{ dir: "src/utils", patterns: ["windrawlib.*"] }];

// SumatraPDF main executable sources (mixed debug/release)
// Combines: darkmodelib_files, synctex_files, mui_files, wingui_files,
// uia_files, engines_files, sumatrapdf_files
const sumatraFiles: FileGroup[] = [
  // darkmodelib
  {
    dir: "ext/darkmodelib/src",
    patterns: [
      "DarkModeSubclass.cpp",
      "DmlibColor.cpp",
      "DmlibDpi.cpp",
      "DmlibHook.cpp",
      "DmlibPaintHelper.cpp",
      "DmlibSubclass.cpp",
      "DmlibSubclassControl.cpp",
      "DmlibSubclassWindow.cpp",
      "DmlibWinApi.cpp",
    ],
  },
  // synctex
  {
    dir: "ext/synctex",
    patterns: ["synctex_parser_utils.c", "synctex_parser.c"],
  },
  // mui
  { dir: "src/mui", patterns: ["Mui.cpp", "TextRender.cpp"] },
  // wingui
  { dir: "src/wingui", patterns: ["*.cpp"] },
  // uia
  {
    dir: "src/uia",
    patterns: [
      "Provider.*",
      "StartPageProvider.*",
      "DocumentProvider.*",
      "PageProvider.*",
      "TextRange.*",
    ],
  },
  // engines
  {
    dir: "src",
    patterns: [
      "Annotation.*",
      "ChmFile.*",
      "DocProperties.*",
      "EngineBase.*",
      "EngineCreate.*",
      "EngineDjVu.*",
      "EngineEbook.*",
      "EngineImages.*",
      "EngineMupdf.*",
      "EngineMupdfImpl.*",
      "EnginePs.*",
      "EbookDoc.*",
      "EbookFormatter.*",
      "HtmlFormatter.*",
      "MobiDoc.*",
      "PdfCreator.*",
      "PalmDbReader.*",
    ],
  },
  // sumatrapdf main files
  {
    dir: "src",
    patterns: [
      "Accelerators.*",
      "Actions.*",
      "AppSettings.*",
      "AppTools.*",
      "Caption.*",
      "Canvas.*",
      "CanvasAboutUI.*",
      "ChmModel.*",
      "Commands.*",
      "CommandPalette.*",
      "CrashHandler.*",
      "DisplayModel.*",
      "DisplayMode.*",
      "EditAnnotations.*",
      "EngineDump.cpp",
      "ExternalViewers.*",
      "Favorites.*",
      "FileHistory.*",
      "FileThumbnails.*",
      "Flags.*",
      "FzImgReader.*",
      "GlobalPrefs.*",
      "HomePage.*",
      "Installer.*",
      "InstallerCommon.cpp",
      "MainWindow.*",
      "Menu.*",
      "Notifications.*",
      "PdfSync.*",
      "Print.*",
      "ProgressUpdateUI.*",
      "PreviewPipe.*",
      "RenderCache.*",
      "RegistryInstaller.*",
      "RegistryPreview.*",
      "RegistrySearchFilter.*",
      "SearchAndDDE.*",
      "Selection.*",
      "SettingsStructs.*",
      "SimpleBrowserWindow.*",
      "SumatraPDF.cpp",
      "SumatraStartup.cpp",
      "SumatraConfig.cpp",
      "SumatraDialogs.*",
      "SumatraProperties.*",
      "StressTesting.*",
      "SvgIcons.*",
      "TableOfContents.*",
      "Tabs.*",
      "Tester.*",
      "Tests.cpp",
      "TextSearch.*",
      "TextSelection.*",
      "Theme.*",
      "Toolbar.*",
      "Translations.*",
      "TranslationLangs.cpp",
      "UpdateCheck.*",
      "VirtWnd.*",
      "WindowTab.*",
      "Uninstaller.cpp",
    ],
  },
];

// Debug-only extra files for SumatraPDF
const sumatraDebugExtra: FileGroup[] = [
  { dir: "src/regress", patterns: ["Regress.*"] },
  { dir: "src", patterns: ["Scratch.*"] },
  {
    dir: "src/testcode",
    patterns: ["TestApp.cpp", "TestTab.cpp", "TestLayout.cpp"],
  },
  { dir: "src/utils/tests", patterns: ["*.cpp"] },
  { dir: "src/utils", patterns: ["UtAssert.*"] },
];

// ── Font files to embed (from premake fonts() function) ─────────────────────
const FONT_FILES = [
  { path: "mupdf/resources/fonts/urw/Dingbats.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-Regular.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusMonoPS-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-Regular.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusRoman-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-Regular.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/NimbusSans-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/urw/StandardSymbolsPS.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/droid/DroidSansFallbackFull.ttf", ext: "ttf" },
  { path: "mupdf/resources/fonts/sil/CharisSIL.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/sil/CharisSIL-Bold.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/sil/CharisSIL-Italic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/sil/CharisSIL-BoldItalic.cff", ext: "cff" },
  { path: "mupdf/resources/fonts/noto/NotoSans-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSansMath-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSansSymbols-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSansSymbols2-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoEmoji-Regular.ttf", ext: "ttf" },
  { path: "mupdf/resources/fonts/noto/NotoMusic-Regular.otf", ext: "otf" },
  { path: "mupdf/resources/fonts/noto/NotoSerif-Regular.otf", ext: "otf" },
];

// ── System libraries for final link ─────────────────────────────────────────
const SYSTEM_LIBS = [
  "comctl32",
  "gdiplus",
  "msimg32",
  "shlwapi",
  "urlmon",
  "version",
  "windowscodecs",
  "wininet",
  "uiautomationcore",
  "uxtheme",
  "wintrust",
  "crypt32",
  "advapi32",
  "kernel32",
  "user32",
  "gdi32",
  "comdlg32",
  "shell32",
  "ole32",
  "oleaut32",
  "winspool",
  "uuid",
  "shcore",
  "dwmapi",
  "powrprof",
  "wbemuuid",
];

// ── Build a single library ──────────────────────────────────────────────────

async function buildLibrary(
  lib: LibDef,
  outDir: string,
  isRelease: boolean,
): Promise<{ archive: string; objs: string[] }> {
  console.log(`Building ${lib.name}...`);

  const sources = await resolveSources(lib.files);
  if (sources.length === 0) {
    console.log(`  (no sources found for ${lib.name})`);
    return { archive: "", objs: [] };
  }
  console.log(`  ${sources.length} source files`);

  // Determine optimization flags
  let optFlags: string[];
  let configDefines: string[];

  if (lib.alwaysOptimize) {
    // optimized_conf: always -Os -DNDEBUG
    optFlags = ["-Os"];
    configDefines = ["NDEBUG"];
  } else if (isRelease) {
    optFlags = ["-Os"];
    configDefines = ["NDEBUG"];
  } else {
    optFlags = ["-O0", "-g"];
    configDefines = ["DEBUG"];
  }

  // Extra config-dependent defines (e.g. harfbuzz allocators)
  const extraDefs = isRelease ? (lib.releaseExtraDefines ?? []) : (lib.debugExtraDefines ?? []);

  // Build define flags
  const allDefines = [...COMMON_DEFINES, ...lib.defines, ...configDefines, ...extraDefs];
  const defineFlags = allDefines.map((d) => `-D${d}`);

  // Build include flags
  const includeFlags = lib.includes.map((d) => `-I${d}`);

  // Suppress warnings; GCC 14+ promotes some to errors even with -w
  const warnFlags = [
    "-w",
    "-Wno-incompatible-pointer-types",
    "-Wno-int-conversion",
    "-Wno-implicit-function-declaration",
  ];

  // Prepare compile units
  const units: { src: string; obj: string; args: string[] }[] = [];
  for (const src of sources) {
    const ext = extname(src).slice(1).toLowerCase();
    const isCpp = ext === "cpp" || ext === "cc";
    const compiler = isCpp ? CXX : CC;

    const langFlags: string[] = [];
    if (isCpp) {
      langFlags.push("-std=c++23");
      if (!lib.rtti) langFlags.push("-fno-rtti");
      if (!lib.exceptions) langFlags.push("-fno-exceptions");
    }

    const obj = objPath(outDir, lib.name, src);
    units.push({
      src,
      obj,
      args: [compiler, ...optFlags, ...defineFlags, ...includeFlags, ...warnFlags, ...langFlags, ...(lib.extraCflags ?? []), "-c", src, "-o", obj],
    });
  }

  await compileAll(units, JOBS);

  const objs = units.map((u) => u.obj);
  const archivePath = join(outDir, "lib", `lib${lib.name}.a`);
  await createArchive(archivePath, objs);
  console.log(`  -> ${archivePath}`);
  return { archive: archivePath, objs };
}

// ── Build SumatraPDF main executable sources ────────────────────────────────

async function buildSumatraExe(
  outDir: string,
  isRelease: boolean,
  archives: string[],
): Promise<void> {
  console.log("Building SumatraPDF executable sources...");

  // Resolve source files
  const groups = [...sumatraFiles];
  if (!isRelease) {
    groups.push(...sumatraDebugExtra);
  }
  const sources = await resolveSources(groups);
  console.log(`  ${sources.length} source files`);

  // Config flags
  const optFlags = isRelease ? ["-Os"] : ["-O0", "-g"];
  const configDefines = isRelease ? ["NDEBUG"] : ["DEBUG"];

  const allDefines = [
    ...COMMON_DEFINES,
    ...configDefines,
    "_CRT_SECURE_NO_WARNINGS",
    "DISABLE_DOCUMENT_RESTRICTIONS",
    "_DARKMODELIB_NO_INI_CONFIG",
    "LIBHEIF_STATIC_BUILD",
    "UNICODE",
    "_UNICODE",
    "_USE_MATH_DEFINES",
  ];
  const defineFlags = allDefines.map((d) => `-D${d}`);

  const includes = [
    "src",
    "mupdf/include",
    "ext/synctex",
    "ext/libdjvu",
    "ext/CHMLib",
    "ext/darkmodelib/include",
    "ext/zlib",
    // WebView2 - may not exist for mingw builds
    "packages/Microsoft.Web.WebView2.1.0.992.28/build/native/include",
  ];
  const includeFlags = includes.map((d) => `-I${d}`);

  // Use -Wno-* instead of -w to still see important errors
  const warnFlags = ["-w"];

  const units: { src: string; obj: string; args: string[] }[] = [];
  for (const src of sources) {
    const ext = extname(src).slice(1).toLowerCase();
    const isCpp = ext === "cpp" || ext === "cc";
    const compiler = isCpp ? CXX : CC;

    const langFlags: string[] = [];
    if (isCpp) {
      // -fpermissive: HWND-to-LONG casts, GDI+ overloads, etc.
      langFlags.push("-std=c++23", "-fno-rtti", "-fno-exceptions", "-fpermissive");
    }

    const obj = objPath(outDir, "sumatrapdf", src);
    units.push({
      src,
      obj,
      args: [compiler, ...optFlags, ...defineFlags, ...includeFlags, ...warnFlags, ...langFlags, "-c", src, "-o", obj],
    });
  }

  await compileAll(units, JOBS);
  const exeObjs = units.map((u) => u.obj);

  // ── Compile _com_util stub (mingw doesn't ship comsuppw.lib) ─────────
  const comUtilSrc = join(outDir, "obj", "_com_util_stub.cpp");
  const comUtilObj = join(outDir, "obj", "_com_util_stub.o");
  await writeFile(comUtilSrc, `
#include <windows.h>
#include <oleauto.h>
namespace _com_util {
  BSTR WINAPI ConvertStringToBSTR(const char *pSrc) {
    if (!pSrc) return nullptr;
    int len = MultiByteToWideChar(CP_ACP, 0, pSrc, -1, nullptr, 0);
    BSTR bstr = SysAllocStringLen(nullptr, len - 1);
    if (bstr) MultiByteToWideChar(CP_ACP, 0, pSrc, -1, bstr, len);
    return bstr;
  }
  char *WINAPI ConvertBSTRToString(BSTR pSrc) {
    if (!pSrc) return nullptr;
    int len = WideCharToMultiByte(CP_ACP, 0, pSrc, -1, nullptr, 0, nullptr, nullptr);
    char *str = new char[len];
    WideCharToMultiByte(CP_ACP, 0, pSrc, -1, str, len, nullptr, nullptr);
    return str;
  }
}
`);
  const comRes = await spawnCmd([CXX, "-Os", "-c", comUtilSrc, "-o", comUtilObj]);
  if (!comRes.ok) {
    console.error(`Failed to compile _com_util stub: ${comRes.stderr}`);
  }
  exeObjs.push(comUtilObj);

  // ── Embed font files ──────────────────────────────────────────────────
  console.log("Embedding font files...");
  const fontObjs: string[] = [];
  for (const font of FONT_FILES) {
    if (!existsSync(font.path)) {
      console.error(`  WARNING: font not found: ${font.path}`);
      continue;
    }
    const base = basename(font.path, `.${font.ext}`).replace(/-/g, "_");
    const sym = `_binary_${base}_${font.ext}`;
    const obj = join(outDir, "obj", "fonts", `${base}.o`);
    await embedBinaryFile(font.path, obj, sym);
    fontObjs.push(obj);
  }

  // ── Compile .rc resource file ─────────────────────────────────────────
  console.log("Compiling resources...");
  const rcObj = join(outDir, "obj", "sumatrapdf", "SumatraPDF.res.o");
  mkdirSync(dirname(rcObj), { recursive: true });
  const rcObjAbsolute = join(process.cwd(), rcObj);
  // Create a modified .rc with forward slashes (macOS windres can't handle backslashes)
  const rcOriginal = await readFile("src/SumatraPDF.rc", "utf-8");
  const rcFixed = rcOriginal.replace(/\\\\/g, "/");
  const rcTmpPath = join(outDir, "obj", "sumatrapdf", "SumatraPDF_mingw.rc");
  await writeFile(rcTmpPath, rcFixed);
  const rcTmpAbsolute = join(process.cwd(), rcTmpPath);
  const rcRes = await spawnCmd([
    WINDRES,
    "-I",
    ".",
    "-D_WIN64",
    ...defineFlags,
    rcTmpAbsolute,
    "-o",
    rcObjAbsolute,
  ], { cwd: "src" });
  const rcObjs: string[] = [];
  if (rcRes.ok) {
    rcObjs.push(rcObj);
    console.log("  -> resources compiled");
  } else {
    console.error(`  WARNING: windres failed (resource file skipped): ${rcRes.stderr.slice(0, 200)}`);
  }

  // ── Link ──────────────────────────────────────────────────────────────
  console.log("Linking SumatraPDF.exe...");
  const exePath = join(outDir, "SumatraPDF.exe");
  const linkArgs = [
    CXX,
    "-o",
    exePath,
    "-static",
    "-static-libgcc",
    "-static-libstdc++",
    "-mwindows", // GUI app (WinMain entry point)
    ...exeObjs,
    ...rcObjs,
    ...fontObjs,
    // archives: order matters (dependents first)
    ...archives,
    // WebView2 import library (MSVC import lib, mingw can usually consume these)
    "packages/Microsoft.Web.WebView2.1.0.992.28/build/native/x64/WebView2Loader.dll.lib",
    // system libraries
    ...SYSTEM_LIBS.map((l) => `-l${l}`),
  ];
  const linkRes = await spawnCmd(linkArgs);
  if (!linkRes.ok) {
    console.error("Link failed:");
    console.error(linkRes.stderr.slice(0, 3000));
    throw new Error("Linking failed");
  }
  console.log(`\n  => ${exePath}`);
}

// ── Top-level build functions ───────────────────────────────────────────────

// Order: libraries that have no deps first, then dependents.
// The link order for archives is: most-dependent first, least-dependent last.
const ALL_LIBS: LibDef[] = [zlib, unrar, libdjvu, chm, unarrlib, libwebp, dav1d, libheif, mupdfLibs, mupdf, utils];

async function build(isRelease: boolean, clean: boolean): Promise<void> {
  const config = isRelease ? "release" : "debug";
  const outDir = isRelease ? "out/mingw-rel64" : "out/mingw-dbg64";

  if (clean && existsSync(outDir)) {
    console.log(`Cleaning ${outDir}...`);
    rmSync(outDir, { recursive: true, force: true });
  }

  const startTime = performance.now();

  console.log(`\n=== Building SumatraPDF (${config}, x64, mingw) ===\n`);
  console.log(`Output: ${outDir}`);
  console.log(`Parallel jobs: ${JOBS}\n`);

  mkdirSync(join(outDir, "obj"), { recursive: true });
  mkdirSync(join(outDir, "lib"), { recursive: true });

  // Build all static libraries
  const archives: string[] = [];
  for (const lib of ALL_LIBS) {
    // For utils, add debug extra files
    const libCopy = { ...lib };
    if (lib.name === "utils" && !isRelease) {
      libCopy.files = [...lib.files, ...utilsDebugExtra];
    }
    const result = await buildLibrary(libCopy, outDir, isRelease);
    if (result.archive) archives.push(result.archive);
  }

  // Build and link the SumatraPDF executable
  // Link order: exe objects first, then archives in dependency order
  // (most-dependent = SumatraPDF sources, least-dependent = zlib)
  // archives are already in order: zlib, unrar, ..., utils
  // For the linker, reverse so that dependents come before dependencies
  const linkArchives = [...archives].reverse();
  await buildSumatraExe(outDir, isRelease, linkArchives);

  const elapsed = ((performance.now() - startTime) / 1000).toFixed(1);
  console.log(`\n=== Build complete (${config}) in ${elapsed}s ===\n`);
}

async function build_debug(clean: boolean): Promise<void> {
  await build(false, clean);
}

async function build_release(clean: boolean): Promise<void> {
  await build(true, clean);
}

// ── Main ────────────────────────────────────────────────────────────────────

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
    console.error("Usage: bun cmd/build-with-mingw.ts [-debug] [-release] [-clean]");
    process.exit(1);
  }
}

if (!doDebug && !doRelease) {
  console.error("Usage: bun cmd/build-with-mingw.ts [-debug] [-release] [-clean]");
  console.error("  -debug    Build Debug x64 configuration");
  console.error("  -release  Build Release x64 configuration");
  console.error("  -clean    Delete output directory before building");
  process.exit(1);
}

(async () => {
  try {
    if (doDebug) await build_debug(doClean);
    if (doRelease) await build_release(doClean);
  } catch (e: any) {
    console.error(`\nBuild failed: ${e.message}`);
    process.exit(1);
  }
})();
