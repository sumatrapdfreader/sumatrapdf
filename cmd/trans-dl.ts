/**
 * Maintain SumatraPDF translations with apptranslator as the source of truth.
 *
 * Local cache (gitignored via .work/):
 *   .work/translations.txt       languages complete enough for the binary
 *   .work/translations.txt.lzsa  compressed archive embedded in the exe
 *
 * Flow:
 *   1. Extract _TR* strings from src + command names
 *   2. POST them to /api/dltransfor (marks active strings; returns sha1 + translations)
 *   3. If any language is missing translations:
 *        - translate with Claude or Grok
 *        - submit each via /api/edittranslation as user "ai claude" / "ai grok"
 *   4. Re-download (should now include submitted translations)
 *   5. Write filtered .work/translations.txt and build .lzsa
 *
 * Usage:
 *   bun cmd/trans-dl.ts                 # production apptranslator + Claude if key set
 *   bun cmd/trans-dl.ts --local         # http://127.0.0.1:9311
 *   bun cmd/trans-dl.ts --server URL
 *   bun cmd/trans-dl.ts --ai=claude|grok|none
 *   bun cmd/trans-dl.ts --no-ai         # download + filter + lzsa only
 *   bun cmd/trans-dl.ts --lang de       # only fill missing for one language
 *   bun cmd/trans-dl.ts --max-submit N  # cap AI submissions (for testing)
 */

import {
  readFileSync,
  writeFileSync,
  readdirSync,
  existsSync,
  mkdirSync,
} from "node:fs";
import { join, extname } from "node:path";
import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { commands } from "./gen-commands";

// strings that should not be sent for translation
// (e.g. command names whose display text is set dynamically)
const translationBlacklist: string[] = [
  "Toggle Windows Previewer",
  "Toggle Windows Search Filter",
];

const DEFAULT_SERVER = "https://www.apptranslator.org";
const LOCAL_SERVER = "http://127.0.0.1:9311";
const APP_NAME = "SumatraPDF";

const workDir = ".work";
const translationsTxtPath = join(workDir, "translations.txt");
const translationsLzsaPath = join(workDir, "translations.txt.lzsa");

const translationPattern = /\b_TR[ANW]?\("(.*?)"\)/g;

type AiProvider = "claude" | "grok" | "none";

interface CliArgs {
  server: string;
  ai: AiProvider;
  langs: Set<string> | null; // null = all
  maxSubmit: number; // 0 = unlimited
  skipLzsa: boolean;
}

function parseArgs(argv: string[]): CliArgs {
  let server = process.env["APPTRANSLATOR_URL"] ?? DEFAULT_SERVER;
  let ai: AiProvider = "claude";
  let langs: Set<string> | null = null;
  let maxSubmit = 0;
  let skipLzsa = false;

  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === "--local") {
      server = LOCAL_SERVER;
    } else if (a === "--server" && argv[i + 1]) {
      server = argv[++i];
    } else if (a.startsWith("--server=")) {
      server = a.slice("--server=".length);
    } else if (a === "--no-ai") {
      ai = "none";
    } else if (a === "--ai" && argv[i + 1]) {
      ai = parseAi(argv[++i]);
    } else if (a.startsWith("--ai=")) {
      ai = parseAi(a.slice("--ai=".length));
    } else if (a === "--lang" && argv[i + 1]) {
      if (!langs) langs = new Set();
      langs.add(argv[++i]);
    } else if (a.startsWith("--lang=")) {
      if (!langs) langs = new Set();
      langs.add(a.slice("--lang=".length));
    } else if (a === "--max-submit" && argv[i + 1]) {
      maxSubmit = parseInt(argv[++i], 10) || 0;
    } else if (a.startsWith("--max-submit=")) {
      maxSubmit = parseInt(a.slice("--max-submit=".length), 10) || 0;
    } else if (a === "--skip-lzsa") {
      skipLzsa = true;
    } else if (a === "--help" || a === "-h") {
      console.log(`Usage: bun cmd/trans-dl.ts [options]
  --local              use ${LOCAL_SERVER}
  --server URL         apptranslator base URL (default ${DEFAULT_SERVER})
  --ai=claude|grok|none  AI for missing translations (default claude)
  --no-ai              same as --ai=none
  --lang CODE          only fill this language (repeatable)
  --max-submit N       stop after N AI submissions (testing)
  --skip-lzsa          do not run MakeLZSA`);
      process.exit(0);
    } else {
      throw new Error(`Unknown arg: ${a} (try --help)`);
    }
  }
  return { server: server.replace(/\/$/, ""), ai, langs, maxSubmit, skipLzsa };
}

