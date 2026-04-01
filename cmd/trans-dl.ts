import { readFileSync, writeFileSync, readdirSync, appendFileSync, existsSync } from "node:fs";
import { join, extname } from "node:path";
import { spawnSync } from "node:child_process";
import { commands } from "./gen-commands";

// strings that should not be sent for translation
// (e.g. command names whose display text is set dynamically)
const translationBlacklist: string[] = [
  "Toggle Windows Previewer",
  "Toggle Windows Search Filter",
];

const apptranslatorServer = "https://www.apptranslator.org";
const translationsDir = "translations";
const translationsTxtPath = join(translationsDir, "translations.txt");

const translationPattern = /\b_TR[AN]?\("(.*?)"\)/g;

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
    const strs = extractTranslations(content);
    allStrs.push(...strs);
  }
  // uniquify
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
  // verify blacklisted strings are actually in the list before removing them
  for (const bl of translationBlacklist) {
    if (!unique.includes(bl)) {
      throw new Error(`Blacklisted string "${bl}" not found in strings to translate`);
    }
  }
  const blacklistSet = new Set(translationBlacklist);
  const filtered = unique.filter((s) => !blacklistSet.has(s));
  console.log(`${filtered.length} unique strings to translate (including command descriptions, ${translationBlacklist.length} blacklisted)`);
  return filtered;
}

function getTransSecret(): string {
  // try reading from secrets file first
  const secretsPath = join("..", "hack", "secrets", "sumatrapdf.env");
  try {
    const content = readFileSync(secretsPath, "utf-8");
    for (const line of content.split("\n")) {
      const trimmed = line.trim();
      if (trimmed.startsWith("TRANS_UPLOAD_SECRET")) {
        const eqIdx = trimmed.indexOf("=");
        if (eqIdx !== -1) {
          const val = trimmed.substring(eqIdx + 1).trim();
          if (val.length >= 4) {
            console.log("Got TRANS_UPLOAD_SECRET");
            return val;
          }
        }
      }
    }
  } catch {
    // fall through to env variable
  }
  const val = process.env["TRANS_UPLOAD_SECRET"] ?? "";
  if (val.length < 4) {
    throw new Error("must set TRANS_UPLOAD_SECRET env variable or in .env file");
  }
  return val;
}

async function downloadTranslationsMust(): Promise<string> {
  const timeStart = performance.now();
  const strs = extractStringsToTranslate();
  strs.sort();
  console.log(`uploading ${strs.length} strings for translation`);
  const secret = getTransSecret();
  const uri = `${apptranslatorServer}/api/dltransfor?app=SumatraPDF&secret=${secret}`;
  const body = strs.join("\n");
  const resp = await fetch(uri, { method: "POST", body });
  if (!resp.ok) {
    throw new Error(`HTTP ${resp.status}: ${resp.statusText}`);
  }
  const text = await resp.text();
  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`downloadTranslations() finished in ${elapsed}s`);
  return text;
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
  // remove last \n (join adds none at end, matching Go behavior of trimming last byte)
  return { fixed: result.join("\n"), badTranslations };
}

function printBadTranslations(badTranslations: BadTranslation[]): void {
  badTranslations.sort((a, b) => a.orig.localeCompare(b.orig));
  let currLang = "";
  for (const bt of badTranslations) {
    const lang = bt.orig.split(":")[0];
    if (lang !== currLang) {
      currLang = lang;
      const uri = `https://www.apptranslator.org/app/SumatraPDF/${lang}`;
      console.log(`\n${uri}`);
    }
    console.log(`${bt.currString}\n  '${bt.orig}' => '${bt.fixed}'`);
  }
}

interface ParsedTranslations {
  perLang: Map<string, Map<string, string>>;
  allStrings: string[];
}

