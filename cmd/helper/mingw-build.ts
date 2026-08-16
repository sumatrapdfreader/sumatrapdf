/**
 * Build SumatraPDF with mingw-w64 cross-compiler.
 * Invoked by cmd/build.ts -mingw.
 *
 * Replicates build logic from premake5.lua / premake5.files.lua
 * for x64 targets using x86_64-w64-mingw32-g++.
 *
 * NOTE: .asm (NASM) files are skipped; C fallbacks are used instead.
 * NOTE: Font embedding (.cff/.ttf/.otf) uses objcopy.
 * NOTE: WebView2 is not used for mingw builds. WebView.cpp provides stub
 * implementations; CHM and the in-app manual fall back to IE / online docs.
 */

import { mkdirSync, existsSync, rmSync } from "node:fs";
import { readFile, writeFile } from "node:fs/promises";
import { join, extname, dirname, basename } from "node:path";
import {
  type BuildTools,
  type FileGroup,
  type LibDef,
  DEFAULT_JOBS,
  FONT_FILES,
  buildLibrary,
  compileAll,
  embedBinaryFile,
  objPath,
  resolveSources,
  spawnCmd,
} from "../deps-build-common";
import {
  zlib,
  unrar,
  chmdec,
  zopfli,
  libarchive,
  libwebp,
  dav1d,
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
  aGumbo,
  mupdf,
} from "../deps-build-defs";

export interface MingwTools {
  cc: string;
  cxx: string;
  ar: string;
  windres: string;
  objcopy: string;
}

const DEFAULT_MINGW_TOOLS: MingwTools = {
  cc: "x86_64-w64-mingw32-gcc",
  cxx: "x86_64-w64-mingw32-g++",
  ar: "x86_64-w64-mingw32-ar",
  windres: "x86_64-w64-mingw32-windres",
  objcopy: "x86_64-w64-mingw32-objcopy",
};

let mingwTools: MingwTools = { ...DEFAULT_MINGW_TOOLS };

export function setMingwTools(tools: Partial<MingwTools>): void {
  mingwTools = { ...DEFAULT_MINGW_TOOLS, ...tools };
}

function mingwBuildTools(): BuildTools {
  return {
    cc: mingwTools.cc,
    cxx: mingwTools.cxx,
    ar: mingwTools.ar,
    embed: mingwTools.objcopy,
  };
}

let compileJobs = DEFAULT_JOBS;

const COMMON_DEFINES = ["WIN32", "_WIN32", "WINVER=0x0605", "_WIN32_WINNT=0x0603"];
const MINGW_CXX_FLAGS = ["-D__GXX_TYPEINFO_EQUALITY_INLINE=1", "-Wno-narrowing"];

// utils static library (mixed debug/release)
const utils: LibDef = {
  name: "base",
  alwaysOptimize: false,
  defines: ["LIBARCHIVE_STATIC"],
  includes: [
    "src",
    "ext/lzma/C",
    "ext/heicdec",
    "ext/libwebp/src",
    "ext/dav1d/include",
    "ext/jxldec",
    "ext/mupdf/include",
    "ext/a-zlib",
    "ext/libarchive",
  ],
  files: [
    {
      dir: "src/base",
      patterns: [
        "ApiHook.*",
        "Archive.*",
        "Arena.*",
        "Arena_win.cpp",
        "Base.h",
        "Base.cpp",
        "Base_win.cpp",
        "ByteReaderWriter.*",
        "CmdLineArgsIter.h",
        "CmdLineArgsIter.cpp",
        "CmdLineArgsIter_win.cpp",
        "Color.*",
        "Crypto.h",
        "Crypto_win.cpp",
        "CssParser.*",
        "DbgHelpDyn.h",
        "DbgHelpDyn_win.cpp",
        "Dict.*",
        "DirScan.h",
        "DirScan.cpp",
        "DirScan_win.cpp",
        "Exif.*",
        "Geom.*",
        "GuessFileType.*",
        "GuessFileTypeFromFile.cpp",
        "File.h",
        "File.cpp",
        "File_win.cpp",
        "FileWatcher.*",
        "GdiPlusUtil.cpp",
        "HtmlTags.*",
        "HtmlPrettyPrint.*",
        "Http.h",
        "Http.cpp",
        "Http_win.cpp",
        "JsonParser.*",
        "Log.*",
        "LzmaSimpleArchive.*",
        "Pixmap.*",
        "Pixmap_win.cpp",
        "RegistryPaths.*",
        "SettingsUtil.*",
        "SquareTreeParser.*",
        "Strconv.*",
        "StrFormat.*",
        "StrFormatParse.*",
        "Str.*",
        "StrUtf8.*",
        "StrVec.*",
        "StrQueue.*",
        "TempAllocator.*",
        "Thread.*",
        "TgaReader.*",
        "TgaReader_win.cpp",
        "TxtParser.*",
        "UITask.*",
        "WinDynCalls.h",
        "WinDynCalls_win.cpp",
        "Win.*",
        "Zip.*",
      ],
    },
    {
      dir: "src/gui",
      patterns: ["Dpi_win.cpp"],
    },
    // LzSA decoder (LzmaDecode + x86 BCJ). Bra.c not needed.
    {
      dir: "ext/lzma/C",
      patterns: ["LzmaDec.c", "Bra86.c"],
    },
  ],
};