function parseAi(s: string): AiProvider {
  if (s === "claude" || s === "grok" || s === "none") return s;
  throw new Error(`--ai must be claude|grok|none, got '${s}'`);
}

function extractTranslations(s: string): string[] {
  const res: string[] = [];
  for (const match of s.matchAll(translationPattern)) {
    res.push(match[1]);
  }
  return res;
}

function getFilesToProcess(): string[] {
  const res: string[] = [];
  const entries = readdirSync("src", { withFileTypes: true });
  for (const entry of entries) {
    if (entry.isFile() && extname(entry.name).toLowerCase() === ".cpp") {
      res.push(join("src", entry.name));
    }
  }
  return res;
}

function extractStringsFromCFilesNoPaths(): string[] {
  const files = getFilesToProcess();
  console.log(`Files to process: ${files.length}`);
  const allStrs: string[] = [];
  for (const path of files) {
    const content = readFileSync(path, "utf-8");
    allStrs.push(...extractTranslations(content));
  }
  const unique = [...new Set(allStrs)];
  console.log(`${unique.length} strings to translate`);
  return unique;
}

function extractStringsToTranslate(): string[] {
  const strs = extractStringsFromCFilesNoPaths();
  for (let i = 1; i < commands.length; i += 2) {
    strs.push(commands[i]);
  }
  const unique = [...new Set(strs)];
  for (const bl of translationBlacklist) {
    if (!unique.includes(bl)) {
      throw new Error(`Blacklisted string "${bl}" not found in strings to translate`);
    }
  }
  const blacklistSet = new Set(translationBlacklist);
  const filtered = unique.filter((s) => !blacklistSet.has(s));
  console.log(
    `${filtered.length} unique strings to translate (including command descriptions, ${translationBlacklist.length} blacklisted)`,
  );
  return filtered;
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

function tryGetTransSecret(): string {
  const s = readSecret("TRANS_UPLOAD_SECRET", false);
  if (s) {
    console.log("Got TRANS_UPLOAD_SECRET");
  }
  return s;
}

/** Minimal empty cache when we cannot talk to apptranslator (no secret). */
function writeEmptyTranslations(): void {
  mkdirSync(workDir, { recursive: true });
  writeFileSync(translationsTxtPath, "", "utf-8");
  console.log(`Wrote empty ${translationsTxtPath} (translations skipped)`);
}

interface DownloadResult {
  full: string; // complete response text
  sha1: string; // sha1 of translation body (line 2 of response)
  body: string; // translation body after the two header lines
}

function parseDownloadResponse(text: string): DownloadResult {
  // Format:
  //   AppTranslator: SumatraPDF\n
  //   <sha1>\n
  //   <body...\n>
  const nl1 = text.indexOf("\n");
  if (nl1 < 0) throw new Error("invalid download: no first newline");
  const header = text.slice(0, nl1);
  if (!header.startsWith("AppTranslator:")) {
    throw new Error(`invalid download header: ${header.slice(0, 80)}`);
  }
  const nl2 = text.indexOf("\n", nl1 + 1);
  if (nl2 < 0) throw new Error("invalid download: no sha1 line");
  const sha1 = text.slice(nl1 + 1, nl2).trim();
  if (!/^[0-9a-f]{40}$/i.test(sha1)) {
    throw new Error(`invalid download sha1: '${sha1}'`);
  }
  // body is everything after second newline; response usually ends with extra \n
  let body = text.slice(nl2 + 1);
  if (body.endsWith("\n")) {
    body = body.slice(0, -1);
  }
  return { full: text, sha1, body };
}

function readLocalSha1(): string | null {
  if (!existsSync(translationsTxtPath)) {
    return null;
  }
  try {
    const text = readFileSync(translationsTxtPath, "utf-8");
    return parseDownloadResponse(text).sha1;
  } catch {
    return null;
  }
}

async function downloadTranslations(server: string, strs: string[], secret: string): Promise<DownloadResult> {
  const timeStart = performance.now();
  const uri = `${server}/api/dltransfor?app=${encodeURIComponent(APP_NAME)}&secret=${encodeURIComponent(secret)}`;
  const body = strs.join("\n");
  console.log(`POST ${uri} (${strs.length} strings, ${body.length} bytes)`);
  const resp = await fetch(uri, { method: "POST", body });
  if (!resp.ok) {
    const errBody = await resp.text();
    throw new Error(`download HTTP ${resp.status}: ${resp.statusText}\n${errBody.slice(0, 500)}`);
  }
  const text = await resp.text();
  const parsed = parseDownloadResponse(text);
  // verify body sha1 matches header
  const computed = createHash("sha1").update(parsed.body, "utf8").digest("hex");
  // server sha1 is of the joined lines before final trailing newline handling;
  // trust server sha1 for cache comparison; only warn if wildly off
  if (computed !== parsed.sha1) {
    // body may differ by trailing newline conventions; still use server sha1
    console.log(`note: local body sha1 ${computed} != header ${parsed.sha1} (using header)`);
  }
  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`download finished in ${elapsed}s, sha1=${parsed.sha1}`);
  return parsed;
}

