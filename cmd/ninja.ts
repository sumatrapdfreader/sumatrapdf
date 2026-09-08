import { existsSync, mkdirSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { runLogged } from "./util";

const ninjaDir = "ninja";
const buildFile = join(ninjaDir, "build.ninja");
const generatedFile = join(ninjaDir, ".generated");
const premakeFiles = ["premake5.lua", "premake5.files.lua"];
const resources = [
  ["SumatraPDF", "SumatraPDF.exe", "../src/SumatraPDF.rc"],
  ["SumatraPDF-static", "SumatraPDF-static.exe", "../src/SumatraPDF.rc"],
  ["libsumatrapdf", "libsumatrapdf.dll", "../src/libsumatrapdf.rc"],
  ["PdfFilter", "obj/PdfFilter.dll", "../src/ifilter/PdfFilter.rc"],
  ["PdfPreview", "obj/PdfPreview.dll", "../src/previewer/PdfPreview.rc"],
] as const;

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
    const re = new RegExp(`^build (../out/([^/]+)/${targetRe})( \\| [^:]+)?: link_msc-v145 (.+)$`, "gm");
    text = text.replace(re, (line, output, config, implicitOutputs, inputs) => {
      const resource = `../out/${config}/obj/${project}/${project}.res`;
      if (inputs.includes(resource)) {
        return line;
      }
      return `build ${resource}: rc_msc-v145 ${source}\nbuild ${output}${implicitOutputs ?? ""}: link_msc-v145 ${resource} ${inputs}`;
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
    fixed = fixed.replace(/(build [^\n]* \.\.\/ext\/synctex\/[^\n]*\n  cflags = [^\n]*)/g, (line) => {
      return line.includes('/wd"4244"') ? line : `${line} /wd"4244" /wd"4267"`;
    });
    // link.exe does not update .exp files, so they cannot be Ninja outputs.
    fixed = fixed.replace(/ \| ([^ \n]+\.exp) /g, " | ");
    fixed = fixed.replace(/(build [^\n]*\.dll) \| [^\n]*\.lib:/g, "$1:");
    // libsumatrapdf's linker options place the DLL outside its intermediate dir.
    fixed = fixed.replace(/build (\.\.\/out\/([^/]+))\/obj\/libsumatrapdf\.dll:/g, "build $1/libsumatrapdf.dll:");
    // link.exe leaves an unchanged import library untouched. Model it as a
    // phony dependency, otherwise Ninja relinks this DLL on every invocation.
    fixed = fixed.replace(/^build (\.\.\/out\/[^/]+)\/obj\/libsumatrapdf\.lib: phony .+\n/gm, "");
    fixed = fixed.replace(
      /^build (\.\.\/out\/[^/]+)\/libsumatrapdf\.dll(?: \| [^:]+)?:(.*)$/gm,
      "build $1/libsumatrapdf.dll:$2",
    );
    // The archive prebuild must wait for every binary it packages.
    fixed = fixed.replace(
      /^build (\.\.\/out\/([^/]+)\/obj\/SumatraPDF\/SumatraPDF\.prebuild): prebuild.*$/gm,
      "build $1: prebuild || ../out/$2/libsumatrapdf.dll ../out/$2/obj/PdfFilter.dll ../out/$2/obj/PdfPreview.dll ../out/$2/sumatrapdf-tool.exe",
    );
    fixed = addResources(fixed, path);
    const dlls = [...fixed.matchAll(/^build (\.\.\/out\/[^/]+)\/libsumatrapdf\.dll: link_msc-v145/gm)];
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
        if (!output.startsWith("../out/")) {
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
