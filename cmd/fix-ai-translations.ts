/**
 * Find (and optionally restore) human translations overwritten by AI on apptranslator.
 *
 * An overwrite is: latest revision for a lang+string is by an AI fill author
 * ("ai", "ai claude", "ai grok", "ai auto") and an earlier revision is by
 * someone else. -fix re-submits the last non-AI translation as that original
 * author (latest-wins on /api/edittranslation). "ai fix" is not an AI fill.
 *
 * Requires apptranslator /api/transhist (history per language).
 *
 * Usage:
 *   bun cmd/fix-ai-translations.ts                 # report all langs
 *   bun cmd/fix-ai-translations.ts --local
 *   bun cmd/fix-ai-translations.ts --lang de
 *   bun cmd/fix-ai-translations.ts -fix            # restore + report
 */

import { readFileSync } from "node:fs";
import { join } from "node:path";

const DEFAULT_SERVER = "https://www.apptranslator.org";
const LOCAL_SERVER = "http://127.0.0.1:9311";
const APP_NAME = "SumatraPDF";

interface CliArgs {
  server: string;
  langs: Set<string> | null; // null = all supported langs
  fix: boolean;
}

interface Rev {
  translation: string;
  author: string;
  dateMs: number;
}

interface StringHist {
  english: string;
  revs: Rev[];
}

interface Overwrite {
  english: string;
  humanTrans: string;
  humanAuthor: string;
  aiTrans: string;
  aiAuthor: string;
}

function parseArgs(argv: string[]): CliArgs {
  let server = process.env["APPTRANSLATOR_URL"] ?? DEFAULT_SERVER;
  let langs: Set<string> | null = null;
  let fix = false;

  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--local") {
      server = LOCAL_SERVER;
    } else if (a === "--server" && argv[i + 1]) {
      server = argv[++i];
    } else if (a.startsWith("--server=")) {
      server = a.slice("--server=".length);
    } else if (a === "--lang" && argv[i + 1]) {
      if (!langs) langs = new Set();
      langs.add(argv[++i]);
    } else if (a.startsWith("--lang=")) {
      if (!langs) langs = new Set();
      langs.add(a.slice("--lang=".length));
    } else if (a === "-fix" || a === "--fix") {
      fix = true;
    } else if (a === "--help" || a === "-h") {
      console.log(`Usage: bun cmd/fix-ai-translations.ts [options]
  --local              use ${LOCAL_SERVER}
  --server URL         apptranslator base URL (default ${DEFAULT_SERVER})
  --lang CODE          only this language (repeatable)
  -fix, --fix          restore overwritten human translations`);
      process.exit(0);
    } else {
      throw new Error(`Unknown arg: ${a} (try --help)`);
    }
  }
  return { server: server.replace(/\/$/, ""), langs, fix };
}

function readSecret(name: string, required: boolean): string {
  const secretsPath = join("..", "hack", "secrets", "sumatrapdf.env");
  try {
    const content = readFileSync(secretsPath, "utf-8");
    for (const line of content.split("\n")) {
      const trimmed = line.trim();
      if (trimmed.startsWith(name)) {
        const eqIdx = trimmed.indexOf("=");
        if (eqIdx !== -1) {
          const val = trimmed.substring(eqIdx + 1).trim();
          if (val.length >= 4) {
            return val;
          }
        }
      }
    }
  } catch {
    // fall through
  }
  const val = process.env[name] ?? "";
  if (val.length >= 4) {
    return val;
  }
  if (required) {
    throw new Error(`must set ${name} env variable or in secrets file`);
  }
  return "";
}

function loadSupportedLangCodes(): string[] {
  const path = join(import.meta.dir, "trans-gen.ts");
  const text = readFileSync(path, "utf-8");
  const startIdx = text.indexOf("const gLangs");
  const endIdx = text.indexOf("];", startIdx);
  const block = text.substring(startIdx, endIdx);
  const codes: string[] = [];
  const re = /\["([^"]+)"/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(block)) !== null) {
    if (m[1] !== "en") codes.push(m[1]);
  }
  return codes;
}

export function isAiAuthor(user: string): boolean {
  const u = user.trim().toLowerCase();
  return u === "ai" || u === "ai claude" || u === "ai grok" || u === "ai auto";
}

function parseDateMs(date: unknown): number {
  const n = typeof date === "number" ? date : typeof date === "string" ? Number(date) : NaN;
  if (!Number.isFinite(n)) return 0;
  return n * 1000;
}