interface BadTranslation {
  currString: string;
  orig: string;
  fixed: string;
}

function fixTranslation(s: string): string {
  s = s.trim();
  if (s.endsWith("\\n")) s = s.slice(0, -2);
  if (s.endsWith("\\r")) s = s.slice(0, -2);
  if (s.endsWith("\\n")) s = s.slice(0, -2);
  s = s.trim();
  return s;
}

function fixTranslations(d: string): { fixed: string; badTranslations: BadTranslation[] } {
  const badTranslations: BadTranslation[] = [];
  const lines = d.split("\n");
  const result: string[] = [];
  let currString = "";

  for (const line of lines) {
    if (line.startsWith(":")) {
      currString = line.substring(1);
      result.push(line);
      continue;
    }
    const fixed = fixTranslation(line);
    if (line !== fixed) {
      badTranslations.push({ currString, orig: line, fixed });
    }
    result.push(fixed);
  }
  return { fixed: result.join("\n"), badTranslations };
}

function printBadTranslations(server: string, badTranslations: BadTranslation[]): void {
  if (badTranslations.length === 0) return;
  badTranslations.sort((a, b) => a.orig.localeCompare(b.orig));
  let currLang = "";
  for (const bt of badTranslations) {
    const lang = bt.orig.split(":")[0];
    if (lang !== currLang) {
      currLang = lang;
      console.log(`\n${server}/app/${APP_NAME}/${lang}`);
    }
    console.log(`${bt.currString}\n  '${bt.orig}' => '${bt.fixed}'`);
  }
}

interface ParsedTranslations {
  perLang: Map<string, Map<string, string>>;
  allStrings: string[];
}

function parseTranslations(d: string): ParsedTranslations {
  // d is full download (with header) or body-only; skip AppTranslator / sha1 lines
  const lines = d.split("\n");
  let start = 0;
  if (lines[0]?.startsWith("AppTranslator:")) {
    start = 2;
  }
  const a = lines.slice(start);
  const perLang = new Map<string, Map<string, string>>();
  const allStrings: string[] = [];
  let currString = "";

  for (const s of a) {
    if (s.length === 0) continue;
    if (s.startsWith(":")) {
      currString = s.substring(1);
      allStrings.push(currString);
      continue;
    }
    const colonIdx = s.indexOf(":");
    if (colonIdx === -1) continue;
    const lang = s.substring(0, colonIdx);
    if (lang.length > 5) throw new Error(`lang too long: '${lang}'`);
    const trans = s.substring(colonIdx + 1);
    let m = perLang.get(lang);
    if (!m) {
      m = new Map();
      perLang.set(lang, m);
    }
    m.set(currString, trans);
  }
  return { perLang, allStrings };
}

function stripAmpersand(s: string): string {
  return s.replaceAll("&", "");
}

/** Fill untranslated no-& strings from matching & translations (local only until submitted). */
function autoAddNoPrefixTranslations(pt: ParsedTranslations): Map<string, Map<string, string>> {
  const { perLang, allStrings } = pt;
  const added = new Map<string, Map<string, string>>();
  let nAdded = 0;
  for (const [lang, translations] of perLang) {
    const cache = new Map<string, string>();
    for (const [orig, trans] of translations) {
      if (orig.includes("&")) {
        cache.set(stripAmpersand(orig), stripAmpersand(trans));
      }
    }
    for (const s of allStrings) {
      if (s.includes("&")) continue;
      if (translations.has(s)) continue;
      const trans = cache.get(s);
      if (trans) {
        translations.set(s, trans);
        let m = added.get(lang);
        if (!m) {
          m = new Map();
          added.set(lang, m);
        }
        m.set(s, trans);
        nAdded++;
      }
    }
  }
  if (nAdded > 0) {
    console.log(`autoAddNoPrefixTranslations: ${nAdded} candidates to submit`);
  }
  return added;
}

