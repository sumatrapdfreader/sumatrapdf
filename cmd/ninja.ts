import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { runLogged } from "./util";

export const ninjaDir = join(".work", "ninja");
// Generated paths are relative to ninjaDir, two levels below the root.
export const ninjaToRoot = join("..", "..");
const buildFile = join(ninjaDir, "build.ninja");
const generatedFile = join(ninjaDir, ".generated");
// ninja.ts is included: it post-processes the generated files, so changing it
// must re-generate them from scratch (the fixups are not idempotent).
const premakeFiles = ["premake5.lua", "premake5.files.lua", "cmd/ninja.ts"];
const resources = [
  ["SumatraPDF", "SumatraPDF.exe", "../../src/SumatraPDF.rc"],
  ["SumatraPDF-static", "SumatraPDF-static.exe", "../../src/SumatraPDF.rc"],
  ["libsumatrapdf", "libsumatrapdf.dll", "../../src/libsumatrapdf.rc"],
  ["PdfFilter", "PdfFilter.dll", "../../src/ifilter/PdfFilter.rc"],
  ["PdfPreview", "PdfPreview.dll", "../../src/previewer/PdfPreview.rc"],
] as const;
// SharedLib projects using dll_shared_lib_dirs(): the .dll ships in out/<cfg>/
// while premake emits it under the intermediate dir out/<cfg>/obj.
const sharedLibs = ["libsumatrapdf", "PdfFilter", "PdfPreview"];

function needsGenerate(): boolean {
  if (!existsSync(buildFile) || !existsSync(generatedFile)) {
    return true;
  }
  const generated = statSync(generatedFile).mtimeMs;
  return premakeFiles.some((path) => statSync(path).mtimeMs > generated);
}

function ninjaFiles(dir: string): string[] {
  const paths: string[] = [];
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) {
      paths.push(...ninjaFiles(path));
    } else if (entry.name.endsWith(".ninja")) {
      paths.push(path);
    }
  }
  return paths;
}

function addResources(text: string, path: string): string {
  for (const [project, target, source] of resources) {
    if (!path.endsWith(`${project}.ninja`)) {
      continue;
    }
    const targetRe = target.replace(".", "\\.");
    const re = new RegExp(`^build (../../out/([^/]+)/${targetRe})( \\| [^:]+)?: link_msc-v145 (.+)$`, "gm");
    text = text.replace(re, (line, output, config, implicitOutputs, inputs) => {
      const resource = `../../out/${config}/obj/${project}/${project}.res`;
      if (inputs.includes(resource)) {
        return line;
      }
      // The prebuild packs IDR_EMBEDDED_PAK, so the .res waits for that stamp.
      // SumatraPDF.exe's archive also holds libsumatrapdf.dll & co and lives in
      // out/<cfg>/, passed via the EMBEDDED_PAK resdefine premake sets.
      let deps = "";
      let flags = "";
      if (project === "SumatraPDF") {
        deps = ` | ../../out/${config}/obj/SumatraPDF/SumatraPDF.prebuild`;
        flags = `\n  resflags = /D EMBEDDED_PAK=.\\..\\..\\out\\${config}\\embedded.lzsa`;
      } else if (project === "SumatraPDF-static") {
        deps = ` | ../../out/${config}/obj-s/SumatraPDF-static/SumatraPDF-static.prebuild`;
      }
      return `build ${resource}: rc_msc-v145 ${source}${deps}${flags}\nbuild ${output}${implicitOutputs ?? ""}: link_msc-v145 ${resource} ${inputs}`;
    });
  }
  return text;
}

