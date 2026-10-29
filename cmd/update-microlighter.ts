// Vendors MicroLighter (https://github.com/davatron5000/microlighter, MIT), the
// syntax highlighter for markdown code blocks, as ext/microlighter.min.js.
//
// The npm package is an ES module that import()s each grammar on demand. We
// bundle the core and every grammar into one classic script that sets
// globalThis.microlighter, so the markdown page loads a single file from
// IDR_EMBEDDED_PAK and its CSP allows a single script.
//
// Run: bun cmd/update-microlighter.ts
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const version = "2.2.0";
const esbuildVersion = "0.25.10";

function run(cmd: string[], cwd: string): void {
  const p = Bun.spawnSync(cmd, { cwd, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    throw new Error(`failed: ${cmd.join(" ")}`);
  }
}

const rootDir = join(import.meta.dir, "..");
const out = join(rootDir, "ext", "microlighter.min.js");
const tmp = mkdtempSync(join(tmpdir(), "microlighter-"));
try {
  run(["npm", "pack", `microlighter@${version}`, "--silent"], tmp);
  run(["tar", "xzf", `microlighter-${version}.tgz`], tmp);
  const pkg = join(tmp, "package");
  writeFileSync(
    join(pkg, "entry.js"),
    `import { highlightAll } from "./dist/highlight.js";\nglobalThis.microlighter = { highlightAll };\n`,
  );
  const bundle = join(tmp, "bundle.js");
  // esbuild bundles every file the template import() can reach (./grammars/*.js)
  run(
    ["bunx", `esbuild@${esbuildVersion}`, "entry.js", "--bundle", "--format=iife", "--minify", `--outfile=${bundle}`],
    pkg,
  );

  const license = readFileSync(join(pkg, "LICENSE"), "utf-8").split("\n")[2]!.trim();
  const header = `/*! microlighter v${version} | MIT License | ${license} | https://github.com/davatron5000/microlighter */\n`;
  writeFileSync(out, header + readFileSync(bundle, "utf-8"));
  console.log(`wrote ${out} (${readFileSync(out).length} bytes)`);
} finally {
  rmSync(tmp, { recursive: true, force: true });
}