/** Language codes we ship (from trans-gen.ts gLangs), excluding English. */
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

// AI translation order: most widely spoken first (rough L1+L2 speaker ranks).
// Polish is forced first (project preference), then global popularity.
// Codes not listed fall to the end (stable by code).
const kAiLangPopularityOrder: string[] = [
  "pl", // Polish — first by request
  "cn", // Chinese Simplified (~1.1B)
  "es", // Spanish (~550M)
  "hi", // Hindi (~600M)
  "ar", // Arabic (~400M)
  "fr", // French (~300M)
  "bn", // Bengali (~270M)
  "br", // Portuguese Brazil (~230M)
  "ru", // Russian (~250M)
  "pt", // Portuguese Portugal
  "id", // Indonesian (~200M)
  "de", // German (~130M)
  "ja", // Japanese (~125M)
  "pa", // Punjabi (~125M)
  "fa", // Persian (~110M)
  "jv", // Javanese (~80M)
  "tr", // Turkish (~85M)
  "kr", // Korean (~80M)
  "vn", // Vietnamese (~85M)
  "ta", // Tamil (~75M)
  "it", // Italian (~65M)
  "th", // Thai (~60M)
  "tw", // Chinese Traditional (~30M+)
  "uk", // Ukrainian (~40M)
  "my", // Malaysian
  "ml", // Malayalam
  "ro", // Romanian
  "nl", // Dutch
  "tl", // Tagalog
  "hu", // Hungarian
  "el", // Greek
  "cz", // Czech
  "sv", // Swedish
  "he", // Hebrew
  "az", // Azerbaijani
  "ne", // Nepali
  "bg", // Bulgarian
  "sr-rs", // Serbian Cyrillic
  "sp-rs", // Serbian Latin
  "sk", // Slovak
  "hr", // Croatian
  "dk", // Danish
  "fi", // Finnish
  "no", // Norwegian Bokmål
  "nn", // Norwegian Nynorsk
  "si", // Sinhala
  "mm", // Burmese
  "ku", // Kurdish
  "uz", // Uzbek
  "lt", // Lithuanian
  "sl", // Slovenian
  "lv", // Latvian
  "et", // Estonian
  "ka", // Georgian
  "sq", // Albanian
  "bs", // Bosnian
  "by", // Belarusian
  "mk", // Macedonian
  "am", // Armenian
  "ca", // Catalan
  "ca-xv", // Catalan-Valencian
  "gl", // Galician
  "eu", // Basque
  "af", // Afrikaans
  "cy", // Welsh
  "ga", // Irish
  "fo", // Faroese
  "fy-nl", // Frisian
  "co", // Corsican
  "sn", // Shona
  "sat", // Santali
  "kw", // Cornish
];

// rank index once (first occurrence wins)
const kAiLangRank = new Map(kAiLangPopularityOrder.map((code, i) => [code, i]));

function aiLangRank(lang: string): number {
  return kAiLangRank.get(lang) ?? 10_000 + lang.charCodeAt(0);
}

/** Sort language codes by static popularity (Polish first). */
function sortLangsByPopularity(langs: Iterable<string>): string[] {
  return [...new Set(langs)].filter((l) => l !== "en").sort((a, b) => {
    const ra = aiLangRank(a);
    const rb = aiLangRank(b);
    if (ra !== rb) return ra - rb;
    return a.localeCompare(b);
  });
}

