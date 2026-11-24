// Looks for access key collisions in menus and dialogs, in every translation.
//
// Items shown together are marked in the sources as
//   //[ ACCESSKEY_GROUP <name>
//   ... TrN("&Item") ... TrN("&Another") ...
//   //] ACCESSKEY_GROUP <name>
// Within a group, items that never appear together (only one of them is
// shown at a time) may share an access key when listed as alternatives:
//   //[ ACCESSKEY_ALTERNATIVE
//   ... TrN("&Foo") ...
//   //| ACCESSKEY_ALTERNATIVE
//   ... TrN("&Bar") ...
//   //] ACCESSKEY_ALTERNATIVE
// ng's own files (src/ng) have their own groups, apart from orig's same-named ones.
//
// Usage: bun cmd/check-access-keys.ts [-all] [-fix] [-dry] [-local | -server <url>]
//   -all: also list translations that dropped the original's access key
//   -fix: move the '&' of clashing or broken translations and submit them to apptranslator
//   -dry: print what -fix would change (string, language, translation before and after)
import { readFileSync, readdirSync, existsSync } from "node:fs";
import { join, extname } from "node:path";
import { gLangs } from "./regenerate-translation-languages.ts";
import { DEFAULT_SERVER, LOCAL_SERVER, submitTranslation, tryGetTransSecret } from "./helper/apptranslator.ts";

interface Translation {
  text: string;
  lang: string;
  translation: string;
}

function parseTranslations(s: string): Map<string, Translation[]> {
  const res = new Map<string, Translation[]>();
  const allLines = s.split("\n");
  // skip first 2 lines (header)
  let lines = allLines.slice(2);
  // trim empty lines from end
  while (lines.length > 0 && lines[lines.length - 1].trim() === "") {
    lines.pop();
  }
  let currStr = "";
  let currTranslations: Translation[] = [];
  for (const l of lines) {
    if (l.length === 0) continue;
    if (l[0] === ":") {
      if (currStr !== "" && currTranslations.length > 0) {
        res.set(currStr, currTranslations);
      }
      currStr = l.substring(1);
      currTranslations = [];
    } else {
      const colonIdx = l.indexOf(":");
      if (colonIdx === -1) continue;
      const lang = l.substring(0, colonIdx);
      const trans = l.substring(colonIdx + 1);
      currTranslations.push({ text: currStr, lang, translation: trans });
    }
  }
  if (currStr !== "" && currTranslations.length > 0) {
    res.set(currStr, currTranslations);
  }
  return res;
}

const translationPattern = /\b(?:TrN|Tr)\("(.*?)"\)/g;

function extractTranslationStrings(s: string): string[] {
  const res: string[] = [];
  for (const match of s.matchAll(translationPattern)) {
    res.push(match[1]);
  }
  return res;
}

function getFilesToProcess(dir = "src", res: string[] = []): string[] {
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const path = join(dir, entry.name);
    if (entry.isDirectory()) {
      getFilesToProcess(path, res);
    } else if (extname(entry.name).toLowerCase() === ".cpp") {
      res.push(path);
    }
  }
  return res;
}

// orig and ng both use src/shared; ng's own menus (src/ng) reuse orig's group
// names, e.g. "Context Menu (Main)", and must not be merged with them
function groupKey(path: string, name: string): string {
  const isNg = path.split(/[\\/]/)[1] === "ng";
  return isNg ? `${name} (ng)` : name;
}

// [alternative block index, branch index] of a string within its group
type Alternative = [number, number];

interface AccessGroup {
  strings: string[];
  alternatives: Map<string, Alternative>;
}

const kGroupMarker = "ACCESSKEY_GROUP ";
const kAltMarker = "ACCESSKEY_ALTERNATIVE";

// "[", "|" or "]" of a "//[ MARKER" line, null when not a marker line
function markerKind(line: string, marker: string): string | null {
  if (!line.startsWith("//")) return null;
  const kind = line[2];
  if (kind !== "[" && kind !== "|" && kind !== "]") return null;
  if (line.substring(4, 4 + marker.length) !== marker) return null;
  return kind;
}

// numbers ACCESSKEY_ALTERNATIVE blocks; a group can span files, so per file isn't enough
let gAltBlocks = 0;

