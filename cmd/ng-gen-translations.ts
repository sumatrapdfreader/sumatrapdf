// Keeps the translations.txt the build embeds complete: every string the
// sources pass to Tr() / TrN(), every command name and every home page tip has
// an entry, as in orig, where cmd/trans-dl.ts uploads that same list to
// apptranslator and downloads a file that holds all of it (a string nobody
// translated yet is an entry without translations). trans::GetTranslation()
// treats a string that has no entry as a bug (ReportDebugIf), so a stale file
// ends a -for-testing run in a non-English UI.
//
// This does not talk to the server. res/translations.txt is the last download;
// strings missing from it are appended, untranslated, to the staged copy in
// .work/ng/translations.txt, which is what cmd/ng-gen-embedded.ts packs.
//
//   bun cmd/ng-gen-translations.ts             stage the file, print the counts
//   bun cmd/ng-gen-translations.ts -missing    list the strings without an entry
//   bun cmd/ng-gen-translations.ts -diff <dir> strings here that <dir>/src lacks
//                                           (<dir> is a checkout of orig)

import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { commandAltDescs, commands } from "./ng-gen-commands";
import { sharedFiles } from "./helper/ng-shared";

const rootDir = join(import.meta.dir, "..");

/** the staged, complete file; the input of cmd/ng-gen-embedded.ts */
export const translationsOutPath = join(rootDir, ".work", "ng", "translations.txt");
const translationsSrcPath = join(rootDir, "src", "ng", "res", "translations.txt");

// orig's translationBlacklist: command names whose text is set dynamically
const translationBlacklist: string[] = ["Toggle Windows Previewer", "Toggle Windows Search Filter"];

// orig's translationPattern
const translationPattern = /\b(?:TrN|Tr)\("(.*?)"\)/g;

// ng: the image editor translates through its host: HostTr(StrL("...")). orig
// does not upload those, so they have no translations, but they reach
// GetTranslation() and need an entry
const hostTrPattern = /\bHostTr\((?:StrL\()?"(.*?)"\)/g;

// orig's tipsPattern
const tipsPattern = /static Str sumatraTips = StrL\(R"tips\(([\s\S]*?)\)tips"\);/;

// orig keeps every source in src/; the port also has src/gui/
function sourceFiles(srcDir: string): string[] {
  const res: string[] = [];
  const walk = (dir: string) => {
    for (const e of readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
      const p = join(dir, e.name);
      if (e.isDirectory()) {
        walk(p);
      } else if (e.name.toLowerCase().endsWith(".cpp") && !e.name.startsWith("EmbeddedData")) {
        res.push(p);
      }
    }
  };
  walk(srcDir);
  if (srcDir === join(rootDir, "src", "ng")) {
    res.push(...sharedFiles.filter((p) => p.endsWith(".cpp")).map((p) => join(rootDir, p)));
  }
  return res;
}

/** orig's extractStringsToTranslate(): Tr() / TrN() literals, tips, command names */
export function extractStrings(srcDir: string, commandNames: string[] | null): string[] {
  const strs: string[] = [];
  for (const path of sourceFiles(srcDir)) {
    const content = readFileSync(path, "utf-8");
    for (const m of content.matchAll(translationPattern)) {
      strs.push(m[1]!);
    }
    for (const m of content.matchAll(hostTrPattern)) {
      strs.push(m[1]!);
    }
  }
  const tipsPath = join(srcDir, "HomePage.cpp");
  const tips = existsSync(tipsPath) ? tipsPattern.exec(readFileSync(tipsPath, "utf-8")) : null;
  if (tips) {
    strs.push(
      ...tips[1]!
        .split("\n")
        .map((s) => s.trim())
        .filter((s) => s.length > 0),
    );
  }
  if (commandNames) {
    for (let i = 1; i < commandNames.length; i += 2) {
      strs.push(commandNames[i]!);
    }
  }
  const blacklist = new Set(translationBlacklist);
  return [...new Set(strs)].filter((s) => !blacklist.has(s));
}

function entriesOf(text: string): Set<string> {
  const res = new Set<string>();
  for (const line of text.split("\n")) {
    if (line.startsWith(":")) {
      res.add(line.slice(1));
    }
  }
  return res;
}

/** the strings of the sources that res/translations.txt has no entry for */
export function missingStrings(): string[] {
  const have = entriesOf(readFileSync(translationsSrcPath, "utf-8"));
  const strs = extractStrings(join(rootDir, "src", "ng"), commands);
  // orig does not upload these, but the command palette passes every command
  // name and alternate text to GetTranslation() all the same
  strs.push(...translationBlacklist, ...commandAltDescs.map(([, desc]) => desc));
  return [...new Set(strs)].filter((s) => !have.has(s)).sort();
}

/** Stages .work/ng/translations.txt; true when its content changed. */
export function genTranslations(): boolean {
  let text = readFileSync(translationsSrcPath, "utf-8");
  if (!text.endsWith("\n")) {
    text += "\n";
  }
  // an entry with no translation lines is what the server sends for a string
  // nobody translated yet: GetTranslation() answers with the English text
  for (const s of missingStrings()) {
    text += ":" + s + "\n";
  }
  if (existsSync(translationsOutPath) && readFileSync(translationsOutPath, "utf-8") === text) {
    return false;
  }
  mkdirSync(join(rootDir, ".work", "ng"), { recursive: true });
  writeFileSync(translationsOutPath, text, "utf-8");
  return true;
}

if (import.meta.main) {
  const args = process.argv.slice(2);
  if (args[0] === "-missing") {
    for (const s of missingStrings()) {
      console.log(s);
    }
  } else if (args[0] === "-diff" && args[1]) {
    // command names come from each tree's own Tr() calls and tables; orig's
    // commands are the port's (cmd/ng-gen-commands.ts is a copy)
    const theirs = new Set(extractStrings(join(args[1], "src"), commands));
    for (const s of extractStrings(join(rootDir, "src", "ng"), commands).sort()) {
      if (!theirs.has(s)) {
        console.log(s);
      }
    }
  } else {
    const changed = genTranslations();
    const n = missingStrings().length;
    console.log(`translations.txt: ${n} string(s) without an entry added${changed ? "" : " (up to date)"}`);
  }
}
