import { Glob } from "bun";
import {
  copyFileSync,
  existsSync,
  mkdirSync,
  readFileSync,
  readdirSync,
  rmSync,
  statSync,
  utimesSync,
  writeFileSync,
} from "node:fs";
import { dirname, join, relative, resolve, sep } from "node:path";
import type { Toolchain } from "./ng-toolchain";

const root = resolve(import.meta.dir, "../..");
export const fontPatterns: [string, string[]][] = [
  ["urw", ["Dingbats.cff", "NimbusMonoPS-*.cff", "NimbusRoman-*.cff", "NimbusSans-*.cff", "StandardSymbolsPS.cff"]],
  ["droid", ["DroidSansFallbackFull.ttf"]],
  ["sil", ["CharisSIL*.cff"]],
  [
    "noto",
    [
      "NotoSans-Regular.otf",
      "NotoSerif-Regular.otf",
      "NotoSansMath-Regular.otf",
      "NotoMusic-Regular.otf",
      "NotoSansSymbols-Regular.otf",
      "NotoSansSymbols2-Regular.otf",
      "NotoEmoji-Regular.ttf",
    ],
  ],
];

function filesUnder(dir: string): string[] {
  if (!existsSync(dir)) return [];
  return readdirSync(dir, { withFileTypes: true })
    .sort((a, b) => a.name.localeCompare(b.name))
    .flatMap((e) => {
      const path = join(dir, e.name);
      return e.isDirectory() ? filesUnder(path) : [path];
    });
}

function writeChanged(path: string, text: string): void {
  if (existsSync(path) && readFileSync(path, "utf8") === text) return;
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, text);
}

async function run(tc: Toolchain, args: string[], cwd = root): Promise<void> {
  const proc = Bun.spawn(args, { cwd, env: { ...process.env, ...tc.env }, stdout: "inherit", stderr: "inherit" });
  if ((await proc.exited) !== 0) throw new Error(`${args[0]} failed`);
}

export type PackOptions = {
  /** the in-app manual (.work/docs); without it help opens the website */
  manual: boolean;
  /** false stores the files as is, for an archive that is compressed as a whole later */
  compress: boolean;
};

const packDefaults: PackOptions = { manual: true, compress: true };

// Match pack-embedded-prebuild.cmd: manual at the root, fonts in fonts\, and the shared translation snapshot.
export async function packEmbedded(
  tc: Toolchain,
  packer: string,
  dir: string,
  inputRoot = root,
  opts: PackOptions = packDefaults,
): Promise<string> {
  const staging = join(dir, "embedded");
  const archive = join(dir, "embedded.lzsa");
  const inputs = new Map<string, string>();
  const docs = join(inputRoot, ".work/docs");
  if (opts.manual) {
    for (const src of filesUnder(docs)) inputs.set(relative(docs, src), src);
  }
  const translations = join(inputRoot, ".work/translations.txt");
  if (!existsSync(translations)) writeChanged(translations, "");
  inputs.set("translations.txt", translations);
  for (const name of ["marked.min.js", "mermaid.min.js"]) inputs.set(name, join(inputRoot, "ext", name));
  for (const [forge, patterns] of fontPatterns) {
    const fontDir = join(inputRoot, "ext/mupdf/resources/fonts", forge);
    for (const pattern of patterns) {
      const names = [...new Glob(pattern).scanSync({ cwd: fontDir })].sort();
      if (names.length === 0) throw new Error(`missing font: ${forge}/${pattern}`);
      for (const name of names) inputs.set(join("fonts", name), join(fontDir, name));
    }
  }
  for (const dst of filesUnder(staging)) {
    if (!inputs.has(relative(staging, dst))) rmSync(dst);
  }
  for (const [name, src] of inputs) {
    const dst = resolve(staging, name);
    if (!dst.startsWith(resolve(staging) + sep)) throw new Error(`invalid archive name: ${name}`);
    if (existsSync(dst) && readFileSync(src).equals(readFileSync(dst))) continue;
    mkdirSync(dirname(dst), { recursive: true });
    copyFileSync(src, dst);
    const st = statSync(src);
    utimesSync(dst, st.atime, st.mtime);
  }
  const packerArgs = opts.compress ? [] : ["-store"];
  await run(tc, [packer, ...packerArgs, archive, staging]);
  return archive;
}

export async function embedLzsa(tc: Toolchain, archive: string, dir: string): Promise<string> {
  const wrapper = join(root, "src/shared/EmbeddedLzsa.S");
  const obj = join(dir, tc.msvcStyle ? "embedded.res" : "embedded.o");
  let args: string[];
  let source = wrapper;
  if (tc.msvcStyle) {
    if (!tc.rc) throw new Error("rc.exe not found");
    source = join(dir, "generated/embedded.rc");
    writeChanged(source, `#include "resource.h"\nIDR_EMBEDDED_PAK RCDATA "${archive.replaceAll("\\", "/")}"\n`);
    args = [tc.rc, "/nologo", "/i", join(root, "src/ng"), "/fo", obj, source];
  } else {
    args = [
      tc.cc,
      "-x",
      "assembler-with-cpp",
      `-DEMBEDDED_LZSA="${archive.replaceAll("\\", "/")}"`,
      "-c",
      wrapper,
      "-o",
      obj,
    ];
  }
  const key = args.join("\n");
  const keyPath = `${obj}.cmd`;
  const deps = [archive, source, join(root, "src/ng/resource.h")];
  if (
    existsSync(obj) &&
    existsSync(keyPath) &&
    readFileSync(keyPath, "utf8") === key &&
    deps.every((p) => statSync(p).mtimeMs <= statSync(obj).mtimeMs)
  )
    return obj;
  console.log(`embedding ${relative(root, archive)}`);
  await run(tc, args);
  writeFileSync(keyPath, key);
  return obj;
}