export function parseHistory(raw: unknown): StringHist[] {
  if (!Array.isArray(raw)) {
    throw new Error(`transhist: expected array, got ${typeof raw}`);
  }
  const out: StringHist[] = [];
  for (const entry of raw) {
    if (!Array.isArray(entry) || entry.length < 1 || typeof entry[0] !== "string") {
      throw new Error(`transhist: bad string entry`);
    }
    const english = entry[0];
    const revs: Rev[] = [];
    for (let i = 1; i < entry.length; i++) {
      const rev = entry[i];
      if (!Array.isArray(rev) || rev.length < 3 || typeof rev[0] !== "string" || typeof rev[1] !== "string") {
        throw new Error(`transhist: bad revision for ${JSON.stringify(english)}`);
      }
      revs.push({ translation: rev[0], author: rev[1], dateMs: parseDateMs(rev[2]) });
    }
    revs.sort((a, b) => a.dateMs - b.dateMs);
    out.push({ english, revs });
  }
  return out;
}

export function findOverwritten(hist: StringHist[]): Overwrite[] {
  const out: Overwrite[] = [];
  for (const { english, revs } of hist) {
    if (revs.length === 0) continue;
    const latest = revs[revs.length - 1];
    if (!isAiAuthor(latest.author)) continue;
    let lastHuman: Rev | undefined;
    for (let i = revs.length - 2; i >= 0; i--) {
      if (!isAiAuthor(revs[i].author)) {
        lastHuman = revs[i];
        break;
      }
    }
    if (!lastHuman) continue;
    out.push({
      english,
      humanTrans: lastHuman.translation,
      humanAuthor: lastHuman.author,
      aiTrans: latest.translation,
      aiAuthor: latest.author,
    });
  }
  return out;
}

async function downloadHistory(server: string, lang: string): Promise<StringHist[]> {
  const uri = `${server}/api/transhist?app=${encodeURIComponent(APP_NAME)}&lang=${encodeURIComponent(lang)}`;
  const resp = await fetch(uri);
  if (!resp.ok) {
    const body = await resp.text();
    throw new Error(`transhist ${lang} HTTP ${resp.status}: ${body.slice(0, 300)}`);
  }
  return parseHistory(await resp.json());
}

async function submitTranslation(
  server: string,
  secret: string,
  user: string,
  lang: string,
  english: string,
  translation: string,
): Promise<void> {
  const params = new URLSearchParams({
    app: APP_NAME,
    lang,
    string: english,
    translation,
    secret,
    user,
  });
  const uri = `${server}/api/edittranslation?${params.toString()}`;
  const resp = await fetch(uri, { method: "GET" });
  if (!resp.ok) {
    const body = await resp.text();
    throw new Error(`edittranslation HTTP ${resp.status}: ${body.slice(0, 300)}`);
  }
  const text = await resp.text();
  if (!text.includes("OK")) {
    throw new Error(`edittranslation unexpected response: ${text.slice(0, 200)}`);
  }
}

function printLangReport(lang: string, overwrites: Overwrite[]): void {
  if (overwrites.length === 0) {
    console.log(`${lang}: no ai over-written translations`);
    return;
  }
  console.log(`${lang}:`);
  for (const o of overwrites) {
    console.log(`${o.english} ${o.humanTrans} by ${o.humanAuthor}, ai: ${o.aiTrans}`);
  }
}

async function main(): Promise<void> {
  const args = parseArgs(process.argv.slice(2));
  console.log(`apptranslator server: ${args.server}`);

  const langs = args.langs ? [...args.langs] : loadSupportedLangCodes();
  langs.sort((a, b) => a.localeCompare(b));
  if (langs.length === 0) {
    throw new Error("no languages to check");
  }

  let secret = "";
  if (args.fix) {
    secret = readSecret("TRANS_UPLOAD_SECRET", true);
  }

  let nOver = 0;
  let nFixed = 0;
  let nFixFail = 0;

  for (const lang of langs) {
    let overwrites: Overwrite[];
    try {
      const hist = await downloadHistory(args.server, lang);
      overwrites = findOverwritten(hist);
    } catch (e) {
      console.error(`${lang}: failed to download history: ${e}`);
      continue;
    }
    printLangReport(lang, overwrites);
    nOver += overwrites.length;
    if (!args.fix || overwrites.length === 0) {
      continue;
    }
    for (const o of overwrites) {
      try {
        await submitTranslation(args.server, secret, o.humanAuthor, lang, o.english, o.humanTrans);
        nFixed++;
        console.log(`  restored ${lang}: ${o.english} -> ${o.humanTrans} by ${o.humanAuthor}`);
      } catch (e) {
        nFixFail++;
        console.error(`  restore failed ${lang}: ${o.english}: ${e}`);
      }
    }
  }

  console.log(`overwritten: ${nOver}${args.fix ? `; restored: ${nFixed}; failed: ${nFixFail}` : ""}`);
}

if (import.meta.main) {
  await main();
}