function findMissing(
  pt: ParsedTranslations,
  langFilter: Set<string> | null,
  // if set, also check these langs even when the download has no rows for them yet
  ensureLangs: string[] | null = null,
): Map<string, string[]> {
  // lang -> list of english strings missing a translation
  const { perLang, allStrings } = pt;
  const missing = new Map<string, string[]>();
  const langs = new Set<string>([...perLang.keys()]);
  if (ensureLangs) {
    for (const lang of ensureLangs) langs.add(lang);
  }
  for (const lang of langs) {
    if (langFilter && !langFilter.has(lang)) continue;
    if (lang === "en") continue;
    const translations = perLang.get(lang) ?? new Map<string, string>();
    const list: string[] = [];
    for (const s of allStrings) {
      if (!translations.has(s)) {
        list.push(s);
      }
    }
    if (list.length > 0) {
      missing.set(lang, list);
    }
  }
  return missing;
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

async function submitMany(
  server: string,
  secret: string,
  user: string,
  lang: string,
  pairs: Map<string, string>,
  maxSubmit: number,
  submittedSoFar: { n: number },
): Promise<number> {
  let n = 0;
  for (const [english, translation] of pairs) {
    if (maxSubmit > 0 && submittedSoFar.n >= maxSubmit) {
      console.log(`--max-submit ${maxSubmit} reached, stopping submissions`);
      break;
    }
    await submitTranslation(server, secret, user, lang, english, translation);
    n++;
    submittedSoFar.n++;
  }
  return n;
}

// Prefer small batches: long UI strings + non-Latin output often truncate at max_tokens.
const kAiBatchSize = 12;

function buildTranslatePrompt(stringsToTranslate: string[], langCode: string): string {
  // Array-in / array-out: same order, no English keys (avoids escape/match bugs
  // with ·, quotes, HTML, etc. that break object-map responses).
  return `Translate each English UI string to the language for locale code "${langCode}".
Input is a JSON array of English strings.
Return ONLY a JSON array of translated strings, same length and same order as the input.
Keep placeholders like %s, %d, %1, etc. unchanged.
Keep access-key ampersands if present (e.g. "&File" → localized with & before the hotkey letter).
Do not wrap in markdown. No explanation.

Input:
${JSON.stringify(stringsToTranslate)}`;
}

/** Extract a JSON value from model text (strips fences; tolerates leading prose). */
function extractJsonText(text: string): string {
  let s = text.trim();
  // full fence
  const fence = s.match(/```(?:json)?\s*([\s\S]*?)```/);
  if (fence) {
    s = fence[1].trim();
  } else if (s.startsWith("```")) {
    // truncated fence opening only
    s = s.replace(/^```(?:json)?\s*/i, "");
    s = s.replace(/```\s*$/, "").trim();
  }
  // Claude sometimes emits \xHH (not valid JSON); convert to \u00HH
  s = s.replace(/\\x([0-9a-fA-F]{2})/g, (_m, hex: string) => `\\u00${hex}`);
  // prefer array (our prompt); fall back to object for older responses
  const arrStart = s.indexOf("[");
  const objStart = s.indexOf("{");
  if (arrStart >= 0 && (objStart < 0 || arrStart < objStart)) {
    const end = s.lastIndexOf("]");
    if (end > arrStart) return s.slice(arrStart, end + 1);
  }
  if (objStart >= 0) {
    const end = s.lastIndexOf("}");
    if (end > objStart) return s.slice(objStart, end + 1);
  }
  return s;
}

function parseTranslationJson(
  text: string,
  strings: string[],
  stringsToTranslate: string[],
  langCode: string,
): Map<string, string> {
  const jsonStr = extractJsonText(text);
  let parsed: unknown;
  try {
    parsed = JSON.parse(jsonStr);
  } catch (e) {
    console.log(`AI response (invalid JSON, first 500 chars): ${text.slice(0, 500)}`);
    throw new Error(`Invalid JSON in AI response for lang ${langCode}: ${e}`);
  }

  const result = new Map<string, string>();

  if (Array.isArray(parsed)) {
    if (parsed.length !== strings.length) {
      throw new Error(
        `AI array length ${parsed.length} != input ${strings.length} for lang ${langCode}`,
      );
    }
    for (let i = 0; i < strings.length; i++) {
      const v = parsed[i];
      if (typeof v !== "string" || v.length === 0) {
        console.log(`  skip empty/non-string translation at index ${i}`);
        continue;
      }
      result.set(strings[i], v);
    }
    return result;
  }

  // legacy object map fallback
  if (parsed && typeof parsed === "object") {
    for (const [key, value] of Object.entries(parsed as Record<string, unknown>)) {
      if (typeof value !== "string") continue;
      let idx = stringsToTranslate.indexOf(key);
      if (idx < 0) idx = strings.indexOf(key);
      if (idx < 0) {
        // keys may have had & stripped or \\x decoded differently
        const keyNorm = key.replaceAll("&", "");
        idx = stringsToTranslate.findIndex((s) => s === keyNorm || s.replaceAll("&", "") === keyNorm);
      }
      if (idx < 0) {
        console.log(`  skip unexpected key: '${key.slice(0, 60)}'`);
        continue;
      }
      result.set(strings[idx], value);
    }
    return result;
  }

  throw new Error(`AI response is neither array nor object for lang ${langCode}`);
}

async function translateWithClaude(
  apiKey: string,
  strings: string[],
  langCode: string,
): Promise<Map<string, string>> {
  const stringsToTranslate = strings.map((s) => s.replaceAll("&", ""));
  const prompt = buildTranslatePrompt(stringsToTranslate, langCode);

  const resp = await fetch("https://api.anthropic.com/v1/messages", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "x-api-key": apiKey,
      "anthropic-version": "2023-06-01",
    },
    body: JSON.stringify({
      model: "claude-sonnet-4-5-20250929",
      max_tokens: 16384,
      messages: [{ role: "user", content: prompt }],
    }),
  });

  if (!resp.ok) {
    const body = await resp.text();
    throw new Error(`Claude API error: ${resp.status} ${resp.statusText}: ${body}`);
  }

  const data = (await resp.json()) as {
    content: { type: string; text?: string }[];
    stop_reason?: string;
  };
  if (data.stop_reason === "max_tokens") {
    throw new Error(`Claude truncated response (max_tokens) for lang ${langCode}, batch ${strings.length}`);
  }
  const text = data.content?.find((c) => c.type === "text")?.text ?? data.content?.[0]?.text ?? "";
  return parseTranslationJson(text, strings, stringsToTranslate, langCode);
}

