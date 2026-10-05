import { existsSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { genDocsForBuild } from "./gen-docs";

export const manualOutDir = resolve(import.meta.dir, "..", ".work", "docs");

export async function genDocs(): Promise<boolean> {
  const manifest = join(manualOutDir, "manifest.json");
  const before = existsSync(manifest) ? statSync(manifest).mtimeMs : 0;
  await genDocsForBuild();
  const after = existsSync(manifest) ? statSync(manifest).mtimeMs : 0;
  return before !== after;
}

if (import.meta.main) await genDocs();