function extractAccesskeyGroups(path: string, groups: Map<string, AccessGroup>): void {
  const lines = readFileSync(path, "utf-8").split(/\r?\n/);
  let groupName = "";
  let group: AccessGroup | null = null;
  let branch: Alternative | null = null;

  for (let line of lines) {
    line = line.trim();
    const groupKind = markerKind(line, kGroupMarker);
    if (groupKind) {
      const newName = line.substring(4 + kGroupMarker.length).trim();
      if (groupKind === "[") {
        if (group) throw new Error(`${path}: group '${groupName}' doesn't end before group '${newName}' starts`);
        groupName = newName;
        const key = groupKey(path, groupName);
        group = groups.get(key) || { strings: [], alternatives: new Map() };
        groups.set(key, group);
        continue;
      }
      if (!group) throw new Error(`${path}: unexpected end of group '${newName}'`);
      if (groupName !== newName) throw new Error(`${path}: group end mismatch: '${newName}' != '${groupName}'`);
      if (branch) throw new Error(`${path}: ${kAltMarker} not closed in group '${groupName}'`);
      group = null;
      continue;
    }

    const altKind = markerKind(line, kAltMarker);
    if (altKind) {
      if (!group) throw new Error(`${path}: ${kAltMarker} outside of a group`);
      const next = line[4 + kAltMarker.length];
      if (next !== undefined && !/\s/.test(next)) throw new Error(`${path}: typo in '${line}'?`);
      if (altKind === "[") {
        if (branch) throw new Error(`${path}: nested ${kAltMarker} isn't supported`);
        gAltBlocks++;
        branch = [gAltBlocks, 0];
      } else if (altKind === "|") {
        if (!branch) throw new Error(`${path}: unexpected '//| ${kAltMarker}'`);
        branch = [branch[0], branch[1] + 1];
      } else {
        if (!branch) throw new Error(`${path}: unexpected '//] ${kAltMarker}'`);
        // without a '//|' every string is in the same branch: nothing is exclusive
        if (branch[1] === 0) throw new Error(`${path}: ${kAltMarker} block without '//| ${kAltMarker}'`);
        branch = null;
      }
      continue;
    }

    if (!group) continue;
    for (const str of extractTranslationStrings(line)) {
      const n = (str.match(/&/g) || []).length;
      if (n > 1) throw new Error(`${path}: more than one '&' in "${str}"`);
      if (!group.strings.includes(str)) {
        group.strings.push(str);
      }
      if (branch) {
        group.alternatives.set(str, branch);
      }
    }
  }
  if (group) throw new Error(`${path}: group '${groupName}' isn't closed`);
}

function isLetterOrDigit(s: string): boolean {
  return /^[\p{L}\p{N}]$/u.test(s);
}

// sharing a key is fine only between different branches of the same
// alternative block: those are never shown together
function areExclusive(a: Alternative | undefined, b: Alternative | undefined): boolean {
  return !!a && !!b && a[0] === b[0] && a[1] !== b[1];
}

interface KeyUse {
  str: string;
  trans: string;
}

function translate(str: string, langCode: string, translations: Map<string, Translation[]>): string {
  for (const item of translations.get(str) || []) {
    if (item.lang === langCode) return item.translation;
  }
  return str;
}

// the issues found in one group for one language, empty when none
function checkGroup(
  name: string,
  group: AccessGroup,
  langCode: string,
  translations: Map<string, Translation[]>,
  showDropped: boolean,
): string[] {
  const warnings: string[] = [];
  const clashes: string[] = [];
  const usedKeys = new Map<string, KeyUse[]>();
  // the letters the items are written with: what a translator can move a key to
  const letters = new Set<string>();

  for (const str of group.strings) {
    const trans = translate(str, langCode, translations);
    for (const ch of trans.toUpperCase()) {
      if (isLetterOrDigit(ch)) letters.add(ch);
    }
    const ix = trans.indexOf("&");
    if (ix === -1) {
      if (showDropped && str.includes("&")) {
        warnings.push(`no access key where the original has one: "${str}" -> "${trans}"`);
      }
      continue;
    }
    if (ix === trans.length - 1) {
      warnings.push(`'&' must be followed by a letter or digit: "${trans}"`);
      continue;
    }
    if (!str.includes("&")) {
      warnings.push(`access key where the original has none: "${str}" -> "${trans}"`);
    }

    // a whole code point: a key outside the BMP is two UTF-16 units
    const key = String.fromCodePoint(trans.codePointAt(ix + 1)!).toUpperCase();
    if (!isLetterOrDigit(key)) {
      // "Fit & Content": the '&' was meant literally, and no key works
      warnings.push(`'&' must be followed by a letter or digit, not '${key}': "${trans}"`);
      continue;
    }
    const uses = usedKeys.get(key) || [];
    const mine = group.alternatives.get(str);
    for (const use of uses) {
      if (!areExclusive(mine, group.alternatives.get(use.str))) {
        clashes.push(`${key}: "${trans}" and "${use.trans}"`);
      }
    }
    uses.push({ str, trans });
    usedKeys.set(key, uses);
  }

  const res: string[] = [];
  if (clashes.length > 0) {
    res.push(`  clashes in group '${name}':`);
    for (const c of clashes) res.push(`    * ${c}`);
    const available = [...letters].filter((k) => !usedKeys.has(k)).sort();
    res.push(`      (available keys: ${available.join("")})`);
  }
  for (const w of warnings) res.push(`  ${w}`);
  return res;
}