async function translateWithGrok(
  apiKey: string,
  strings: string[],
  langCode: string,
): Promise<Map<string, string>> {
  const stringsToTranslate = strings.map((s) => s.replaceAll("&", ""));
  const prompt = buildTranslatePrompt(stringsToTranslate, langCode);

  const resp = await fetch("https://api.x.ai/v1/chat/completions", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      Authorization: `Bearer ${apiKey}`,
    },
    body: JSON.stringify({
      model: "grok-3-latest",
      messages: [{ role: "user", content: prompt }],
      temperature: 0.2,
      max_tokens: 16384,
    }),
  });

  if (!resp.ok) {
    const body = await resp.text();
    throw new Error(`Grok API error: ${resp.status} ${resp.statusText}: ${body}`);
  }

  const data = (await resp.json()) as {
    choices: { message: { content: string }; finish_reason?: string }[];
  };
  if (data.choices?.[0]?.finish_reason === "length") {
    throw new Error(`Grok truncated response (length) for lang ${langCode}, batch ${strings.length}`);
  }
  const text = data.choices?.[0]?.message?.content ?? "";
  return parseTranslationJson(text, strings, stringsToTranslate, langCode);
}

/** Translate a batch; on truncation/parse failure, split and retry. */
async function translateBatchWithRetry(
  ai: AiProvider,
  apiKey: string,
  strings: string[],
  langCode: string,
): Promise<Map<string, string>> {
  if (strings.length === 0) return new Map();
  const run =
    ai === "grok"
      ? () => translateWithGrok(apiKey, strings, langCode)
      : () => translateWithClaude(apiKey, strings, langCode);
  try {
    return await run();
  } catch (e) {
    if (strings.length <= 1) throw e;
    const mid = Math.ceil(strings.length / 2);
    console.log(
      `  retry ${langCode}: split batch of ${strings.length} → ${mid}+${strings.length - mid} (${e})`,
    );
    const left = await translateBatchWithRetry(ai, apiKey, strings.slice(0, mid), langCode);
    const right = await translateBatchWithRetry(ai, apiKey, strings.slice(mid), langCode);
    for (const [k, v] of right) left.set(k, v);
    return left;
  }
}

