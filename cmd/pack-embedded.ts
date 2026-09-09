/**
 * Stages everything the exe embeds under .work/embedded/ and packs it into
 * one LzSA archive (.work/embedded.lzsa) linked as IDR_EMBEDDED_PAK.
 *
 * Contents (in-archive names = paths relative to .work/embedded/):
 *   translations.txt
 *   marked.min.js
 *   mermaid.min.js
 *   <files from .work/docs/…>  (manual assets; optional if docs not generated yet)
 *
 * The non-static SumatraPDF build adds libsumatrapdf.dll & co on top of this
 * staging dir and packs out/<cfg>/embedded.lzsa instead; see
 * cmd/pack-embedded-prebuild.cmd (the VS / ninja prebuild).
 *
 * Usage: bun cmd/pack-embedded.ts
 */
import { copyFileSync, existsSync, mkdirSync, readdirSync, rmSync, statSync, writeFileSync } from "node:fs";
import { join, relative, resolve } from "node:path";

const workDir = ".work";
const stagingDir = join(workDir, "embedded");
const archivePath = join(workDir, "embedded.lzsa");
const translationsTxt = join(workDir, "translations.txt");
const docsDir = join(workDir, "docs");
const makeLzsaExe = resolve(join("bin", "MakeLZSA.exe"));

function formatSize(n: number): string {
  if (n < 1024) return `${n} B`;
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`;
  return `${(n / (1024 * 1024)).toFixed(2)} MB`;
}

function listFilesRecursive(dir: string): string[] {
  const out: string[] = [];
  const walk = (d: string) => {
    for (const ent of readdirSync(d, { withFileTypes: true })) {
      const p = join(d, ent.name);
      if (ent.isDirectory()) {
        walk(p);
      } else if (ent.isFile()) {
        out.push(p);
      }
    }
  };
  if (existsSync(dir)) {
    walk(dir);
  }
  return out;
}

export async function packEmbedded(): Promise<void> {
  if (!existsSync(makeLzsaExe)) {
    throw new Error(`'${makeLzsaExe}' doesn't exist`);
  }
  mkdirSync(workDir, { recursive: true });
  if (!existsSync(translationsTxt)) {
    writeFileSync(translationsTxt, "");
    console.log(`created empty ${translationsTxt}`);
  }
  for (const p of ["ext/marked.min.js", "ext/mermaid.min.js"]) {
    if (!existsSync(p)) {
      throw new Error(`missing ${p}`);
    }
  }

  // Staging is also what the VS / ninja prebuild packs (MakeLZSA only — no bun on PATH).
  rmSync(stagingDir, { recursive: true, force: true });
  mkdirSync(stagingDir, { recursive: true });

  copyFileSync(translationsTxt, join(stagingDir, "translations.txt"));
  copyFileSync("ext/marked.min.js", join(stagingDir, "marked.min.js"));
  copyFileSync("ext/mermaid.min.js", join(stagingDir, "mermaid.min.js"));

  let nDocs = 0;
  if (existsSync(docsDir)) {
    for (const abs of listFilesRecursive(docsDir)) {
      const rel = relative(docsDir, abs);
      const dest = join(stagingDir, rel);
      mkdirSync(resolve(dest, ".."), { recursive: true });
      copyFileSync(abs, dest);
      nDocs++;
    }
  } else {
    console.log(`note: ${docsDir} missing; packing without manual docs`);
  }

  const nTotal = 3 + nDocs;
  console.log(`packing ${nTotal} files into ${archivePath} (${nDocs} manual assets)`);
  await runMakeLzsa();
}

async function runMakeLzsa(): Promise<void> {
  // Keep previous archive for MakeLZSA entry reuse (faster rebuilds).
  const proc = Bun.spawn([makeLzsaExe, archivePath, stagingDir], {
    stdout: "inherit",
    stderr: "inherit",
  });
  const code = await proc.exited;
  if (code !== 0) {
    throw new Error(`MakeLZSA failed with exit code ${code}`);
  }
  const size = statSync(archivePath).size;
  console.log(`wrote ${archivePath} (${formatSize(size)})`);
}

if (import.meta.main) {
  await packEmbedded();
}
