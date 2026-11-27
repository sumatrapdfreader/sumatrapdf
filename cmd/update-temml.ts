// Vendors Temml (https://temml.org, MIT), which renders the TeX math in
// markdown ($...$, $$...$$, ```math) to MathML, as ext/temml.min.js and
// ext/temml.css.
//
// MathML is drawn by the browser view itself (WebView2, WebKit) with the system
// math font (Cambria Math on Windows), so no math fonts are bundled. temml.css is
// the package's Temml-Local.css with Temml.woff2 (its small script-letters font)
// inlined as a data: url, so the page loads one stylesheet.
//
// Run: bun cmd/update-temml.ts
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

const version = "0.14.0";

function run(cmd: string[], cwd: string): void {
  const p = Bun.spawnSync(cmd, { cwd, stdout: "inherit", stderr: "inherit" });
  if (p.exitCode !== 0) {
    throw new Error(`failed: ${cmd.join(" ")}`);
  }
}

const rootDir = join(import.meta.dir, "..");
const outJs = join(rootDir, "ext", "temml.min.js");
const outCss = join(rootDir, "ext", "temml.css");
const tmp = mkdtempSync(join(tmpdir(), "temml-"));
try {
  run(["npm", "pack", `temml@${version}`, "--silent"], tmp);
  run(["tar", "xzf", `temml-${version}.tgz`], tmp);
  const dist = join(tmp, "package", "dist");

  const license = readFileSync(join(tmp, "package", "LICENSE"), "utf-8")
    .split("\n")[2]!
    .trim();
  const header = `/*! Temml v${version} | MIT License | ${license} | https://temml.org */\n`;
  writeFileSync(outJs, header + readFileSync(join(dist, "temml.min.js"), "utf-8"));

  const woff2 = readFileSync(join(dist, "Temml.woff2")).toString("base64");
  const css = readFileSync(join(dist, "Temml-Local.css"), "utf-8");
  const fontUrl = "url('Temml.woff2')";
  if (!css.includes(fontUrl)) {
    throw new Error(`Temml-Local.css no longer has ${fontUrl}`);
  }
  writeFileSync(outCss, header + css.replace(fontUrl, `url(data:font/woff2;base64,${woff2})`));
  console.log(`wrote ${outJs} (${readFileSync(outJs).length} bytes)`);
  console.log(`wrote ${outCss} (${readFileSync(outCss).length} bytes)`);
} finally {
  rmSync(tmp, { recursive: true, force: true });
}