//--- -fix: move a translation's '&' to resolve clashes and broken keys

interface Fix {
  lang: string;
  str: string;
  from: string;
  to: string;
}

// the key a translation's '&' picks; "" when it has none or it isn't a letter or digit
function accessKeyOf(trans: string): string {
  const ix = trans.indexOf("&");
  if (ix === -1 || ix === trans.length - 1) return "";
  const key = String.fromCodePoint(trans.codePointAt(ix + 1)!).toUpperCase();
  return isLetterOrDigit(key) ? key : "";
}

// an English fallback isn't a translation: there is nothing to submit for it
function hasTranslation(str: string, lang: string, translations: Map<string, Translation[]>): boolean {
  return (translations.get(str) || []).some((t) => t.lang === lang && t.translation.length > 0);
}

// the translation without its '&'. A '&' standing alone as a word ("Fit & Content",
// "& Explorer", "Help &") takes one of its spaces with it
function withoutAmp(t: string): string {
  const ix = t.indexOf("&");
  if (ix === -1) return t;
  const spaceBefore = ix === 0 || t[ix - 1] === " ";
  const spaceAfter = ix === t.length - 1 || t[ix + 1] === " ";
  if (spaceBefore && spaceAfter) {
    // drop the space after it, or the one before it when it ends the text
    return ix < t.length - 1 ? t.slice(0, ix) + t.slice(ix + 2) : t.slice(0, Math.max(ix - 1, 0));
  }
  return t.slice(0, ix) + t.slice(ix + 1);
}

// "保存(&S)": the key is a Latin letter in parentheses, not one of the text's
const kParenKeyRe = /\(&([^\s()])\)/u;

// t with its '&' moved to the first key isFree accepts, word-initial letters
// first; for "(&S)" the letter in the parentheses changes, the English
// original's letters first. null when no key is free
function rekey(t: string, english: string, isFree: (key: string) => boolean): string | null {
  const cur = accessKeyOf(t);
  const paren = kParenKeyRe.exec(t);
  if (paren) {
    const pool = [...english.replaceAll("&", "").toUpperCase(), ..."ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"];
    for (const key of pool) {
      if (/^[A-Z0-9]$/.test(key) && key !== cur && isFree(key)) {
        return t.slice(0, paren.index) + `(&${key})` + t.slice(paren.index + paren[0].length);
      }
    }
    return null;
  }
  const base = withoutAmp(t);
  const wordStarts: number[] = [];
  const others: number[] = [];
  let prevInWord = false;
  for (let i = 0; i < base.length;) {
    const ch = String.fromCodePoint(base.codePointAt(i)!);
    if (isLetterOrDigit(ch)) {
      (prevInWord ? others : wordStarts).push(i);
    }
    // combining marks (Hindi, Bengali vowel signs) are part of the word
    prevInWord = /^[\p{L}\p{N}\p{M}]$/u.test(ch);
    i += ch.length;
  }
  for (const i of [...wordStarts, ...others]) {
    const key = String.fromCodePoint(base.codePointAt(i)!).toUpperCase();
    if (key !== cur && isFree(key)) {
      return base.slice(0, i) + "&" + base.slice(i);
    }
  }
  return null;
}

// A string's translation is shown in every group that has the string, so a
// key is free only if no item shown with it, in any of those groups, uses it.
// Of two clashing items the first one in the sources moves.
function fixLang(
  lang: string,
  groups: AccessGroup[],
  translations: Map<string, Translation[]>,
): { fixes: Fix[]; unresolved: string[] } {
  const overrides = new Map<string, string>();
  const tr = (str: string) => overrides.get(str) ?? translate(str, lang, translations);
  const groupsOf = new Map<string, AccessGroup[]>();
  for (const g of groups) {
    for (const str of g.strings) {
      const list = groupsOf.get(str) || [];
      list.push(g);
      groupsOf.set(str, list);
    }
  }
  const isFree = (str: string, key: string): boolean => {
    for (const g of groupsOf.get(str)!) {
      const mine = g.alternatives.get(str);
      for (const other of g.strings) {
        if (other === str || areExclusive(mine, g.alternatives.get(other))) continue;
        if (accessKeyOf(tr(other)) === key) return false;
      }
    }
    return true;
  };

  // gives every translated item shown with str that uses key another key;
  // false (and nothing changed) when one of them is an English fallback or has no free key
  const moveHolder = (str: string, key: string): boolean => {
    // each move applies at once, so the next one sees its new key; undone on failure
    const before = new Map<string, string | undefined>();
    const undo = () => {
      for (const [other, prev] of before) {
        if (prev === undefined) overrides.delete(other);
        else overrides.set(other, prev);
      }
      return false;
    };
    for (const g of groupsOf.get(str)!) {
      const mine = g.alternatives.get(str);
      for (const other of g.strings) {
        if (other === str || areExclusive(mine, g.alternatives.get(other))) continue;
        if (accessKeyOf(tr(other)) !== key) continue;
        if (!hasTranslation(other, lang, translations)) return undo();
        const to = rekey(tr(other), other, (k) => k !== key && isFree(other, k));
        if (!to) return undo();
        before.set(other, overrides.get(other));
        overrides.set(other, to);
      }
    }
    return before.size > 0;
  };

  const unresolved: string[] = [];
  for (const g of groups) {
    for (const str of g.strings) {
      const t = tr(str);
      if (!t.includes("&") || !hasTranslation(str, lang, translations)) continue;
      const key = accessKeyOf(t);
      if (key !== "" && isFree(str, key)) continue;
      let to = rekey(t, str, (k) => isFree(str, k));
      // no other key for this one: move the item it clashes with instead
      if (!to && key !== "" && moveHolder(str, key)) continue;
      // no free key: a broken '&' is worse than none
      if (!to && key === "") to = withoutAmp(t);
      if (!to) {
        unresolved.push(`"${str}" -> "${t}"`);
        continue;
      }
      overrides.set(str, to);
    }
  }
  const fixes: Fix[] = [];
  for (const [str, to] of overrides) {
    fixes.push({ lang, str, from: translate(str, lang, translations), to });
  }
  return { fixes, unresolved };
}

