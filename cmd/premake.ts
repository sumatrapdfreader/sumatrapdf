import { $ } from "bun";
import { join } from "node:path";
import { readFileSync, writeFileSync } from "node:fs";

const premakePath = join("bin", "premake5.exe");
await $`${premakePath} vs2022`;

// premake5.exe always emits CRLF line endings, but these two project files are
// checked in with LF (the rest of the vs2022/ files use CRLF). Without this,
// every regeneration flips their line endings and produces a whole-file diff.
// Normalize them back to LF so regen only shows real content changes.
const lfFiles = ["vs2022/SumatraPDF-dll.vcxproj", "vs2022/SumatraPDF.vcxproj"];
for (const f of lfFiles) {
  const content = readFileSync(f, "utf8");
  const normalized = content.replace(/\r\n/g, "\n");
  if (normalized !== content) {
    writeFileSync(f, normalized);
    console.log(`normalized line endings to LF: ${f}`);
  }
}
