// Reports dependencies needed to run SumatraPDF tests, especially LaTeX/SyncTeX
// integration tests (tests/latex.ts).
//
// Run:  bun tests/check-deps.ts

import { existsSync } from "node:fs";
import { join } from "node:path";
import { EXE, ROOT } from "./util.ts";

const WSL_DISTRO = "Ubuntu";

const MIKTEX_INSTALL = [
  "winget install MiKTeX.MiKTeX",
  "pdflatex.exe / lualatex.exe are also searched in %PATH% and:",
  "  %LOCALAPPDATA%\\Programs\\MiKTeX\\miktex\\bin\\x64\\",
  "  C:\\Program Files\\MiKTeX\\miktex\\bin\\x64\\",
].join("\n    ");

const TECTONIC_WIN_INSTALL = [
  "In PowerShell:",
  "  [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072",
  "  iex ((New-Object System.Net.WebClient).DownloadString('https://drop-ps1.fullyjustified.net'))",
].join("\n    ");

const TECTONIC_WSL_INSTALL = [
  `wsl --install -d ${WSL_DISTRO}`,
  `wsl -d ${WSL_DISTRO} -- bash -lc "curl --proto '=https' --tlsv1.2 -fsSL https://drop-sh.fullyjustified.net |sh"`,
].join("\n    ");

type Dep = {
  name: string;
  ok: boolean;
  detail?: string;
  install?: string;
};

function runCmd(cmd: string[]): { ok: boolean; stdout: string; stderr: string } {
  try {
    const p = Bun.spawnSync({ cmd, stdout: "pipe", stderr: "pipe" });
    return {
      ok: p.exitCode === 0,
      stdout: p.stdout.toString().trim(),
      stderr: p.stderr.toString().trim(),
    };
  } catch (e) {
    return { ok: false, stdout: "", stderr: String(e) };
  }
}

function firstLine(text: string): string {
  return text.split(/\r?\n/).find((l) => l.trim())?.trim() ?? "";
}

function findOnPath(exe: string): string | null {
  return Bun.which(exe) ?? null;
}

function findLatexEngine(engine: "pdflatex" | "lualatex"): string | null {
  const exe = `${engine}.exe`;
  const localAppData = process.env.LOCALAPPDATA ?? "";
  const candidates = [
    exe,
    join(localAppData, "Programs", "MiKTeX", "miktex", "bin", "x64", exe),
    join("C:/Program Files/MiKTeX/miktex/bin/x64", exe),
    join("C:/Program Files (x86)/MiKTeX/miktex/bin/x64", exe),
  ];
  for (const c of candidates) {
    if (c === exe) {
      const inPath = Bun.which(c);
      if (inPath) {
        return inPath;
      }
      continue;
    }
    if (c && existsSync(c)) {
      return c;
    }
  }
  return null;
}

function depFromPath(name: string, path: string | null, install?: string): Dep {
  if (path) {
    return { name, ok: true, detail: path };
  }
  return { name, ok: false, install };
}

function depFromCmd(name: string, cmd: string[], install?: string): Dep {
  const r = runCmd(cmd);
  if (r.ok) {
    const detail = firstLine(r.stdout) || firstLine(r.stderr);
    return { name, ok: true, detail: detail || cmd.join(" ") };
  }
  return { name, ok: false, install };
}

function depFromExePaths(name: string, paths: string[], install?: string): Dep {
  for (const p of paths) {
    if (p && existsSync(p)) {
      return { name, ok: true, detail: p };
    }
  }
  return { name, ok: false, install };
}

function checkCore(): Dep[] {
  const deps: Dep[] = [];
  deps.push(
    existsSync(EXE)
      ? { name: "SumatraPDF.exe (dbg64)", ok: true, detail: EXE }
      : {
          name: "SumatraPDF.exe (dbg64)",
          ok: false,
          install: "bun ./cmd/build.ts",
        },
  );

  const git = findOnPath("git");
  deps.push(
    git
      ? depFromCmd("git", [git, "--version"])
      : { name: "git", ok: false, install: "winget install Git.Git" },
  );

  const cl = findOnPath("cl");
  deps.push(
    cl
      ? { name: "cl.exe (MSVC)", ok: true, detail: cl }
      : {
          name: "cl.exe (MSVC)",
          ok: false,
          install: "Install Visual Studio Build Tools (Desktop development with C++)",
        },
  );

  return deps;
}

function checkLatex(): Dep[] {
  const deps: Dep[] = [];

  const pdflatex = findLatexEngine("pdflatex");
  deps.push(depFromPath("pdflatex (MiKTeX)", pdflatex, MIKTEX_INSTALL));

  const lualatex = findLatexEngine("lualatex");
  deps.push(depFromPath("lualatex (MiKTeX)", lualatex, MIKTEX_INSTALL));

  deps.push(depFromCmd("tectonic (Windows)", ["tectonic", "--version"], TECTONIC_WIN_INSTALL));

  const wsl = depFromCmd("wsl.exe", ["wsl.exe", "--version"], "wsl --install");
  deps.push(wsl);

  const wslDistro = depFromCmd(
    `WSL distro: ${WSL_DISTRO}`,
    ["wsl.exe", "-d", WSL_DISTRO, "--", "echo", "ok"],
    `wsl --install -d ${WSL_DISTRO}`,
  );
  deps.push(wslDistro);

  const tectonicWsl = depFromCmd(
    `tectonic in WSL (${WSL_DISTRO})`,
    ["wsl.exe", "-d", WSL_DISTRO, "--", "tectonic", "--version"],
    TECTONIC_WSL_INSTALL,
  );
  deps.push(tectonicWsl);

  return deps;
}