// the translations are apptranslator's: changes go there, as user "ai fix" (trans-dl.ts's fixes)
async function fixAll(
  groups: Map<string, AccessGroup>,
  translations: Map<string, Translation[]>,
  dryRun: boolean,
  server: string,
) {
  const secret = dryRun ? "" : tryGetTransSecret();
  if (!dryRun && !secret) {
    console.error("TRANS_UPLOAD_SECRET not set: can't submit; -dry shows the changes");
    process.exit(1);
  }
  const all: Fix[] = [];
  const groupList = [...groups.values()];
  for (const [lang] of gLangs) {
    // English is the sources': fix it there
    if (lang === "en") continue;
    const { fixes, unresolved } = fixLang(lang, groupList, translations);
    all.push(...fixes);
    for (const u of unresolved) console.log(`${lang}: no free key for ${u}`);
  }
  for (const f of all) {
    console.log(`${f.lang}: "${f.str}": "${f.from}" -> "${f.to}"`);
    if (!dryRun) await submitTranslation(server, secret, "ai fix", f.lang, f.str, f.to);
  }
  if (dryRun) {
    console.log(`${all.length} changes; -dry submitted nothing`);
  } else {
    console.log(`submitted ${all.length} changes to ${server}; bun cmd/trans-dl.ts downloads them`);
  }
}

async function main() {
  const args = process.argv.slice(2);
  const showDropped = args.includes("-all");
  const dryRun = args.includes("-dry");
  const fix = dryRun || args.includes("-fix");
  let server = process.env["APPTRANSLATOR_URL"] ?? DEFAULT_SERVER;
  if (args.includes("-local")) server = LOCAL_SERVER;
  const serverIdx = args.indexOf("-server");
  if (serverIdx !== -1 && args[serverIdx + 1]) server = args[serverIdx + 1]!;
  const translationsTxtPath = join(".work", "translations.txt");
  if (!existsSync(translationsTxtPath)) {
    console.error(`Missing ${translationsTxtPath}. Run: bun cmd/trans-dl.ts`);
    process.exit(1);
  }
  const translations = parseTranslations(readFileSync(translationsTxtPath, "utf-8"));

  const groups = new Map<string, AccessGroup>();
  for (const file of getFilesToProcess()) {
    extractAccesskeyGroups(file, groups);
  }
  if (fix) {
    await fixAll(groups, translations, dryRun, server);
    return;
  }

  let nLangsWithIssues = 0;
  let englishHasIssues = false;
  for (const [langCode, langName] of gLangs) {
    const issues: string[] = [];
    for (const [name, group] of groups) {
      issues.push(...checkGroup(name, group, langCode, translations, showDropped));
    }
    if (issues.length === 0) continue;
    nLangsWithIssues++;
    if (langCode === "en") englishHasIssues = true;
    console.log(`${langName} (${langCode}):`);
    for (const line of issues) console.log(line);
    console.log("");
  }
  const nStrings = [...groups.values()].reduce((n, g) => n + g.strings.length, 0);
  console.log(
    `checked ${nStrings} strings in ${groups.size} groups, ${nLangsWithIssues} of ${gLangs.length} languages have issues`,
  );
  if (!showDropped) {
    console.log("translations that dropped the original's access key are not listed; -all lists them");
  }
  // English is what the sources say; the rest is up to translators
  process.exit(englishHasIssues ? 1 : 0);
}

await main();