// Debug-only extra files for utils
const utilsDebugExtra: FileGroup[] = [];

// SumatraPDF main executable sources (mixed debug/release)
// Combines: darkmodelib_files, synctex_files, mui_files, gui_files,
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
  // gui (portable + Windows). Dpi_win.cpp lives in src/gui but is compiled
  // into the base/utils lib because Win.cpp calls it.
  {
    dir: "src/gui",
    patterns: [
      "Gfx_win.cpp",
      "GfxGdiplus_win.cpp",
      "GfxDirect2D_win.cpp",
      "CommandPaletteModel.cpp",
      "GuiColors.cpp",
      "Layout.cpp",
      "Layout_win.cpp",
      "PlatformFont.cpp",
      "PlatformFont_win.cpp",
      "PlatformText.cpp",
      "PlatformText_win.cpp",
      "UIModels.cpp",
      "VirtCtrl.cpp",
      "VirtHost_win.cpp",
      "UiPlatform_win.cpp",
    ],
  },
  { dir: "src/gui/win", patterns: ["*.cpp"] },
  // uia
  {
    dir: "src/uia",
    patterns: ["Provider.*", "StartPageProvider.*", "DocumentProvider.*", "PageProvider.*", "TextRange.*"],
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
      "EngineDjvuDec.*",
      "EngineEbook.*",
      "EngineImages.*",
      "EngineMupdf.*",
      "EngineMupdfImpl.*",
      "EnginePs.*",
      "EbookDoc.*",
      "EbookFormatter.*",
      "GumboHtmlParser.*",
      "HtmlFormatter.*",
      "MobiDoc.*",
      "PdfCadDetect.*",
      "PdfCadEnhanceDevice.*",
      "PdfDarkModeAnalysis.cpp",
      "PdfDarkModeCache.cpp",
      "PdfDarkModeColor.cpp",
      "PdfDarkModeDevice.cpp",
      "PdfDarkModeEngineCache.cpp",
      "PdfDarkModeImageBgBlend.cpp",
      "PdfDarkModeImageClassifier.cpp",
      "PdfDarkModeImageRules.cpp",
      "PdfDarkModeImageStats.cpp",
      "PdfDarkModeOklab.cpp",
      "PdfDarkModeProfile.cpp",
      "PdfDarkModeScanProcess.cpp",
      "PdfCreator.*",
      "PalmDbReader.*",
    ],
  },
  // sumatrapdf main files
  {
    dir: "src",
    patterns: [
      "Accelerators.*",
      "ShortcutParse.*",
      "Actions.*",
      "AvifReader.*",
      "DarkMode_win.*",
      "AppSettings.*",
      "AppTools.*",
      "Caption.*",
      "Canvas.*",
      "CanvasAboutUI.*",
      "ChmDump.*",
      "ChmModel.*",
      "AdvancedSettingsDialog.*",
      "AIChatCommon.*",
      "AppUnitTests.*",
      "AIChatPanel.*",
      "AIAntiGravity.*",
      "AICodexBuild.*",
      "AIGrokBuild.*",
      "KeyboardHelp.*",
      "KeyboardHelp_win.cpp",
      "CaptionGlyphs.*",
      "AddFavoriteDialog.*",
      "ChangeColorDialog.*",
      "ChangeLanguageDialog.*",
      "ChangeScrollbarDialog.*",
      "ChangeThemeDialog.*",
      "CustomZoomDialog.*",
      "EbookSettingsDialog.*",
      "GetPasswordDialog.*",
      "GoToPageDialog.*",
      "InverseSearchDialog.*",
      "SettingsDialog.*",
      "AIClaudeCode.*",
      "CommandAvailability.*",
      "ExifDump.*",
      "FilterHighlightDraw.*",
      "FindBar.*",
      "FindWindow.*",
      "MarkdownModel.*",
      "MarkdownToc.*",
      "NavFilesInFolder.*",
      "SelectionTranslate.*",
      "TreeModel.*",
      "GrokBuild.*",
      "CodexBuild.*",
      "Commands.*",
      "CommandPalette.*",
      "CommandPaletteCollect.*",
      "CommandPaletteDraw.*",
      "CommandPaletteFilter.*",
      "FilterUtil.*",
      "WebpReader.*",
      "CrashHandler.*",
      "DisplayModel.*",
      "DocumentLayout.*",
      "DisplayMode.*",
      "PageRenderPolicy.*",
      "PageRenderService.*",
      "ReaderModel.*",
      "EditAnnotations.*",
      "EmbeddedResources.*",
      "EngineDump.cpp",
      "ExternalViewers.*",
      "ImageEditHostSumatra.*",
      "ImageSaveCropResize.*",
      "Favorites.*",
      "FileHistory.*",
      "FileThumbnails.*",
      "Flags.*",
      "ImageReader.h",
      "ImageReader.cpp",
      "ImageReader_win.cpp",
      "GlobalPrefs.*",
      "GumboHelpers.*",
      "HangDetector.*",
      "HomePage.*",
      "Installer.*",
      "InstallerCommon.cpp",
      "JxlReader.*",
      "LinkFollow.*",
      "MainWindow.*",
      "Menu.*",
      "Notifications.*",
      "PdfSync.*",
      "PdfTools.*",
      "PngOptimizer.*",
      "Print.*",
      "ProgressUpdateUI.*",
      "PreviewPipe.*",
      "ReadAloudHighlight.*",
      "ReadAloudPlaybackBar.*",
      "RefHover.*",
      "RefHoverCanvas.*",
      "RefHoverDetect.*",
      "RefHoverInternal.*",
      "RefHoverPopup.*",
      "RefHoverRender.*",
      "RefHoverShow.*",
      "RefHoverText.*",
      "RefHoverTextDetect.*",
      "FormFields.*",
      "OverlayScrollbar.*",
      "RenderCache.*",
      "RegistryInstaller.*",
      "RegistryPreview.*",
      "RegistrySearchFilter.*",
      "SearchAndDDE.*",
      "Screenshot.*",
      "ScreenshotCapture.*",
      "Selection.*",
      "SelectionHandlers.*",
      "SelectionToolbar.*",
      "SelectTextKeyboard.*",
      "SettingsStructs.*",
      "SimpleBrowserWindow.*",
      "SumatraControl.*",
      "SumatraLog.cpp",
      "SumatraPDF.cpp",
      "SumatraStartup.cpp",
      "SumatraConfig.cpp",
      "SumatraDialogs.*",
      "SumatraProperties.*",
      "StressTesting.*",
      "SvgIcons.*",
      "TableOfContents.*",
      "Tabs.*",
      "TabGroupsManage.*",
      "Tester.*",
      "Tests.cpp",
      "TextSearch.*",
      "TextSelection.*",
      "TextViewWnd.*",
      "Theme.*",
      "Theme_win.*",
      "TipText.*",
      "Toolbar.*",
      "Toolbar_win.*",
      "Translations.*",
      "TranslationLangs.cpp",
      "UpdateCheck.*",
      "VirtCtrl.*",
      "WindowTab.*",
      "Uninstaller.cpp",
    ],
  },
];

