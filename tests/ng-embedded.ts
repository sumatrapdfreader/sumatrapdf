import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, statSync, utimesSync, writeFileSync } from "node:fs";
import { dirname, join, resolve, sep } from "node:path";
import { embedLzsa, fontPatterns, packEmbedded } from "../cmd/helper/embedded";
import { findToolchain, hostPlatform } from "../cmd/helper/ng-toolchain";
import { root as ROOT } from "../cmd/helper/ng-compile";
import { extractStringsToTranslate } from "../cmd/trans-dl";

export async function testit(): Promise<void> {
  const fail = (message: string): never => {
    throw new Error(message);
  };
  const tc = findToolchain(ROOT, hostPlatform(), false, fail);
  const strings = extractStringsToTranslate();
  if (!strings.includes("Failed to save image")) fail("ng translation strings were not collected");
  const run = (args: string[]) => {
    const proc = Bun.spawnSync(args, { cwd: ROOT, env: { ...process.env, ...tc.env } });
    if (proc.exitCode !== 0) fail(`${args[0]} failed:\n${proc.stdout}\n${proc.stderr}`);
  };
  run([process.execPath, join(ROOT, "cmd/ng-build.ts"), "-rel", "test_embedded"]);
  const dir = join(ROOT, "out", tc.plat, "rel");
  const packer = join(dir, tc.msvcStyle ? "MakeLZSA.exe" : "MakeLZSA");
  const probe = join(dir, tc.msvcStyle ? "test_embedded.exe" : "test_embedded");
  run([probe, join(dir, "embedded.lzsa"), join(dir, "embedded")]);

  const workDir = resolve(ROOT, ".work");
  const fixture = mkdtempSync(join(workDir, "ng-embedded-test-"));
  const put = (path: string, text: string) => {
    const dst = join(fixture, path);
    mkdirSync(dirname(dst), { recursive: true });
    writeFileSync(dst, text);
  };
  const names = (archive: string): string[] => {
    const data = readFileSync(archive);
    const lzsaMagic = 0x41537a4c;
    const archiveHeaderSize = 8;
    const fileCountOffset = 4;
    const entryMetadataSize = 24;
    if (data.readUInt32LE() !== lzsaMagic) fail("invalid LzSA magic");
    const out: string[] = [];
    let offset = archiveHeaderSize;
    for (let i = 0; i < data.readUInt32LE(fileCountOffset); i++) {
      const size = data.readUInt32LE(offset);
      out.push(data.subarray(offset + entryMetadataSize, offset + size - 1).toString());
      offset += size;
    }
    return out;
  };
  try {
    for (const name of ["marked.min.js", "mermaid.min.js"]) put(`ext/${name}`, "test");
    for (const [forge, patterns] of fontPatterns) {
      for (const pattern of patterns)
        put(`ext/mupdf/resources/fonts/${forge}/${pattern.replace("*", "Regular")}`, "font");
    }
    const output = join(fixture, "out");
    let archive = await packEmbedded(tc, packer, output, fixture);
    if (statSync(join(fixture, ".work/translations.txt")).size !== 0) fail("missing translations did not become empty");
    if (names(archive).some((name) => name.endsWith(".md"))) fail("missing manual was packed");
    run([packer, archive]);
    await embedLzsa(tc, archive, output);

    put(".work/docs/nested/index.md", "manual");
    put(".work/translations.txt", ":test\nde:Test\n");
    archive = await packEmbedded(tc, packer, output, fixture);
    if (!names(archive).includes("nested\\index.md")) fail("manual was not packed at the archive root");
    if (!names(archive).includes("fonts\\NotoSans-Regular.otf")) fail("fonts were not packed");
    run([packer, archive]);
    const before = statSync(archive).mtimeMs;
    await packEmbedded(tc, packer, output, fixture);
    if (statSync(archive).mtimeMs !== before) fail("unchanged archive was rewritten");

    rmSync(join(fixture, ".work/docs/nested/index.md"));
    await packEmbedded(tc, packer, output, fixture);
    if (names(archive).includes("nested\\index.md")) fail("removed manual file remains in the archive");
    const translationPath = join(fixture, ".work/translations.txt");
    const timestamp = statSync(translationPath);
    put(".work/translations.txt", ":changed\n");
    utimesSync(translationPath, timestamp.atime, timestamp.mtime);
    const old = readFileSync(archive);
    await packEmbedded(tc, packer, output, fixture);
    if (old.equals(readFileSync(archive))) fail("changed translations did not update the archive");
    if (!existsSync(join(output, "embedded", "translations.txt"))) fail("translations were not staged");

    // what the wasm build asks for: no manual, no fonts, files stored as is
    const marker = "compressible ".repeat(64);
    put(".work/docs/nested/index.md", "manual");
    put(".work/translations.txt", marker);
    const forWeb = { manual: false, compress: false, fonts: false };
    archive = await packEmbedded(tc, packer, output, fixture, forWeb);
    if (names(archive).some((name) => name.endsWith(".md"))) fail("manual was packed without being asked for");
    if (names(archive).some((name) => name.startsWith("fonts\\") || name.startsWith("fonts/"))) {
      fail("fonts were packed for the web");
    }
    if (!readFileSync(archive).includes(marker)) fail("-store compressed a file");
    run([packer, archive]);
    // same files, different packing: entries of the previous archive can't be reused
    archive = await packEmbedded(tc, packer, output, fixture);
    if (readFileSync(archive).includes(marker)) fail("stored entry was reused in a compressed archive");
    archive = await packEmbedded(tc, packer, output, fixture, forWeb);
    if (!readFileSync(archive).includes(marker)) fail("compressed entry was reused in a stored archive");
  } finally {
    if (!resolve(fixture).startsWith(workDir + sep)) fail(`invalid fixture path: ${fixture}`);
    rmSync(fixture, { recursive: true, force: true });
  }
  console.log("PASS: shared LzSA packing and extraction");
}

if (import.meta.main) {
  try {
    await testit();
  } catch (error) {
    console.error(error);
    process.exitCode = 1;
  }
}
