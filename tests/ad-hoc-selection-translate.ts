// Ad-hoc selection translation test for Grok Build / Claude Code / OpenAI Codex.
//
// Exercises the C++ translation path via -dbg-control TestSelectionTranslate.
// Requires the corresponding CLI to be installed; missing backends are skipped.
//
// NOT registered in tests/all.ts — run directly:
//   bun tests/ad-hoc-selection-translate.ts [--no-build]
// or as part of: bun tests/before-release.ts

import { existsSync } from "node:fs";
import { join } from "node:path";
import { ControlCommand, runControlCommand } from "../cmd/control.ts";
import { EXE, ROOT, runStandalone } from "./util.ts";

const PHRASE =
  "In July of last year, Amazon.com reached an important way station.";

type Backend = {
  id: number;
  name: string;
  exePaths: string[];
};

const BACKENDS: Backend[] = [
  {
    id: 1,
    name: "Grok Build",
    exePaths: [
      join(process.env.USERPROFILE ?? "", ".grok", "bin", "grok.exe"),
      join(process.env.USERPROFILE ?? "", ".local", "bin", "grok.exe"),
    ],
  },
  {
    id: 0,
    name: "Claude Code",
    exePaths: [
      join(process.env.USERPROFILE ?? "", ".local", "bin", "claude.exe"),
      join(
        process.env.USERPROFILE ?? "",
        "AppData",
        "Local",
        "Programs",
        "claude-code",
        "claude.exe",
      ),
    ],
  },
  {
    id: 2,
    name: "OpenAI Codex",
    exePaths: [
      join(process.env.USERPROFILE ?? "", ".codex", "bin", "codex.exe"),
      join(process.env.USERPROFILE ?? "", ".local", "bin", "codex.exe"),
      join(
        process.env.LOCALAPPDATA ?? "",
        "Microsoft",
        "WinGet",
        "Links",
        "codex.exe",
      ),
    ],
  },
];

function fail(msg: string): never {
  throw new Error(msg);
}

function isInstalled(paths: string[]): boolean {
  for (const p of paths) {
    if (p && existsSync(p)) {
      return true;
    }
  }
  return false;
}

function looksLikePolish(text: string, english: string): boolean {
  const trimmed = text.trim();
  if (!trimmed) {
    return false;
  }
  if (trimmed === english.trim()) {
    return false;
  }
  if (/did not contain text/i.test(trimmed)) {
    return false;
  }
  if (/failed to authenticate|authentication_failed/i.test(trimmed)) {
    return false;
  }
  if (/lipcu/i.test(trimmed)) {
    return true;
  }
  if (/[ąćęłńóśźż]/i.test(trimmed)) {
    return true;
  }
  return false;
}

async function runBackendTranslation(backend: Backend): Promise<void> {
  if (!isInstalled(backend.exePaths)) {
    console.log(`• skip ${backend.name}: CLI not installed`);
    return;
  }

  console.log(`• testing ${backend.name} English → Polish ...`);
  const res = await runControlCommand(EXE, ControlCommand.TestSelectionTranslate, [
    backend.id,
    "English",
    "Polish",
    PHRASE,
  ]);

  const exitCode = Number(res[0]);
  const translation = String(res[1] ?? "").trim();
  console.log(`  exit=${exitCode}`);
  console.log(`  result: ${translation}`);

  if (
    exitCode !== 0 ||
    /authentication|api error|not installed|timed out|model is not supported/i.test(
      translation,
    )
  ) {
    if (/authentication|api error|not installed|timed out|model is not supported/i.test(translation)) {
      console.log(`  skip ${backend.name}: ${translation.slice(0, 120)}`);
      return;
    }
    fail(`${backend.name} failed: ${translation}`);
  }

  if (!looksLikePolish(translation, PHRASE)) {
    fail(`${backend.name} did not return plausible Polish: ${translation}`);
  }
  console.log(`  ✅ ${backend.name} ok`);
}

export async function testit(): Promise<void> {
  for (const backend of BACKENDS) {
    await runBackendTranslation(backend);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}