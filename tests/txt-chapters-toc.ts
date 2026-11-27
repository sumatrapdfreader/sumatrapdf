// A .txt book gets a table of contents from its chapter headings: lines like
// "Chapter 12", "CHAPTER IV", "Prologue", "第十二章 标题", "楔子", "Глава 3"
// on their own after a blank line. A text with fewer than two such lines, or
// with them inside paragraphs, gets none.
//
// Run:  bun tests/txt-chapters-toc.ts [--no-build]   (or via tests/run-almost-all.ts)

import { writeFileSync } from "node:fs";
import { EXE, runStandalone, tmpPath } from "./util";

const PARA = "It was a long day, and the road went on and on past the hills.\n".repeat(30);

const BOOK = [
  "The Long Road\n\nby Someone\n",
  "Prologue\n",
  PARA,
  "Chapter 1\n",
  PARA,
  "CHAPTER II. The River\n",
  PARA,
  "Chapter Three: Hills\n",
  PARA,
  "第十二章 回家\n",
  "他走了很久很久。\n".repeat(30),
  "楔子\n",
  "故事从这里开始。\n".repeat(30),
  "Глава 3\n",
  "Он шёл долго.\n".repeat(30),
  "Epilogue\n",
  PARA,
].join("\n");

const WANT = [
  "Prologue",
  "Chapter 1",
  "CHAPTER II. The River",
  "Chapter Three: Hills",
  "第十二章 回家",
  "楔子",
  "Глава 3",
  "Epilogue",
];

// "Chapter 1" inside a paragraph and a lone heading-like line: not a book
const NOT_A_BOOK = [
  "Notes\n",
  "See chapter 1 for the details.\nChapter 2 is next week.\n",
  "Some text.\n".repeat(20),
  "Chapter 4\n",
  "More text.\n".repeat(20),
].join("\n");

type TocEntry = { title: string; page: number; children?: TocEntry[] };

function toc(name: string, text: string): string[] {
  const path = tmpPath(`txt-chapters-toc-${name}.txt`);
  writeFileSync(path, text);
  const r = Bun.spawnSync([EXE, "cli", "toc", "--json", path]);
  const out = r.stdout.toString();
  if (r.exitCode !== 0) {
    throw new Error(`txt-chapters-toc ${name}: cli toc exit ${r.exitCode}\n${out}\n${r.stderr.toString()}`);
  }
  const flat: string[] = [];
  const walk = (entries: TocEntry[]) => {
    for (const e of entries) {
      flat.push(e.title);
      walk(e.children ?? []);
    }
  };
  walk((JSON.parse(out).toc ?? []) as TocEntry[]);
  return flat;
}

export async function testit(): Promise<void> {
  const book = toc("book", BOOK);
  if (JSON.stringify(book) !== JSON.stringify(WANT)) {
    throw new Error(`txt-chapters-toc: book toc\n got ${JSON.stringify(book)}\nwant ${JSON.stringify(WANT)}`);
  }
  const notBook = toc("notes", NOT_A_BOOK);
  if (notBook.length !== 0) {
    throw new Error(`txt-chapters-toc: notes got a toc: ${JSON.stringify(notBook)}`);
  }
  console.log("txt-chapters-toc: OK");
}

if (import.meta.main) {
  await runStandalone(testit);
}
