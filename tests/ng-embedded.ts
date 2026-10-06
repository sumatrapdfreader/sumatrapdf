import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve, sep } from "node:path";
import { findToolchain, hostPlatform } from "../cmd/helper/ng-toolchain";
import { ROOT } from "./util.ts";

export async function testit(): Promise<void> {
  const fail = (message: string): never => {
    throw new Error(message);
  };
  const tc = findToolchain(ROOT, hostPlatform(), false, fail);
  const workDir = resolve(ROOT, ".work");
  mkdirSync(workDir, { recursive: true });
  const fixture = mkdtempSync(join(workDir, "ng-embedded-test-"));
  const put = (path: string, text: string) => {
    const dst = join(fixture, path);
    mkdirSync(dirname(dst), { recursive: true });
    writeFileSync(dst, text);
  };
  const run = (args: string[]) => {
    const proc = Bun.spawnSync(args, { cwd: fixture, env: { ...process.env, ...tc.env } });
    if (proc.exitCode !== 0) fail(`${args[0]} failed:\n${proc.stdout}\n${proc.stderr}`);
  };

  try {
    const generator = readFileSync(join(ROOT, "cmd/ng-gen-embedded.ts"), "utf8");
    put("cmd/ng-gen-embedded.ts", generator);
    put(
      "cmd/ng-gen-docs.ts",
      'import { resolve } from "node:path";\n' +
        'export const manualOutDir = resolve(import.meta.dir, "../.work/docs");\n' +
        "export async function genDocs() { return true; }\n",
    );
    put(
      "cmd/ng-gen-translations.ts",
      'import { resolve } from "node:path";\n' +
        'export const translationsOutPath = resolve(import.meta.dir, "../.work/translations.txt");\n' +
        "export function genTranslations() {}\n",
    );
    put(".work/translations.txt", "test");
    put("ext/marked.min.js", "test");
    put("ext/mermaid.min.js", "test");
    for (const match of generator.matchAll(/"([^"\n]+\.(?:cff|otf|ttf))"/g)) {
      const name = match[1]!;
      const forge = ["urw", "sil", "noto", "droid"].find((dir) =>
        existsSync(join(ROOT, "ext/mupdf/resources/fonts", dir, name)),
      );
      if (!forge) fail(`font not found: ${name}`);
      put(`ext/mupdf/resources/fonts/${forge}/${name}`, "test");
    }
    put("src/ng/base/Base.h", "using u8 = unsigned char;\nstruct Str;\n");
    copyFileSync(join(ROOT, "src/ng/EmbeddedResources.h"), join(fixture, "src/ng/EmbeddedResources.h"));

    for (const count of [0, 1]) {
      if (count === 1) put(".work/docs/index.md", "test");
      run([process.execPath, join(fixture, "cmd/ng-gen-embedded.ts")]);
      put(
        "probe.cpp",
        '#include "base/Base.h"\n#include "EmbeddedResources.h"\n' +
          `int main() { return gEmbeddedManualCount == ${count} ? 0 : 1; }\n`,
      );
      const exe = join(fixture, tc.msvcStyle ? "probe.exe" : "probe");
      const sources = ["src/ng/EmbeddedDataManual.cpp", "probe.cpp"];
      const flags = tc.msvcStyle
        ? ["/nologo", "/std:c++20", "/W4", "/WX", "/EHsc", "/Isrc/ng", `/Fe${exe}`]
        : ["-std=c++20", "-pedantic-errors", "-Wall", "-Werror", "-Isrc/ng", "-o", exe];
      run([tc.cxx, ...flags, ...sources]);
      run([exe]);
    }
  } finally {
    if (!resolve(fixture).startsWith(workDir + sep)) fail(`invalid fixture path: ${fixture}`);
    rmSync(fixture, { recursive: true, force: true });
  }
  console.log("PASS: ng embedded manual with and without docs");
}

if (import.meta.main) {
  try {
    await testit();
  } catch (error) {
    console.error(error);
    process.exitCode = 1;
  }
}
