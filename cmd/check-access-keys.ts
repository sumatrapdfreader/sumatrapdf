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
// Usage: bun cmd/check-access-keys.ts [-all]
//   -all: also list translations that dropped the original's access key
import { readFileSync, readdirSync, existsSync } from "node:fs";
import { join, extname } from "node:path";
import { gLangs } from "./trans-gen.ts";

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

function main() {
  const showDropped = process.argv.slice(2).includes("-all");
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

main();