function checkAdHocOptional(): Dep[] {
  const asanExe = join(ROOT, "out", "dbg64_asan", "SumatraPDF-static.exe");
  const deps: Dep[] = [];

  deps.push(
    existsSync(asanExe)
      ? { name: "SumatraPDF-static.exe (ASan, issue-chm-lzx)", ok: true, detail: asanExe }
      : {
          name: "SumatraPDF-static.exe (ASan, issue-chm-lzx)",
          ok: false,
          install: "bun ./cmd/build-asan.ts",
        },
  );

  const exifPy = join(ROOT, "..", "exif-py");
  deps.push(
    existsSync(join(exifPy, ".git"))
      ? { name: "exif-py clone (ad-hoc-exif)", ok: true, detail: exifPy }
      : {
          name: "exif-py clone (ad-hoc-exif)",
          ok: false,
          install: `git clone https://github.com/ianare/exif-py.git ${exifPy}`,
        },
  );

  const userProfile = process.env.USERPROFILE ?? "";
  const localAppData = process.env.LOCALAPPDATA ?? "";

  deps.push(
    depFromExePaths(
      "Grok Build CLI (ad-hoc-selection-translate)",
      [
        join(userProfile, ".grok", "bin", "grok.exe"),
        join(userProfile, ".local", "bin", "grok.exe"),
      ],
      "Install Grok CLI from https://x.ai/",
    ),
  );

  deps.push(
    depFromExePaths(
      "Claude Code CLI (ad-hoc-selection-translate)",
      [
        join(userProfile, ".local", "bin", "claude.exe"),
        join(userProfile, "AppData", "Local", "Programs", "claude-code", "claude.exe"),
      ],
      "Install Claude Code from https://claude.ai/",
    ),
  );

  deps.push(
    depFromExePaths(
      "OpenAI Codex CLI (ad-hoc-selection-translate)",
      [
        join(userProfile, ".codex", "bin", "codex.exe"),
        join(userProfile, ".local", "bin", "codex.exe"),
        join(localAppData, "Microsoft", "WinGet", "Links", "codex.exe"),
      ],
      "Install Codex CLI from https://openai.com/codex",
    ),
  );

  return deps;
}

function printDep(dep: Dep): void {
  const tag = dep.ok ? "OK  " : "MISS";
  const detail = dep.detail ? `  ${dep.detail}` : "";
  console.log(`${tag}  ${dep.name}${detail}`);
  if (!dep.ok && dep.install) {
    for (const line of dep.install.split("\n")) {
      console.log(`      ${line}`);
    }
  }
}

function printSection(title: string, deps: Dep[]): number {
  console.log(`\n=== ${title} ===`);
  for (const dep of deps) {
    printDep(dep);
  }
  return deps.filter((d) => !d.ok).length;
}

function printLatexSummary(latex: Dep[]): void {
  const haveMiktex = latex.slice(0, 2).some((d) => d.ok);
  const haveTectonicWin = latex[2]?.ok;
  const haveTectonicWsl = latex[5]?.ok;

  console.log("\n=== LaTeX test coverage (tests/latex.ts) ===");
  if (haveMiktex) {
    console.log("OK   issue-5633, ad-hoc-synctex-chinese (MiKTeX pdflatex/lualatex)");
  } else {
    console.log("MISS issue-5633, ad-hoc-synctex-chinese need MiKTeX");
  }
  if (haveTectonicWin || haveTectonicWsl) {
    const parts = [];
    if (haveTectonicWin) {
      parts.push("Windows tectonic");
    }
    if (haveTectonicWsl) {
      parts.push(`WSL ${WSL_DISTRO} tectonic`);
    }
    console.log(`OK   ad-hoc-synctex-wsl (${parts.join(" + ")})`);
  } else {
    console.log("MISS ad-hoc-synctex-wsl needs tectonic on Windows and/or inside WSL");
  }
}

function main(): void {
  console.log("SumatraPDF test dependencies\n");

  const core = checkCore();
  const latex = checkLatex();
  const adHoc = checkAdHocOptional();

  let missing = 0;
  missing += printSection("Core (tests/all.ts)", core);
  missing += printSection("LaTeX / SyncTeX (tests/latex.ts)", latex);
  missing += printSection("Ad-hoc / before-release (optional)", adHoc);

  printLatexSummary(latex);

  const latexMissing = latex.filter((d) => !d.ok).length;
  console.log("");
  if (missing === 0) {
    console.log("All checked dependencies are installed.");
  } else {
    console.log(`${missing} dependency check(s) missing (${latexMissing} in LaTeX section).`);
  }
}

main();