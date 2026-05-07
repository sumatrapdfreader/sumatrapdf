import { readFileSync, readdirSync } from "node:fs";
import { join, extname } from "node:path";

// [iso_code, display_name, ...]
const gLangs: string[][] = [
  ["af", "Afrikaans"], ["am", "Armenian (Հայերեն)"], ["ar", "Arabic (الْعَرَبيّة)"],
  ["az", "Azerbaijani (Azərbaycanca)"], ["bg", "Bulgarian (Български)"], ["bn", "Bengali (বাংলা)"],
  ["co", "Corsican (Corsu)"], ["br", "Portuguese - Brazil (Português)"],
  ["bs", "Bosnian (Bosanski)"], ["by", "Belarusian (Беларуская)"],
  ["ca", "Catalan (Català)"], ["ca-xv", "Catalan-Valencian (Català-Valencià)"],
  ["cn", "Chinese Simplified (简体中文)"], ["cy", "Welsh (Cymraeg)"],
  ["cz", "Czech (Čeština)"], ["de", "German (Deutsch)"], ["dk", "Danish (Dansk)"],
  ["el", "Greek (Ελληνικά)"], ["en", "English"], ["es", "Spanish (Español)"],
  ["et", "Estonian (Eesti)"], ["eu", "Basque (Euskara)"], ["fa", "Persian (فارسی)"],
  ["fi", "Finnish (Suomi)"], ["fr", "French (Français)"], ["fo", "Faroese (Føroyskt)"],
  ["fy-nl", "Frisian (Frysk)"], ["ga", "Irish (Gaeilge)"], ["gl", "Galician (Galego)"],
  ["he", "Hebrew (עברית)"], ["hi", "Hindi (हिंदी)"], ["hr", "Croatian (Hrvatski)"],
  ["hu", "Hungarian (Magyar)"], ["id", "Indonesian (Bahasa Indonesia)"],
  ["it", "Italian (Italiano)"], ["ja", "Japanese (日本語)"], ["jv", "Javanese (ꦧꦱꦗꦮ)"],
  ["ka", "Georgian (ქართული)"], ["kr", "Korean (한국어)"], ["ku", "Kurdish (كوردی)"],
  ["kw", "Cornish (Kernewek)"], ["lt", "Lithuanian (Lietuvių)"],
  ["lv", "Latvian (latviešu valoda)"], ["mk", "Macedonian (македонски)"],
  ["ml", "Malayalam (മലയാളം)"], ["mm", "Burmese (ဗမာ စာ)"],
  ["my", "Malaysian (Bahasa Melayu)"], ["ne", "Nepali (नेपाली)"],
  ["nl", "Dutch (Nederlands)"], ["nn", "Norwegian Neo-Norwegian (Norsk nynorsk)"],
  ["no", "Norwegian (Norsk)"], ["pa", "Punjabi (ਪੰਜਾਬੀ)"], ["pl", "Polish (Polski)"],
  ["pt", "Portuguese - Portugal (Português)"], ["ro", "Romanian (Română)"],
  ["ru", "Russian (Русский)"], ["sat", "Santali (ᱥᱟᱱᱛᱟᱲᱤ)"],
  ["si", "Sinhala (සිංහල)"], ["sk", "Slovak (Slovenčina)"],
  ["sl", "Slovenian (Slovenščina)"], ["sn", "Shona (Shona)"],
  ["sp-rs", "Serbian (Latin)"], ["sq", "Albanian (Shqip)"],
  ["sr-rs", "Serbian (Cyrillic)"], ["sv", "Swedish (Svenska)"],
  ["ta", "Tamil (தமிழ்)"], ["th", "Thai (ภาษาไทย)"], ["tl", "Tagalog (Tagalog)"],
  ["tr", "Turkish (Türkçe)"], ["tw", "Chinese Traditional (繁體中文)"],
  ["uk", "Ukrainian (Українська)"], ["uz", "Uzbek (O'zbek)"],
  ["vn", "Vietnamese (Việt Nam)"],
];

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

const translationPattern = /\b_TR[AN]?\("(.*?)"\)/g;