function parseTranslations(d: string): ParsedTranslations {
  const lines = d.split("\n");
  const a = lines.slice(2);
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

function autoAddNoPrefixTranslations(pt: ParsedTranslations): void {
  const { perLang, allStrings } = pt;
  let nAdded = 0;
  for (const [lang, translations] of perLang) {
    // build cache: for translated strings containing "&",
    // map stripped_original => stripped_translation
    const cache = new Map<string, string>();
    for (const [orig, trans] of translations) {
      if (orig.includes("&")) {
        cache.set(stripAmpersand(orig), stripAmpersand(trans));
      }
    }
    // for untranslated strings without "&", look up in cache
    for (const s of allStrings) {
      if (s.includes("&")) continue;
      if (translations.has(s)) continue;
      const trans = cache.get(s);
      if (trans) {
        translations.set(s, trans);
        nAdded++;
      }
    }
  }
  if (nAdded > 0) {
    console.log(`autoAddNoPrefixTranslations: added ${nAdded} translations`);
  }
}

const aiTranslationsPath = join(translationsDir, "translations-from-ai.txt");

function getClaudeApiKey(): string {
  const secretsPath = join("..", "hack", "secrets", "sumatrapdf.env");
  try {
    const content = readFileSync(secretsPath, "utf-8");
    for (const line of content.split("\n")) {
      const trimmed = line.trim();
      if (trimmed.startsWith("CLAUDE_API_KEY")) {
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
    // fall through to env variable
  }
  const val = process.env["CLAUDE_API_KEY"] ?? "";
  if (val.length < 4) {
    throw new Error("must set CLAUDE_API_KEY env variable or in secrets file");
  }
  return val;
}

// load cached AI translations from file
// format: :<english>\n<lang>:<translation>\n
function loadAITranslationsCache(): Map<string, Map<string, string>> {
  const cache = new Map<string, Map<string, string>>();
  if (!existsSync(aiTranslationsPath)) {
    return cache;
  }
  const content = readFileSync(aiTranslationsPath, "utf-8");
  const lines = content.split("\n");
  let currString = "";
  for (const line of lines) {
    if (line.length === 0) continue;
    if (line.startsWith(":")) {
      currString = line.substring(1);
      continue;
    }
    const colonIdx = line.indexOf(":");
    if (colonIdx === -1) continue;
    const lang = line.substring(0, colonIdx);
    const trans = line.substring(colonIdx + 1);
    let m = cache.get(lang);
    if (!m) {
      m = new Map();
      cache.set(lang, m);
    }
    m.set(currString, trans);
  }
  return cache;
}

function appendToAICache(english: string[], lang: string, translations: Map<string, string>): void {
  let content = "";
  for (const s of english) {
    const trans = translations.get(s);
    if (!trans) continue;
    content += `:${s}\n${lang}:${trans}\n`;
  }
  if (content.length > 0) {
    appendFileSync(aiTranslationsPath, content, "utf-8");
  }
}

async function translateWithClaude(apiKey: string, strings: string[], langCode: string): Promise<Map<string, string>> {
  let stringsToTranslate = strings.map((s) => s.replaceAll("&", ""));
  const stringsJson = JSON.stringify(stringsToTranslate);
  const prompt = `Translate the following English UI strings to the language identified by locale code "${langCode}". Return ONLY a JSON object mapping each English string to its translation. No explanation, no markdown formatting, just the JSON object.\n\n${stringsJson}`;

  const resp = await fetch("https://api.anthropic.com/v1/messages", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
      "x-api-key": apiKey,
      "anthropic-version": "2023-06-01",
    },
    body: JSON.stringify({
      model: "claude-sonnet-4-5-20250929",
      max_tokens: 4096,
      messages: [{ role: "user", content: prompt }],
    }),
  });

  if (!resp.ok) {
    const body = await resp.text();
    throw new Error(`Claude API error: ${resp.status} ${resp.statusText}: ${body}`);
  }

  const data = (await resp.json()) as { content: { text: string }[] };
  const text = data.content[0].text;

  // extract JSON from response
  const jsonMatch = text.match(/\{[\s\S]*\}/);
  if (!jsonMatch) {
    console.log(`Claude response (no JSON found): ${text}`);
    throw new Error(`No JSON found in Claude response for lang ${langCode}`);
  }

  let parsed: Record<string, string>;
  try {
    parsed = JSON.parse(jsonMatch[0]);
  } catch (e) {
    console.log(`Claude response (invalid JSON): ${text}`);
    throw new Error(`Invalid JSON in Claude response for lang ${langCode}: ${e}`);
  }

  const result = new Map<string, string>();
  for (const [key, value] of Object.entries(parsed)) {
    if (typeof value === "string") {
      let idx = stringsToTranslate.indexOf(key);
      if (idx < 0) throw new Error(`Claude returned unexpected key: '${key}' for lang ${langCode}`);
      let origStr = strings[idx];
      let verif = origStr.replaceAll("&", "");
      if (verif != key) {
        throw new Error(`Claude returned key '${key}' that doesn't match original '${origStr}' (lang ${langCode})`);
      }
      result.set(origStr, value);
    }
  }
  return result;
}

async function addTranslationsFromAI(pt: ParsedTranslations): Promise<void> {
  const { perLang, allStrings } = pt;

  let apiKey: string;
  try {
    apiKey = getClaudeApiKey();
  } catch {
    console.log("CLAUDE_API_KEY not set, skipping AI translations");
    return;
  }

  // load cache of previously translated strings
  const aiCache = loadAITranslationsCache();

  let nFromCache = 0;
  let nFromApi = 0;

  for (const [lang, translations] of perLang) {
    // find untranslated strings without "&"
    const missing: string[] = [];
    for (const s of allStrings) {
      if (translations.has(s)) continue;
      // check AI cache first
      const cached = aiCache.get(lang)?.get(s);
      if (cached) {
        translations.set(s, cached);
        nFromCache++;
        continue;
      }
      missing.push(s);
    }

    if (missing.length === 0) continue;

    // batch in groups of 48
    for (let i = 0; i < missing.length; i += 48) {
      const batch = missing.slice(i, i + 48);
      console.log(
        `Translating ${batch.length} strings to ${lang} (${i + 1}-${i + batch.length} of ${missing.length})...`,
      );
      try {
        const result = translateWithClaude(apiKey, batch, lang);
        const translated = await result;
        for (const [english, trans] of translated) {
          translations.set(english, trans);
          // update in-memory cache
          let langCache = aiCache.get(lang);
          if (!langCache) {
            langCache = new Map();
            aiCache.set(lang, langCache);
          }
          langCache.set(english, trans);
          nFromApi++;
        }
        // append to cache file immediately
        appendToAICache(batch, lang, translated);
      } catch (e) {
        console.error(`Error translating to ${lang}: ${e}`);
        // continue with next batch / language
      }
    }
  }

  if (nFromCache > 0 || nFromApi > 0) {
    console.log(`AI translations: ${nFromCache} from cache, ${nFromApi} from API`);
  }
}

function generateGoodSubset(pt: ParsedTranslations): void {
  const { perLang, allStrings } = pt;
  const nStrings = allStrings.length;
  const langsToSkip = new Set<string>();
  const goodLangs: string[] = [];
  const fullyTranslated: string[] = [];
  const notTranslated: string[] = [];

  for (const [lang, m] of perLang) {
    const nMissing = nStrings - m.size;
    let skipStr = "";
    if (nMissing > 180) {
      skipStr = "  SKIP";
      langsToSkip.add(lang);
      notTranslated.push(lang);
    } else {
      if (nMissing === 0) {
        fullyTranslated.push(lang);
      } else {
        goodLangs.push(lang);
      }
    }
    if (nMissing > 0) {
      console.log(`Lang ${lang}, missing: ${nMissing}${skipStr}`);
    }
  }

  // write translations-good.txt with langs that don't miss too many translations
  allStrings.sort();
  // for backwards compat first 2 lines are skipped by ParseTranslationsTxt()
  const out: string[] = ["AppTranslator: SumatraPDF", "AppTranslator: SumatraPDF"];

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

  const content = out.join("\n");
  const path = join(translationsDir, "translations-good.txt");
  writeFileSync(path, content, "utf-8");
  console.log(`Wrote ${path} of size ${content.length}`);
  console.log(`not translated langs: ${notTranslated}`);
  console.log(`good langs: ${goodLangs}`);
  console.log(`fully translated langs: ${fullyTranslated}`);
}

async function main() {
  const d = await downloadTranslationsMust();
  const { fixed, badTranslations } = fixTranslations(d);

  const path = join(translationsDir, "translations.txt");
  const curr = readFileSync(path, "utf-8");
  if (fixed === curr) {
    console.log("Translations didn't change");
  }

  const pt = parseTranslations(fixed);
  autoAddNoPrefixTranslations(pt);
  await addTranslationsFromAI(pt);
  generateGoodSubset(pt);

  writeFileSync(path, fixed, "utf-8");
  console.log(`Wrote ${path} of size ${fixed.length}`);

  printBadTranslations(badTranslations);

  // compress translations-good.txt into .lzsa archive
  const lzsaPath = join(translationsDir, "translations.txt.lzsa");
  const goodPath = join(translationsDir, "translations-good.txt");
  const makeLzsa = join("bin", "MakeLZSA.exe");
  const lzsaArgs = [lzsaPath, `${goodPath}:translations-good.txt`];
  console.log(`Running ${makeLzsa} ${lzsaArgs.join(" ")}`);
  const res = spawnSync(makeLzsa, lzsaArgs, { stdio: "inherit" });
  if (res.status !== 0) {
    throw new Error(`MakeLZSA failed with exit code ${res.status}`);
  }
  console.log(`Wrote ${lzsaPath}`);
}

await main();
