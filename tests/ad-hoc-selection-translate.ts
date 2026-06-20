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
import { EXE, runStandalone } from "./util.ts";

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

function normalizeTranslation(text: string): string {
  return text.trim().replace(/\s+/g, " ");
}

function wordOverlapRatio(a: string, b: string): number {
  const la = normalizeTranslation(a).toLowerCase().split(" ").filter(Boolean);
  const lb = normalizeTranslation(b).toLowerCase().split(" ").filter(Boolean);
  const setB = new Set(lb);
  let common = 0;
  for (const w of la) {
    if (setB.has(w)) {
      common++;
    }
  }
  const union = new Set([...la, ...lb]).size;
  return union > 0 ? common / union : 0;
}

// Prefer an exact match. Separate LLM calls can pick synonyms, so for this
// fixed English phrase also accept Polish outputs that share key wording.
function translationsAgree(a: string, b: string): boolean {
  const na = normalizeTranslation(a);
  const nb = normalizeTranslation(b);
  if (na === nb) {
    return true;
  }

  const wordsA = na.split(" ");
  const wordsB = nb.split(" ");
  let prefix = 0;
  const minLen = Math.min(wordsA.length, wordsB.length);
  for (let i = 0; i < minLen; i++) {
    if (wordsA[i] !== wordsB[i]) {
      break;
    }
    prefix++;
  }
  if (prefix >= 4) {
    return true;
  }

  const overlap = wordOverlapRatio(na, nb);
  if (overlap >= 0.5) {
    return true;
  }

  // PHRASE is English mentioning July and Amazon.com
  if (
    /amazon/i.test(na) &&
    /amazon/i.test(nb) &&
    /lipc/i.test(na) &&
    /lipc/i.test(nb) &&
    overlap >= 0.25
  ) {
    return true;
  }

  return false;
}

function mentionsJulyInPolish(text: string): boolean {
  return /lipc/i.test(text);
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

function isSkippableError(translation: string): boolean {
  return /authentication|api error|not installed|timed out|model is not supported/i.test(
    translation,
  );
}

async function translatePhrase(
  backend: Backend,
  srcLang: string,
): Promise<{ exitCode: number; translation: string } | null> {
  const res = await runControlCommand(EXE, ControlCommand.TestSelectionTranslate, [
    backend.id,
    srcLang,
    "Polish",
    PHRASE,
  ]);
  const exitCode = Number(res[0]);
  const translation = String(res[1] ?? "").trim();
  return { exitCode, translation };
}

async function translatePhraseWithRetries(
  backend: Backend,
  srcLang: string,
  maxAttempts = 3,
): Promise<{ exitCode: number; translation: string }> {
  let last: { exitCode: number; translation: string } | null = null;
  for (let attempt = 0; attempt < maxAttempts; attempt++) {
    const res = await translatePhrase(backend, srcLang);
    if (!res) {
      continue;
    }
    last = res;
    if (res.exitCode !== 0 || isSkippableError(res.translation)) {
      return res;
    }
    if (!looksLikePolish(res.translation, PHRASE)) {
      if (attempt + 1 < maxAttempts) {
        console.log(`  retry ${srcLang} (not plausible Polish, attempt ${attempt + 1})`);
      }
      continue;
    }
    if (srcLang === "English" && !mentionsJulyInPolish(res.translation)) {
      if (attempt + 1 < maxAttempts) {
        console.log(`  retry ${srcLang} (missing lipc*, attempt ${attempt + 1})`);
      }
      continue;
    }
    return res;
  }
  if (!last) {
    fail(`${backend.name}: no response for ${srcLang} → Polish`);
  }
  return last;
}

async function runBackendTranslation(backend: Backend): Promise<void> {
  if (!isInstalled(backend.exePaths)) {
    console.log(`• skip ${backend.name}: CLI not installed`);
    return;
  }

  console.log(`• testing ${backend.name} English → Polish ...`);
  const english = await translatePhraseWithRetries(backend, "English");
  console.log(`  English exit=${english.exitCode}`);
  console.log(`  English result: ${english.translation}`);

  if (english.exitCode !== 0 || isSkippableError(english.translation)) {
    if (isSkippableError(english.translation)) {
      console.log(`  skip ${backend.name}: ${english.translation.slice(0, 120)}`);
      return;
    }
    fail(`${backend.name} English → Polish failed: ${english.translation}`);
  }

  if (!looksLikePolish(english.translation, PHRASE)) {
    fail(`${backend.name} did not return plausible Polish: ${english.translation}`);
  }
  if (!mentionsJulyInPolish(english.translation)) {
    fail(`${backend.name} English did not translate July (expected lipc*): ${english.translation}`);
  }

  console.log(`• testing ${backend.name} Auto → Polish ...`);
  const auto = await translatePhraseWithRetries(backend, "Auto");
  console.log(`  Auto exit=${auto.exitCode}`);
  console.log(`  Auto result: ${auto.translation}`);

  if (auto.exitCode !== 0 || isSkippableError(auto.translation)) {
    if (isSkippableError(auto.translation)) {
      console.log(`  skip ${backend.name}: ${auto.translation.slice(0, 120)}`);
      return;
    }
    fail(`${backend.name} Auto → Polish failed: ${auto.translation}`);
  }

  if (!looksLikePolish(auto.translation, PHRASE)) {
    fail(`${backend.name} Auto did not return plausible Polish: ${auto.translation}`);
  }
  if (!mentionsJulyInPolish(auto.translation)) {
    fail(`${backend.name} Auto did not translate July (expected lipc*): ${auto.translation}`);
  }

  const englishNorm = normalizeTranslation(english.translation);
  const autoNorm = normalizeTranslation(auto.translation);
  if (!translationsAgree(english.translation, auto.translation)) {
    fail(
      `${backend.name} Auto vs English mismatch:\n` +
        `  English: ${englishNorm}\n` +
        `  Auto:    ${autoNorm}`,
    );
  }

  if (englishNorm === autoNorm) {
    console.log(`  ✅ ${backend.name} ok (English and Auto match)`);
  } else {
    console.log(
      `  ✅ ${backend.name} ok (English and Auto agree; overlap=${wordOverlapRatio(englishNorm, autoNorm).toFixed(2)})`,
    );
  }
}

export async function testit(): Promise<void> {
  for (const backend of BACKENDS) {
    await runBackendTranslation(backend);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}