// Stages the in-app manual under .work/docs: the markdown pages (from the
// sumatra-website repo) reachable from SumatraPDF-documentation.md, a
// manifest, the concatenated all-docs.md and the static files the on-demand
// renderer (docs/gen_docs.render.js) needs. The build's prebuild packs that dir into IDR_EMBEDDED_PAK.
import { readFileSync, writeFileSync, existsSync, readdirSync, mkdirSync, renameSync, rmSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { commands as commandsDef } from "./gen-commands";
import { checkCdnImages } from "./r2";
import { copyFileNormalized, websiteDocsDir } from "./util.js";

const docsDir = "docs";
const manualOutDir = join(".work", "docs");
const mdDir = websiteDocsDir;

const kMainPage = "SumatraPDF-documentation.md";
const kAllDocsFile = "all-docs.md";
// hosted on the website, not in this repo
const kExcludeFromAllDocs = new Set(["SumatraPDF-all-docs-for-llm-ai.md"]);

const kManualStaticFiles = [
  "sumatra.css",
  "manual.css",
  "theme.js",
  "gen_toc.js",
  "gen_code_copy.js",
  "gen_docs.fulltext_search.js",
  "gen_docs.render.js",
  "gen_docs.search.html",
  "gen_docs.search.js",
  "manual.shell.html",
  "favicon.ico",
];

function removeNotionId(s: string): string {
  if (s.length <= 32) return s;
  if (/^[0-9a-fA-F]{32}$/.test(s.slice(-32))) return s.slice(0, -32);
  return s;
}

// the .html name a page is addressed by in the app (manifest key, links)
function getHTMLFileName(mdName: string): string {
  const name = mdName.split("#")[0];
  const base = name.replace(/\.md$/, "");
  return removeNotionId(base).trim().replace(/ /g, "-") + ".html";
}

// .md files linked from `content`, in order, without fenced / inline code
function extractMdLinksInOrder(content: string): string[] {
  const prose = content.replace(/```[\s\S]*?```/g, "").replace(/`[^`\n]*`/g, "");
  const linkRe = /\[([^\]]+)\]\(([^)]+\.md)(?:#[^)]*)?\)/g;
  const seen = new Set<string>();
  const links: string[] = [];
  let m: RegExpExecArray | null;
  while ((m = linkRe.exec(prose)) !== null) {
    const fileName = m[2].replace(/%20/g, " ").replace(/^\.\//, "");
    if (
      fileName.startsWith("https://") ||
      fileName.startsWith("http://") ||
      kExcludeFromAllDocs.has(fileName) ||
      seen.has(fileName)
    ) {
      continue;
    }
    seen.add(fileName);
    links.push(fileName);
  }
  return links;
}

// the renderer drops this section of the main page, so its pages are not
// part of the manual
function stripMiscDocsSection(text: string): string {
  const startIdx = text.indexOf("## Misc docs");
  const endIdx = startIdx < 0 ? -1 : text.indexOf("## Downloads", startIdx);
  if (endIdx < 0) {
    return text;
  }
  return text.slice(0, startIdx) + text.slice(endIdx);
}

// every page reachable from the main page, main page first; a link to a
// missing page is an error
function collectPages(): string[] {
  const pages = [kMainPage];
  for (let i = 0; i < pages.length; i++) {
    const path = join(mdDir, pages[i]);
    if (!existsSync(path)) {
      throw new Error(`linked markdown file '${pages[i]}' not found`);
    }
    let text = readFileSync(path, "utf-8");
    if (pages[i] === kMainPage) {
      text = stripMiscDocsSection(text);
    }
    for (const link of extractMdLinksInOrder(text)) {
      if (!pages.includes(link)) {
        pages.push(link);
      }
    }
  }
  return pages;
}

function genAllDocsMd(outDir: string): void {
  const src = readFileSync(join(mdDir, kMainPage), "utf-8");
  const files = [kMainPage, ...extractMdLinksInOrder(src).filter((f) => f !== kMainPage)];
  const parts: string[] = [];
  for (const fileName of files) {
    const path = join(mdDir, fileName);
    if (!existsSync(path)) {
      throw new Error(`all-docs: '${fileName}' not found`);
    }
    const content = readFileSync(path, "utf-8");
    parts.push(`::${fileName}\n${content}`);
  }
  const outPath = join(outDir, kAllDocsFile);
  writeFileSync(outPath, parts.join("\n"));
  console.log(`wrote '${outPath}' (${files.length} files)`);
}

function writeBundledRenderJs(outDir: string): void {
  const template = readFileSync(join(docsDir, "gen_docs.render.js"), "utf-8");
  const searchHtml = readFileSync(join(docsDir, "gen_docs.search.html"), "utf-8");
  const searchJs = readFileSync(join(docsDir, "gen_docs.search.js"), "utf-8");
  const marker = "/*COMMANDS_SEARCH_BUNDLE*/";
  if (!template.includes(marker)) {
    throw new Error(`${join(docsDir, "gen_docs.render.js")} missing ${marker}`);
  }
  const bundle =
    "const kCommandsSearchHtml = " +
    JSON.stringify(searchHtml) +
    ";\nconst kCommandsSearchJs = " +
    JSON.stringify(searchJs) +
    ";\n";
  writeFileSync(join(outDir, "gen_docs.render.js"), template.replace(marker, bundle));
}

// Build the manual in a scratch dir and swap it in at the end: a build's
// prebuild (cmd/pack-embedded-prebuild.cmd) mirrors .work/docs and packs
// without the manual when the dir is missing, so it must never see a
// half-written one.
function writeManualPakFiles(pages: string[]): void {
  const outDir = `${manualOutDir}.tmp`;
  rmSync(outDir, { recursive: true, force: true });
  mkdirSync(outDir, { recursive: true });

  const manifest: Record<string, string> = {};
  for (const name of pages) {
    copyFileNormalized(join(outDir, name), join(mdDir, name));
    manifest[getHTMLFileName(name)] = name;
  }
  writeFileSync(join(outDir, "manifest.json"), JSON.stringify(manifest, null, 2));
  console.log(`wrote manifest.json (${Object.keys(manifest).length} pages)`);

  genAllDocsMd(outDir);

  for (const name of kManualStaticFiles) {
    if (name === "gen_docs.render.js") {
      continue;
    }
    copyFileNormalized(join(outDir, name), join(docsDir, name));
  }
  writeBundledRenderJs(outDir);
  copyFileNormalized(join(outDir, "markdown-it.min.js"), join("cmd", "markdown-it.min.js"));
  const bundledRender = readFileSync(join(outDir, "gen_docs.render.js"), "utf-8");
  if (!bundledRender.includes("cmd_ids") || !bundledRender.includes("driver();")) {
    throw new Error("bundled gen_docs.render.js missing Commands search UI");
  }

  // MakeLZSA stores file mtimes, so replacing identical files would still
  // change the archive and relink the exe on every build
  if (sameDirContents(outDir, manualOutDir)) {
    rmSync(outDir, { recursive: true, force: true });
    return;
  }
  rmSync(manualOutDir, { recursive: true, force: true });
  renameSync(outDir, manualOutDir);
}

function sameDirContents(a: string, b: string): boolean {
  if (!existsSync(a) || !existsSync(b)) {
    return false;
  }
  const list = (dir: string) => readdirSync(dir, { recursive: true, encoding: "utf8" }).sort();
  const filesA = list(a);
  if (filesA.join("\n") !== list(b).join("\n")) {
    return false;
  }
  for (const rel of filesA) {
    const pa = join(a, rel);
    if (statSync(pa).isDirectory()) {
      continue;
    }
    if (!readFileSync(pa).equals(readFileSync(join(b, rel)))) {
      return false;
    }
  }
  return true;
}

function extractCommandsFromMarkdown(): string[] {
  const lines = readFileSync(join(mdDir, "Commands.md"), "utf-8").split("\n");
  const cmds: string[] = [];
  for (const line of lines) {
    if (!line.startsWith("Cmd")) continue;
    const idx = line.indexOf(",");
    if (idx >= 0) cmds.push(line.slice(0, idx));
  }
  if (cmds.length < 20) throw new Error(`too few commands in Commands.md: ${cmds.length}`);
  return cmds;
}

function getCommandNames(): string[] {
  const names: string[] = [];
  for (let i = 0; i < commandsDef.length; i += 2) {
    names.push(commandsDef[i]);
  }
  return names;
}

function checkCommandsAreDocumented(): void {
  console.log("checkCommandsAreDocumented");
  const srcCmds = getCommandNames();
  console.log(`${srcCmds.length} commands in gen-commands.ts`);
  const docCmds = extractCommandsFromMarkdown();

  // special-case: remove old name which is still documented but not present in code
  const idx = docCmds.indexOf("CmdOpen");
  if (idx >= 0) docCmds.splice(idx, 1);

  console.log(`${docCmds.length} commands in Commands.md`);

  const docSet = new Set(docCmds);
  const onlyInSrc: string[] = [];
  for (const cmd of srcCmds) {
    if (docSet.has(cmd)) {
      docSet.delete(cmd);
    } else {
      onlyInSrc.push(cmd);
    }
  }
  if (onlyInSrc.length > 0) {
    console.log(`${onlyInSrc.length} in gen-commands.ts but not Commands.md:`);
    for (const c of onlyInSrc) console.log(`  ${c}`);
  }
  if (docSet.size > 0) {
    console.log(`${docSet.size} in Commands.md but not in gen-commands.ts:`);
    for (const c of docSet) console.log(`  ${c}`);
  }
}

export type GenDocsOptions = {
  // called from the build scripts: no network (r2 image check) and no
  // Commands.md audit, just a fresh .work/docs for the prebuild to pack
  forBuild?: boolean;
};

export async function main(opts: GenDocsOptions = {}) {
  const timeStart = performance.now();
  console.log("gen-docs starting");

  // CI has no (private) sumatra-website checkout; the exe ships without the manual
  if (!existsSync(mdDir)) {
    console.log(`gen-docs: skipping, '${mdDir}' missing (needs sumatra-website checkout)`);
    return;
  }

  // validates links by walking the doc graph from the main page
  const pages = collectPages();
  writeManualPakFiles(pages);
  if (!opts.forBuild) {
    await checkCdnImages([mdDir]);
  }

  // the build's prebuild (cmd/pack-embedded-prebuild.cmd) stages .work/docs
  // with translations + marked/mermaid under out/<cfg>/embedded[-static]
  // and packs IDR_EMBEDDED_PAK, so rebuild the exe to pick up the new manual
  console.log(
    `To preview on-demand rendering, open: file://${resolve(join(manualOutDir, "manual.shell.html"))} (needs a local server or in-app help)`,
  );

  if (!opts.forBuild) {
    checkCommandsAreDocumented();
  }

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`gen-docs finished in ${elapsed}s`);
}

// the exe embeds .work/docs, so every local build regenerates it first
export async function genDocsForBuild(): Promise<void> {
  await main({ forBuild: true });
}

if (import.meta.main) {
  await main();
}