function fixEscapes(): void {
  for (const path of ninjaFiles(ninjaDir)) {
    const text = readFileSync(path, "utf-8");
    let fixed = text.replace(/\$+\(/g, () => "$$(");
    fixed = fixed
      .split("\n")
      .map((line) => {
        // Visual Studio's Unicode character set adds these definitions itself.
        if ((line.startsWith("cflags_") || line.startsWith("cxxflags_")) && !line.includes('/D"UNICODE"')) {
          line = `${line} /D"UNICODE" /D"_UNICODE"`;
        }
        // MSVC requires debug information for useful ASan reports and otherwise
        // emits C5072, which our project correctly promotes to an error.
        if (
          (line.startsWith("cflags_") || line.startsWith("cxxflags_")) &&
          line.includes("/fsanitize=address") &&
          !line.includes("/Zi")
        ) {
          line = `${line} /Zi`;
        }
        if (
          (line.startsWith("cflags_") || line.startsWith("cxxflags_")) &&
          line.includes("/fsanitize=address") &&
          !line.includes("/FS")
        ) {
          return `${line} /FS`;
        }
        // rc.exe rejects options placed after the input file.
        if (line.startsWith("  command = rc ")) {
          return line.replace(" $in $resflags", " $resflags $in");
        }
        if (line.includes("nasm.exe") || line.includes("bin2coff.exe")) {
          return line.replaceAll('\\"', '"');
        }
        if (line.includes("prebuildcommands =")) {
          return line.replaceAll('\\"', '""');
        }
        return line;
      })
      .join("\n");
    // Premake's Ninja backend does not apply the Synctex file filter.
    fixed = fixed.replace(/(build [^\n]* \.\.\/\.\.\/ext\/synctex\/[^\n]*\n  cflags = [^\n]*)/g, (line) => {
      return line.includes('/wd"4244"') ? line : `${line} /wd"4244" /wd"4267"`;
    });
    // link.exe does not update .exp files, so they cannot be Ninja outputs.
    fixed = fixed.replace(/ \| ([^ \n]+\.exp) /g, " | ");
    fixed = fixed.replace(/(build [^\n]*\.dll) \| [^\n]*\.lib:/g, "$1:");
    // dll_shared_lib_dirs() sets targetdir to the intermediate dir but links
    // with /OUT into out/<cfg>/, so rewrite both the build edges and the phony
    // aliases premake points at the intermediate path.
    for (const name of sharedLibs) {
      const re = new RegExp(`(\\.\\./out/[^/\\s]+)/obj/${name}\\.dll`, "g");
      fixed = fixed.replace(re, `$1/${name}.dll`);
    }
    // link.exe leaves an unchanged import library untouched. Model it as a
    // phony dependency, otherwise Ninja relinks this DLL on every invocation.
    fixed = fixed.replace(/^build (\.\.\/\.\.\/out\/[^/]+)\/obj\/libsumatrapdf\.lib: phony .+\n/gm, "");
    fixed = fixed.replace(
      /^build (\.\.\/\.\.\/out\/[^/]+)\/libsumatrapdf\.dll(?: \| [^:]+)?:(.*)$/gm,
      "build $1/libsumatrapdf.dll:$2",
    );
    // The archive prebuild must wait for every binary it packages.
    fixed = fixed.replace(
      /^build (\.\.\/\.\.\/out\/([^/]+)\/obj\/SumatraPDF\/SumatraPDF\.prebuild): prebuild.*$/gm,
      "build $1: prebuild || ../../out/$2/libsumatrapdf.dll ../../out/$2/PdfFilter.dll ../../out/$2/PdfPreview.dll ../../out/$2/sumatrapdf-tool.exe",
    );
    fixed = addResources(fixed, path);
    const dlls = [...fixed.matchAll(/^build (\.\.\/\.\.\/out\/[^/]+)\/libsumatrapdf\.dll: link_msc-v145/gm)];
    for (const [, outputDir] of dlls) {
      fixed += `\nbuild ${outputDir}/obj/libsumatrapdf.lib: phony ${outputDir}/libsumatrapdf.dll\n`;
    }
    fixed = fixed.replace(/\/D"([^"]+)="([^"]+)""/g, (_match, name, value) => `/D${name}=\\"${value}\\"`);
    if (fixed !== text) {
      writeFileSync(path, fixed);
    }
  }
}

function createOutputDirs(): void {
  for (const path of ninjaFiles(ninjaDir)) {
    const text = readFileSync(path, "utf-8");
    for (const line of text.split("\n")) {
      const outputs = line.match(/^build (.+?):/)?.[1];
      if (!outputs) {
        continue;
      }
      for (const output of outputs.split(" ")) {
        if (!output.startsWith("../../out/")) {
          continue;
        }
        mkdirSync(dirname(join(ninjaDir, output)), { recursive: true });
      }
    }
  }
}

export async function ensureNinja(): Promise<void> {
  const generate = needsGenerate();
  if (generate) {
    await runLogged(join("bin", "premake5.exe"), ["--cc=msc-v145", "ninja"]);
  }
  fixEscapes();
  createOutputDirs();
  if (generate) {
    writeFileSync(generatedFile, "");
  }
}

if (import.meta.main) {
  await ensureNinja();
}
