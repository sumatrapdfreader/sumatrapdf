/**
 * Check that translated menu item labels use Title Case.
 *
 * Menu labels are Tr("...") / TrN("...") strings in:
 *   - MenuDef arrays (`MenuDef menuDefFoo[] = { ... };`)
 *   - statements calling a menu API (AppendMenuW, MenuSetText, MenuAppend*, ...)
 *   - kMenuFuncs: functions that build labels in variables before they reach a menu
 *
 * Title Case (Windows / macOS guidelines): capitalize every word except
 * articles, coordinating conjunctions and short prepositions, unless first or
 * last. All-caps / mixed-case words (PDF, DjVu), format specifiers and text
 * after '\t' (shortcuts) are left alone.
 *
 * Usage:
 *   bun cmd/menu-title-case.ts        # report labels that need changing
 *   bun cmd/menu-title-case.ts -fix   # rewrite them in place
 *
 * A changed label is a new string to apptranslator; bun cmd/trans-dl.ts reuses
 * the old translations for it (matching English case variants).
 */
import { readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import path from "node:path";

const srcDir = path.join(import.meta.dir, "..", "src");

// lowercase in the middle of a label
const kMinorWords = new Set([
  "a",
  "an",
  "the",
  "and",
  "but",
  "or",
  "nor",
  "as",
  "at",
  "by",
  "for",
  "from",
  "in",
  "into",
  "of",
  "on",
  "onto",
  "over",
  "per",
  "to",
  "via",
  "vs",
  "with",
]);

const kMenuCallRe = /\b(AppendMenuW|InsertMenuW|ModifyMenuW|MenuSetText|MenuAppend\w*|MenuInsert\w*)\(/;
const kMenuDefStartRe = /\bMenuDef\s+\w+\[\]\s*=\s*\{/;
const kTrRe = /\bTrN?\("((?:[^"\\]|\\.)*)"\)/g;

// functions whose every Tr() string is a menu label: [file, function]
const kMenuFuncs: [string, string][] = [
  ["Favorites.cpp", "AppendFavMenus"],
  ["ng/Favorites.cpp", "AppendFavMenus"],
  ["shared/FavoritesCommon.cpp", "FavCompactReadableNameTemp"],
  ["shared/FavoritesCommon.cpp", "RebuildFavMenu"],
  ["Toolbar.cpp", "BuildSaveHoverMenu"],
  ["ng/Toolbar.cpp", "BuildSaveHoverMenu"],
  ["TableOfContents.cpp", "TocContextMenu"],
  ["ng/TableOfContents.cpp", "BuildTocContextMenu"],
  ["Menu.cpp", "OnWindowContextMenu"],
  ["ng/Menu.cpp", "BuildWindowContextMenu"],
  ["Menu.cpp", "MenuUpdatePrintItem"],
  ["shared/StripGroups.cpp", "GroupMenuTitleTemp"],
  ["shared/AnnotFilterToolbarCommon.cpp", "AnnotListMenuItems"],
];

// "static void Foo(int x) {" at column 0 -> "Foo"
const kFuncStartRe = /^[A-Za-z][^;=]*?\b(\w+)\([^;]*\{\s*$/;

// "fo&lder" -> index of 'f'; -1 when the word has no letter
function firstLetter(word: string): number {
  return word.search(/\p{L}/u);
}

function letters(word: string): string {
  return word.replace(/[^\p{L}]/gu, "");
}

function isMixedOrCaps(word: string): boolean {
  return /\p{Lu}/u.test(letters(word).slice(1));
}

function setFirst(word: string, upper: boolean): string {
  const i = firstLetter(word);
  if (i < 0) return word;
  const ch = word[i]!;
  return word.slice(0, i) + (upper ? ch.toUpperCase() : ch.toLowerCase()) + word.slice(i + 1);
}

function titleWord(word: string, isEdge: boolean): string {
  if (firstLetter(word) !== 0 && !/^[&(]/.test(word)) return word; // "%s", "50%"
  if (isMixedOrCaps(word)) return word;
  const minor = kMinorWords.has(letters(word).toLowerCase());
  return setFirst(word, isEdge || !minor);
}

// "Show in fo&lder" -> "Show in Fo&lder"; "&Shrink To Fit" -> "&Shrink to Fit"
export function toTitleCase(label: string): string {
  const tab = label.indexOf("\\t");
  const text = tab < 0 ? label : label.slice(0, tab);
  const rest = tab < 0 ? "" : label.slice(tab);

  const parts = text.split(/(\s+)/);
  const wordIdx = parts.map((p, i) => (/\S/.test(p) && firstLetter(p) >= 0 ? i : -1)).filter((i) => i >= 0);
  const first = wordIdx[0];
  const last = wordIdx[wordIdx.length - 1];
  for (const i of wordIdx) {
    // hyphenated parts are words too: "Auto-hide" -> "Auto-Hide", but "E-mail" stays
    const sub = parts[i]!.split("-");
    parts[i] = sub
      .map((w, j) => {
        if (j > 0 && letters(sub[j - 1]!).length === 1) return w;
        return titleWord(w, (i === first && j === 0) || (i === last && j === sub.length - 1));
      })
      .join("-");
  }
  return parts.join("") + rest;
}

function listCppFiles(dir: string, out: string[] = []): string[] {
  for (const name of readdirSync(dir)) {
    const p = path.join(dir, name);
    if (statSync(p).isDirectory()) {
      listCppFiles(p, out);
    } else if (name.endsWith(".cpp")) {
      out.push(p);
    }
  }
  return out;
}

interface Finding {
  file: string;
  line: number;
  label: string;
  fixed: string;
}

// line indexes (0-based) whose Tr() strings are menu labels
function menuLines(lines: string[], funcs: Set<string>): Set<number> {
  const res = new Set<number>();
  let inDef = false;
  let inFunc = false;
  for (let i = 0; i < lines.length; i++) {
    const l = lines[i]!;
    const fm = kFuncStartRe.exec(l);
    if (fm) inFunc = funcs.has(fm[1]!);
    if (/^\}/.test(l)) inFunc = false;
    if (inFunc) {
      res.add(i);
      continue;
    }
    if (kMenuDefStartRe.test(l)) inDef = true;
    if (inDef) {
      res.add(i);
      if (/^\s*\};/.test(l)) inDef = false;
      continue;
    }
    if (!kMenuCallRe.test(l)) continue;
    // the statement may continue on the next lines
    for (let j = i; j < lines.length; j++) {
      res.add(j);
      if (lines[j]!.trimEnd().endsWith(";")) break;
    }
  }
  return res;
}

function processFile(file: string, fix: boolean, findings: Finding[]): void {
  const text = readFileSync(file, "utf8");
  const lines = text.split("\n");
  const rel = path.relative(srcDir, file).replaceAll("\\", "/");
  const funcs = new Set(kMenuFuncs.filter(([f]) => f === rel).map(([, fn]) => fn));
  for (const fn of funcs) {
    if (!lines.some((l) => kFuncStartRe.exec(l)?.[1] === fn)) {
      throw new Error(`${rel}: function ${fn} from kMenuFuncs not found`);
    }
  }
  const menu = menuLines(lines, funcs);
  let changed = false;

  for (let i = 0; i < lines.length; i++) {
    if (!menu.has(i)) continue;
    lines[i] = lines[i]!.replace(kTrRe, (m, label: string) => {
      const fixed = toTitleCase(label);
      if (fixed === label) return m;
      findings.push({ file: rel, line: i + 1, label, fixed });
      changed = true;
      return m.replace(`"${label}"`, `"${fixed}"`);
    });
  }
  if (fix && changed) writeFileSync(file, lines.join("\n"));
}

function main(): void {
  const fix = process.argv.includes("-fix");
  const findings: Finding[] = [];
  for (const f of listCppFiles(srcDir)) processFile(f, fix, findings);

  for (const f of findings) {
    console.log(`${f.file}:${f.line}: "${f.label}" -> "${f.fixed}"`);
  }
  const verb = fix ? "fixed" : "need fixing (run with -fix)";
  console.log(`${findings.length} menu label(s) ${verb}`);
  if (!fix && findings.length > 0) process.exitCode = 1;
}

if (import.meta.main) main();