// Debug-only extra files for SumatraPDF
const sumatraDebugExtra: FileGroup[] = [
  { dir: "src", patterns: ["SumatraTest.*"] },
  { dir: "src/regress", patterns: ["Regress.*"] },
  { dir: "src", patterns: ["Scratch.*"] },
  {
    dir: "src/testcode",
    patterns: ["TestApp.cpp", "TestTab.cpp", "TestLayout.cpp"],
  },
  // note: src/base/tests/*.cpp omitted for mingw (not essential, may pull extra headers)
  { dir: "src/base", patterns: ["UtAssert.*"] },
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
  "winhttp", // HttpPostUrl (Http_win.cpp); not pulled via #pragma on mingw
  "uiautomationcore",
  "uxtheme",
  "wintrust",
  "crypt32",
  "ncrypt",
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
  // smooth-scroll timer resolution (timeBeginPeriod / timeEndPeriod in Canvas.cpp)
  "winmm",
];

// ── Build SumatraPDF main executable sources ────────────────────────────────

async function buildSumatraExe(outDir: string, isRelease: boolean, archives: string[]): Promise<void> {
  console.log("Building SumatraPDF executable sources...");

  // Resolve source files
  const groups = [...sumatraFiles];
  if (!isRelease) {
    groups.push(...sumatraDebugExtra);
  }
  const sources = await resolveSources(groups);
  console.log(`  ${sources.length} source files`);

  // pre-create obj dir for exe objs (parallel fs safety)
  mkdirSync(join(outDir, "obj", "sumatrapdf"), { recursive: true });

  // Config flags
  const optFlags = isRelease ? ["-Os"] : ["-O0", "-g"];
  const configDefines = isRelease ? ["NDEBUG"] : ["DEBUG"];

  const allDefines = [
    ...COMMON_DEFINES,
    ...configDefines,
    "_CRT_SECURE_NO_WARNINGS",
    "DISABLE_DOCUMENT_RESTRICTIONS",
    "_DARKMODELIB_NO_INI_CONFIG",
    "LIBARCHIVE_STATIC",
    "UNICODE",
    "_UNICODE",
    "_USE_MATH_DEFINES",
    "CMARK_GFM_STATIC_DEFINE",
  ];
  const defineFlags = allDefines.map((d) => `-D${d}`);

  const includes = [
    "src",
    "ext/mupdf/include",
    "ext/synctex",
    "ext/djvudec",
    "ext/chmdec",
    "ext/a-zopfli",
    "ext/darkmodelib/include",
    "ext/heicdec",
    "ext/libwebp/src",
    "ext/jxldec",
    "ext/a-zlib",
    "ext/libarchive",
    "ext/cmark-gfm/src",
    "ext/cmark-gfm/extensions",
    "ext/mupdf/scripts/cmark-gfm",
  ];
  const includeFlags = includes.map((d) => `-I${d}`);

  // Use -Wno-* instead of -w to still see important errors
  const warnFlags = ["-w"];

  const units: { src: string; obj: string; args: string[] }[] = [];
  for (const src of sources) {
    const ext = extname(src).slice(1).toLowerCase();
    const isCpp = ext === "cpp" || ext === "cc";
    const compiler = isCpp ? mingwTools.cxx : mingwTools.cc;

    const langFlags: string[] = [];
    if (isCpp) {
      // -fpermissive: HWND-to-LONG casts, GDI+ overloads, etc.
      langFlags.push("-std=c++23", ...MINGW_CXX_FLAGS, "-fno-rtti", "-fno-exceptions", "-fpermissive");
    }

    const obj = objPath(outDir, "sumatrapdf", src);
    units.push({
      src,
      obj,
      args: [compiler, ...optFlags, ...defineFlags, ...includeFlags, ...warnFlags, ...langFlags, "-c", src, "-o", obj],
    });
  }

  await compileAll(units, compileJobs);
  const exeObjs = units.map((u) => u.obj);

  // ── Compile _com_util stub (mingw doesn't ship comsuppw.lib) ─────────
  const comUtilSrc = join(outDir, "obj", "_com_util_stub.cpp");
  const comUtilObj = join(outDir, "obj", "_com_util_stub.o");
  await writeFile(
    comUtilSrc,
    `
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
`,
  );
  const comRes = await spawnCmd([mingwTools.cxx, "-Os", ...MINGW_CXX_FLAGS, "-c", comUtilSrc, "-o", comUtilObj]);
  if (!comRes.ok) {
    console.error(`Failed to compile _com_util stub: ${comRes.stderr}`);
  }
  exeObjs.push(comUtilObj);

  // ── TextToSpeech stub (WinRT headers unavailable for mingw cross-compile) ──
  const ttsStubSrc = join(outDir, "obj", "_tts_stub.cpp");
  const ttsStubObj = join(outDir, "obj", "_tts_stub.o");
  await writeFile(
    ttsStubSrc,
    `
#include "base/Base.h"
#include "TextToSpeech.h"

bool TtsSpeakUtf8(Str) { return false; }
void TtsStop() {}
void TtsRelease() {}
bool TtsIsSpeaking() { return false; }
int TtsGetSpokenPosUtf8() { return -1; }
void TtsSetNotifyWindow(HWND, UINT, WPARAM, LPARAM) {}
void TtsProcessEvents() {}
Vec<TtsVoiceInfo> TtsGetVoices() { return Vec<TtsVoiceInfo>(); }
void TtsFreeVoices(Vec<TtsVoiceInfo>&) {}
bool TtsSetVoiceById(Str) { return false; }
Str TtsGetVoiceId() { return Str(); }
void TtsSetSpeed(float) {}
float TtsGetSpeed() { return 1.0f; }
`,
  );
  const ttsRes = await spawnCmd([
    mingwTools.cxx,
    "-Os",
    ...MINGW_CXX_FLAGS,
    "-Isrc",
    "-c",
    ttsStubSrc,
    "-o",
    ttsStubObj,
  ]);
  if (!ttsRes.ok) {
    console.error(`Failed to compile TextToSpeech stub: ${ttsRes.stderr}`);
    throw new Error("TextToSpeech stub compile failed");
  }
  exeObjs.push(ttsStubObj);

  // ── debug test stubs (TestPlugin/TestPreview don't build cleanly with mingw GDI+) ──
  const testStubSrc = join(outDir, "obj", "_test_stub.cpp");
  const testStubObj = join(outDir, "obj", "_test_stub.o");
  await writeFile(
    testStubSrc,
    `
#include "base/Base.h"
void TestPlugin(WStr) {}
void TestPreview(WStr) {}
`,
  );
  const testRes = await spawnCmd([
    mingwTools.cxx,
    "-Os",
    ...MINGW_CXX_FLAGS,
    "-Isrc",
    "-c",
    testStubSrc,
    "-o",
    testStubObj,
  ]);
  if (!testRes.ok) {
    console.error(`Failed to compile test stub: ${testRes.stderr}`);
    throw new Error("test stub compile failed");
  }
  exeObjs.push(testStubObj);

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
    await embedBinaryFile(mingwBuildTools(), "pe", font.path, obj, sym);
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
  const rcRes = await spawnCmd(
    [mingwTools.windres, "-I", ".", "-D_WIN64", ...defineFlags, rcTmpAbsolute, "-o", rcObjAbsolute],
    { cwd: "src" },
  );
  const rcObjs: string[] = [];
  if (rcRes.ok) {
    rcObjs.push(rcObj);
    console.log("  -> resources compiled");
  } else {
    console.error(`  WARNING: windres failed (resource file skipped): ${rcRes.stderr.slice(0, 200)}`);
  }

  // ── uiautomationcore import lib (missing from mingw-w64 < 12) ─────────
  // mingw-w64 v11 (Ubuntu 24.04) has the UIA headers but not the import
  // library; synthesize one from a .def for the few functions we call.
  const probe = await spawnCmd([mingwTools.cxx, "-print-file-name=libuiautomationcore.a"], {
    captureStdout: true,
  });
  const extraLibDirs: string[] = [];
  if (probe.stdout.trim() === "libuiautomationcore.a") {
    console.log("Generating uiautomationcore import library (not in this mingw-w64)...");
    const defPath = join(outDir, "obj", "uiautomationcore.def");
    await writeFile(
      defPath,
      [
        "LIBRARY UIAutomationCore.dll",
        "EXPORTS",
        "UiaGetReservedNotSupportedValue",
        "UiaHostProviderFromHwnd",
        "UiaRaiseAutomationEvent",
        "UiaRaiseStructureChangedEvent",
        "UiaReturnRawElementProvider",
        "UiaClientsAreListening",
        "UiaDisconnectProvider",
        "",
      ].join("\n"),
    );
    const dlltool = mingwTools.cxx.replace(/g\+\+.*$/, "dlltool");
    const libPath = join(outDir, "obj", "libuiautomationcore.a");
    const dtRes = await spawnCmd([dlltool, "-d", defPath, "-l", libPath]);
    if (!dtRes.ok) {
      throw new Error(`dlltool failed: ${dtRes.stderr}`);
    }
    extraLibDirs.push(`-L${join(outDir, "obj")}`);
  }

  // ── Link ──────────────────────────────────────────────────────────────
  console.log("Linking SumatraPDF.exe...");
  const exePath = join(outDir, "SumatraPDF.exe");

  // use response file to avoid excessive command-line length with hundreds of .o files
  const linkObjs = [...exeObjs, ...rcObjs, ...fontObjs, ...archives];
  const rspPath = join(outDir, "obj", "sumatrapdf", "link.rsp");
  const rspLines = linkObjs.map((p) => p);
  await writeFile(rspPath, rspLines.join("\n") + "\n");

  const linkArgs = [
    mingwTools.cxx,
    "-o",
    exePath,
    "-static",
    "-static-libgcc",
    "-static-libstdc++",
    "-mwindows", // GUI app (WinMain entry point)
    "@" + rspPath,
    ...extraLibDirs,
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

// same def as in linux-build.ts (ext/djvudec is the standalone DjVu decoder)
const djvudec: LibDef = {
  name: "djvudec",
  alwaysOptimize: true,
  defines: [],
  includes: [],
  files: [{ dir: "ext/djvudec", patterns: ["djvu.c"] }],
};

// Order: libraries that have no deps first, then dependents.
// The link order for archives is: most-dependent first, least-dependent last.
const ALL_LIBS: LibDef[] = [
  zlib,
  unrar,
  djvudec,
  chmdec,
  libarchive,
  libwebp,
  dav1d,
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
  aGumbo,
  zopfli,
  mupdf,
  utils,
];

export interface MingwBuildOptions {
  outDir: string;
  isRelease?: boolean;
  clean?: boolean;
  tools?: Partial<MingwTools>;
  /** parallel compile jobs; defaults to min(4, cpu count) */
  jobs?: number;
}

export async function buildMingw(opts: MingwBuildOptions): Promise<void> {
  setMingwTools(opts.tools ?? {});
  compileJobs = opts.jobs ?? DEFAULT_JOBS;
  await build(opts.isRelease ?? false, opts.clean ?? false, opts.outDir);
}

async function build(isRelease: boolean, clean: boolean, outDir: string): Promise<void> {
  const config = isRelease ? "release" : "debug";

  if (clean && existsSync(outDir)) {
    console.log(`Cleaning ${outDir}...`);
    rmSync(outDir, { recursive: true, force: true });
  }

  const startTime = performance.now();

  console.log(`\n=== Building SumatraPDF (${config}, x64, mingw) ===\n`);
  console.log(`Output: ${outDir}`);
  console.log(`Parallel jobs: ${compileJobs}\n`);

  mkdirSync(join(outDir, "obj"), { recursive: true });
  mkdirSync(join(outDir, "lib"), { recursive: true });

  // Build all static libraries
  const archives: string[] = [];
  for (const lib of ALL_LIBS) {
    // For utils, add debug extra files
    const libCopy = { ...lib };
    if (lib.name === "base" && !isRelease) {
      libCopy.files = [...lib.files, ...utilsDebugExtra];
    }
    const result = await buildLibrary(libCopy, outDir, isRelease, {
      tools: mingwBuildTools(),
      commonDefines: COMMON_DEFINES,
      cxxFlags: MINGW_CXX_FLAGS,
      jobs: compileJobs,
    });
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
