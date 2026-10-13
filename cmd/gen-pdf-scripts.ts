import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join, relative, resolve } from "node:path";
import { websiteDocsDir } from "./util";

const root = resolve(import.meta.dir, "..");
const kDefaultScript = "load-a-pdf.js";
const kScriptSeparator = "// ---";

export function genPDFScripts(inputRoot = root): string | null {
  const scriptsDir = join(inputRoot, relative(root, websiteDocsDir), "scripts");
  const output = join(inputRoot, ".work/scripts.js");
  if (!existsSync(scriptsDir)) {
    rmSync(output, { force: true });
    console.log(`gen-pdf-scripts: skipping, '${scriptsDir}' missing (needs sumatra-website checkout)`);
    return null;
  }
  const names = readdirSync(scriptsDir, { withFileTypes: true })
    .filter((file) => file.isFile() && file.name.endsWith(".js"))
    .map((file) => file.name)
    .sort();
  const first = names.indexOf(kDefaultScript);
  if (first >= 0) names.unshift(...names.splice(first, 1));
  const scripts = names.map((name) => {
    const source = readFileSync(join(scriptsDir, name), "utf8").replaceAll("\r\n", "\n").trimEnd();
    if (source.includes(kScriptSeparator)) throw new Error(`${name}: contains the script separator`);
    if (!source.trim()) throw new Error(`${name}: empty script`);
    return source;
  });
  if (!scripts.length) throw new Error("No PDF scripts found");
  const bundle = scripts.join(`\n${kScriptSeparator}\n`) + "\n";
  mkdirSync(join(inputRoot, ".work"), { recursive: true });
  if (!existsSync(output) || readFileSync(output, "utf8") !== bundle) writeFileSync(output, bundle);
  return output;
}

if (import.meta.main) {
  const output = genPDFScripts();
  if (output) console.log(output);
}