async function fillMissingWithAiAndSubmit(
  args: CliArgs,
  secret: string,
  pt: ParsedTranslations,
  autoAdded: Map<string, Map<string, string>>,
  ensureLangs: string[] | null,
): Promise<number> {
  const user = args.ai === "grok" ? "ai grok" : "ai claude";
  const submitted = { n: 0 };

  // submit auto-derived no-prefix translations in popularity order
  for (const lang of sortLangsByPopularity(autoAdded.keys())) {
    if (args.langs && !args.langs.has(lang)) continue;
    if (args.maxSubmit > 0 && submitted.n >= args.maxSubmit) break;
    const pairs = autoAdded.get(lang)!;
    const n = await submitMany(args.server, secret, "ai auto", lang, pairs, args.maxSubmit, submitted);
    if (n > 0) {
      console.log(`submitted ${n} auto no-prefix translations for ${lang}`);
    }
  }

  if (args.ai === "none") {
    return submitted.n;
  }

  let apiKey = "";
  if (args.ai === "claude") {
    apiKey = readSecret("CLAUDE_API_KEY", false);
    if (!apiKey) {
      console.log("CLAUDE_API_KEY not set, skipping AI translations");
      return submitted.n;
    }
  } else if (args.ai === "grok") {
    apiKey = readSecret("XAI_API_KEY", false) || readSecret("GROK_API_KEY", false);
    if (!apiKey) {
      console.log("XAI_API_KEY / GROK_API_KEY not set, skipping AI translations");
      return submitted.n;
    }
  }

  // autoAdd already mutated pt; find what is still missing on our in-memory map
  const missing = findMissing(pt, args.langs, ensureLangs);
  let totalMissing = 0;
  for (const list of missing.values()) totalMissing += list.length;
  const langsInOrder = sortLangsByPopularity(missing.keys());
  console.log(
    `missing translations after auto-fill: ${totalMissing} across ${missing.size} languages`,
  );
  console.log(`AI order (most popular first, pl first): ${langsInOrder.join(", ")}`);

  // fully translate one language before moving to the next
  for (const lang of langsInOrder) {
    if (args.maxSubmit > 0 && submitted.n >= args.maxSubmit) break;
    const currentMissing = missing.get(lang) ?? [];
    if (currentMissing.length === 0) continue;

    console.log(`Lang ${lang}: ${currentMissing.length} missing — finishing this language first`);

    for (let i = 0; i < currentMissing.length; i += kAiBatchSize) {
      if (args.maxSubmit > 0 && submitted.n >= args.maxSubmit) {
        console.log(`  --max-submit reached mid-language ${lang}; remaining stay for next run`);
        break;
      }
      const batch = currentMissing.slice(i, i + kAiBatchSize);
      console.log(
        `  translating ${batch.length} strings to ${lang} (${i + 1}-${Math.min(i + batch.length, currentMissing.length)} of ${currentMissing.length}) with ${args.ai}...`,
      );
      try {
        const translated = await translateBatchWithRetry(args.ai, apiKey, batch, lang);
        if (translated.size === 0) {
          console.log(`  no translations returned for batch`);
          continue;
        }
        const n = await submitMany(
          args.server,
          secret,
          user,
          lang,
          translated,
          args.maxSubmit,
          submitted,
        );
        console.log(`  submitted ${n} translations for ${lang}`);
        // update in-memory so we don't re-translate
        let m = pt.perLang.get(lang);
        if (!m) {
          m = new Map();
          pt.perLang.set(lang, m);
        }
        for (const [en, tr] of translated) {
          m.set(en, tr);
        }
      } catch (e) {
        console.error(`  error translating to ${lang}: ${e}`);
      }
    }
  }

  console.log(`total submissions this run: ${submitted.n}`);
  return submitted.n;
}

// Write languages complete enough for the binary to .work/translations.txt
function writeTranslationsForBinary(pt: ParsedTranslations, downloadSha1: string): void {
  const { perLang, allStrings } = pt;
  const nStrings = allStrings.length;
  const langsToSkip = new Set<string>();
  const goodLangs: string[] = [];
  const fullyTranslated: string[] = [];
  const notTranslated: string[] = [];

  // skip languages that are mostly untranslated. Threshold scales with string count
  // (historically 180 when there were ~500 strings ≈ 35%).
  const skipIfMissingMoreThan = Math.max(180, Math.floor(nStrings * 0.35));
  for (const [lang, m] of perLang) {
    const nMissing = nStrings - m.size;
    let skipStr = "";
    if (nMissing > skipIfMissingMoreThan) {
      skipStr = "  SKIP";
      langsToSkip.add(lang);
      notTranslated.push(lang);
    } else if (nMissing === 0) {
      fullyTranslated.push(lang);
    } else {
      goodLangs.push(lang);
    }
    if (nMissing > 0) {
      console.log(`Lang ${lang}, missing: ${nMissing}${skipStr}`);
    }
  }
  console.log(`skip languages with more than ${skipIfMissingMoreThan} missing of ${nStrings}`);

  allStrings.sort();
  // header: AppTranslator + sha1 of the full download (for cache comparison);
  // ParseTranslationsTxt() skips the first 2 lines
  const out: string[] = [`AppTranslator: ${APP_NAME}`, downloadSha1];

  const sortedLangs = [...perLang.keys()].filter((lang) => !langsToSkip.has(lang)).sort();

  for (const s of allStrings) {
    out.push(":" + s);
    for (const lang of sortedLangs) {
      const m = perLang.get(lang)!;
      const trans = m.get(s);
      if (!trans) continue;
      if (trans.includes("\n")) throw new Error(`translation contains newline`);
      out.push(lang + ":" + trans);
    }
  }

  mkdirSync(workDir, { recursive: true });
  const content = out.join("\n") + "\n";
  writeFileSync(translationsTxtPath, content, "utf-8");
  console.log(`Wrote ${translationsTxtPath} of size ${content.length}`);
  console.log(`not translated langs: ${notTranslated}`);
  console.log(`good langs: ${goodLangs}`);
  console.log(`fully translated langs: ${fullyTranslated}`);
}

