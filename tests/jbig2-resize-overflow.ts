// jbig2_image_resize() must reject a height whose buffer size wraps.
// Compiles the vendored decoder; does not build SumatraPDF.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, runStandalone, tmpPath } from "./util.ts";

function vcvarsPath(): string {
  const vswhere = join(
    process.env["ProgramFiles(x86)"] ?? "C:\\Program Files (x86)",
    "Microsoft Visual Studio",
    "Installer",
    "vswhere.exe",
  );
  if (!existsSync(vswhere)) {
    return "";
  }
  const found = spawnSync(
    vswhere,
    ["-latest", "-requires", "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
    {
      encoding: "utf8",
    },
  );
  const root = found.stdout.trim();
  if (found.status !== 0 || !root) {
    return "";
  }
  const bat = join(root, "VC", "Auxiliary", "Build", "vcvarsall.bat");
  return existsSync(bat) ? bat : "";
}

function compileAndRun(vcvars: string, arch: "x64" | "x86"): void {
  const out = tmpPath(`jbig2-resize-${arch}.exe`);
  const objDir = tmpPath(`jbig2-resize-${arch}`);
  const src = join(ROOT, "tests", "jbig2-resize-overflow.c");
  const dec = join(ROOT, "ext", "a-jbig2dec", "jbig2dec.c");
  const inc = join(ROOT, "ext", "a-jbig2dec");
  mkdirSync(objDir, { recursive: true });
  const bat = tmpPath(`jbig2-resize-${arch}.bat`);
  // /Fo takes a directory. A trailing backslash would escape the closing quote.
  writeFileSync(
    bat,
    [
      `@echo off`,
      `call "${vcvars}" ${arch} >nul`,
      `if errorlevel 1 exit /b 1`,
      `cl /nologo /O1 /D_CRT_SECURE_NO_WARNINGS /DHAVE_STRING_H=1 /DJBIG_NO_MEMENTO /wd4018 /wd4100 /wd4146 /wd4244 /wd4267 /wd4456 /wd4701 /I"${inc}" /Fo"${objDir}/" /Fe"${out}" "${src}" "${dec}"`,
      `exit /b %ERRORLEVEL%`,
      ``,
    ].join("\r\n"),
  );
  const built = spawnSync("cmd.exe", ["/d", "/c", bat], { cwd: ROOT, encoding: "utf8" });
  if (built.status !== 0) {
    throw new Error(`cl ${arch} failed (${built.status})\n${built.stdout}\n${built.stderr}`);
  }
  const ran = spawnSync(out, [], { encoding: "utf8" });
  if (ran.status !== 0) {
    throw new Error(`${arch}: ${ran.stdout}${ran.stderr}`);
  }
}

export async function testit(): Promise<void> {
  const vcvars = vcvarsPath();
  if (!vcvars) {
    console.log("jbig2 resize: skip, vcvarsall.bat is not installed");
    return;
  }
  compileAndRun(vcvars, "x64");
  compileAndRun(vcvars, "x86");
}

if (import.meta.main) {
  await runStandalone(testit);
}