function extractTranslationStrings(s: string): string[] {
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

interface AccessGroup {
  group: string[];
  inAltGroup: boolean;
  altGroup: string[];
}

function isGroupStartOrEnd(s: string): boolean {
  return s.startsWith("//[ ACCESSKEY_GROUP ") || s.startsWith("//] ACCESSKEY_GROUP ");
}

function isAltGroupStartOrEnd(s: string): boolean {
  return (
    s.startsWith("//[ ACCESSKEY_ALTERNATIVE") ||
    s.startsWith("//| ACCESSKEY_ALTERNATIVE") ||
    s.startsWith("//] ACCESSKEY_ALTERNATIVE")
  );
}

function extractAccesskeyGroups(path: string): Map<string, AccessGroup> {
  const content = readFileSync(path, "utf-8");
  const lines = content.split(/\r?\n/);
  const groups = new Map<string, AccessGroup>();
  let groupName = "";
  let group: AccessGroup | null = null;

  for (let line of lines) {
    line = line.trim();
    if (isGroupStartOrEnd(line)) {
      const newName = line.substring(20);
      if (line[2] === "[") {
        if (group) throw new Error(`Group '${groupName}' doesn't end before group '${newName}' starts`);
        groupName = newName;
        group = groups.get(groupName) || { group: [], inAltGroup: false, altGroup: [] };
        groups.set(groupName, group);
      } else {
        if (!group) throw new Error(`Unexpected group end ('${newName}')`);
        if (groupName !== newName) throw new Error(`Group end mismatch: '${newName}' != '${groupName}'`);
        group = null;
      }
    } else if (isAltGroupStartOrEnd(line)) {
      if (!group) throw new Error("Can't use ACCESSKEY_ALTERNATIVE outside of group");
      if (line[2] === "[") {
        if (group.inAltGroup) throw new Error("Nested ACCESSKEY_ALTERNATIVE isn't supported");
        group.inAltGroup = true;
      } else if (line[2] === "|") {
        if (!group.inAltGroup) throw new Error("Unexpected ACCESSKEY_ALTERNATIVE alternative");
      } else {
        if (!group.inAltGroup) throw new Error("Unexpected ACCESSKEY_ALTERNATIVE end");
        group.inAltGroup = false;
      }
    } else if (group) {
      const strs = extractTranslationStrings(line);
      for (const str of strs) {
        const exists = group.group.includes(str);
        const n = (str.match(/&/g) || []).length;
        if (n > 1) throw new Error("TODO: handle multiple '&' in strings");
        if (exists) {
          group.group.push(str);
        }
        if (group.inAltGroup) {
          group.altGroup.push(str);
        }
      }
    }
  }
  return groups;
}

function isAlnum(s: string): boolean {
  const c = s.charCodeAt(0);
  return (c >= 65 && c <= 90) || (c >= 97 && c <= 122) || (c >= 48 && c <= 57);
}

function detectAccesskeyClashes(
  allGroups: Map<string, AccessGroup>,
  translations: Map<string, Translation[]>,
): void {
  for (const lang of gLangs) {
    const langCode = lang[0];
    const langName = lang[1];
    console.log(`Accesskey issues for '${langName}'${"=".repeat(23 + langName.length)}'`);
    const warnings: string[] = [];
    for (const [, group] of allGroups) {
      const usedKeys: Map<string, string> = new Map();
      const strs = group.group;
      for (const str of strs.slice(1)) {
        let trans = str;
        const transItems = translations.get(str);
        if (transItems) {
          for (const item of transItems) {
            if (item.lang === langCode) {
              trans = item.translation;
              break;
            }
          }
        }
        const ix = trans.indexOf("&");
        if (ix === -1) {
          if (str.includes("&")) {
            warnings.push("WARNING: Translation has no accesskey where original does:");
            warnings.push(`         "${strs}", "${trans}"`);
            continue;
          }
        }
        if (ix === trans.length - 1) {
          warnings.push(`ERROR: '&' must be followed by a letter ("${trans}")`);
          continue;
        }
        if (!str.includes("&")) {
          warnings.push("WARNING: Translation has accesskey where original doesn't:");
          warnings.push(`         "${str}", "${trans}"`);
        }
        if (ix >= 0) {
          const key = trans[ix + 1].toUpperCase();
          if (usedKeys.has(key)) {
            // clash detected (placeholder for full duplicate reporting)
          } else {
            if (!isAlnum(key)) {
              warnings.push(`WARNING: Access key '${key}' might not work on all keyboards ("${trans}")`);
            }
            usedKeys.set(key, trans);
          }
        }
      }
    }
    console.log("");
  }
}

function printGroups(file: string, groups: Map<string, AccessGroup>): void {
  if (groups.size === 0) return;
  console.log(file);
  for (const [name, g] of groups) {
    console.log(`  ${name}`);
    for (const s of g.group) {
      console.log(`    ${s}`);
    }
    if (g.altGroup.length > 0) {
      console.log("    alt groups:");
      for (const s of g.altGroup) {
        console.log(`    ${s}`);
      }
    }
  }
}

function updateGroups(m1: Map<string, AccessGroup>, m2: Map<string, AccessGroup>): Map<string, AccessGroup> {
  for (const [k, g2] of m2) {
    const g1 = m1.get(k);
    if (!g1) {
      m1.set(k, g2);
    } else {
      g1.group.push(...g2.group);
      g1.altGroup.push(...g2.altGroup);
    }
  }
  return m1;
}

function main() {
  const cFiles = getFilesToProcess();
  const allGroups = new Map<string, AccessGroup>();
  for (const file of cFiles) {
    const groups = extractAccesskeyGroups(file);
    printGroups(file, groups);
    updateGroups(allGroups, groups);
  }
  const translationsTxtPath = join("translations", "translations.txt");
  const d = readFileSync(translationsTxtPath, "utf-8");
  const translations = parseTranslations(d);
  detectAccesskeyClashes(allGroups, translations);
}

main();