function makeLzsa(): void {
  if (!existsSync(translationsTxtPath)) {
    throw new Error(`missing ${translationsTxtPath}; run without --skip-lzsa after download`);
  }
  const makeLzsaExe = join("bin", "MakeLZSA.exe");
  const lzsaArgs = [translationsLzsaPath, `${translationsTxtPath}:translations.txt`];
  console.log(`Running ${makeLzsaExe} ${lzsaArgs.join(" ")}`);
  const res = spawnSync(makeLzsaExe, lzsaArgs, { stdio: "inherit" });
  if (res.status !== 0) {
    throw new Error(`MakeLZSA failed with exit code ${res.status}`);
  }
  console.log(`Wrote ${translationsLzsaPath}`);
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  console.log(`apptranslator server: ${args.server}`);
  console.log(`ai: ${args.ai}`);

  mkdirSync(workDir, { recursive: true });

  const secret = tryGetTransSecret();
  if (!secret) {
    console.log("TRANS_UPLOAD_SECRET not set; skipping download/AI");
    writeEmptyTranslations();
    if (!args.skipLzsa) {
      makeLzsa();
    }
    return;
  }

  const strs = extractStringsToTranslate();
  strs.sort();

  const localSha1 = readLocalSha1();
  if (localSha1) {
    console.log(`local cache sha1: ${localSha1} (${translationsTxtPath})`);
  } else {
    console.log(`no local cache at ${translationsTxtPath}`);
  }

  // 1) download
  let dl = await downloadTranslations(args.server, strs, secret);
  if (localSha1 && dl.sha1 === localSha1) {
    console.log("sha1 matches local cache (translations body unchanged for these strings)");
  }

  // fix trailing junk on translation lines
  const { fixed: fixedBody, badTranslations } = fixTranslations(dl.body);
  // rebuild full text with same header + (possibly fixed) body
  let fullText = `AppTranslator: ${APP_NAME}\n${dl.sha1}\n${fixedBody}\n`;

  // parse + auto-fill no-prefix candidates
  let pt = parseTranslations(fullText);
  const autoAdded = autoAddNoPrefixTranslations(pt);

  // langs we care about filling: those already present on the server, plus
  // any --lang filter. When the store is empty (local), require --lang.
  const supportedLangs = loadSupportedLangCodes();
  const ensureLangs =
    args.langs && args.langs.size > 0
      ? [...args.langs]
      : pt.perLang.size > 0
        ? [...pt.perLang.keys()]
        : null;

  // missing after in-memory auto-fill (auto-added still need server submit)
  const missingAfterAuto = findMissing(pt, args.langs, ensureLangs);
  let nMissing = 0;
  for (const list of missingAfterAuto.values()) nMissing += list.length;
  let nAuto = 0;
  for (const m of autoAdded.values()) nAuto += m.size;

  console.log(
    `missing after auto no-prefix: ${nMissing} across ${missingAfterAuto.size} langs (auto-derived to submit: ${nAuto}); supported=${supportedLangs.length}`,
  );

  if (args.ai === "none") {
    if (nMissing > 0 || nAuto > 0) {
      console.log(
        `skipping submit (--no-ai / --ai=none); re-run with --ai=claude|grok to fill ${nMissing + nAuto} missing`,
      );
    }
  } else if (nMissing > 0 || nAuto > 0) {
    // 2) AI translate + submit (and auto no-prefix submit)
    await fillMissingWithAiAndSubmit(args, secret, pt, autoAdded, ensureLangs);

    // 3) re-download so cache is pure server truth
    console.log("re-downloading after submissions...");
    dl = await downloadTranslations(args.server, strs, secret);
    const fixed2 = fixTranslations(dl.body);
    fullText = `AppTranslator: ${APP_NAME}\n${dl.sha1}\n${fixed2.fixed}\n`;
    pt = parseTranslations(fullText);
    // local-only auto no-prefix for good subset (already on server if submitted above)
    autoAddNoPrefixTranslations(pt);
  }

  printBadTranslations(args.server, badTranslations);

  // 4) write filtered translations for the binary + lzsa
  //    (sha1 line is the full download sha1 for cache comparison on next run)
  writeTranslationsForBinary(pt, dl.sha1);
  if (!args.skipLzsa) {
    makeLzsa();
  }
}

await main();
