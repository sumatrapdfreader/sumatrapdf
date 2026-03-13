// markdown-it 14.1.0 - https://github.com/markdown-it/markdown-it (MIT license)
// vendored in cmd/markdown-it.min.js from https://cdn.jsdelivr.net/npm/markdown-it@14.1.0/dist/markdown-it.min.js
import MarkdownIt from "./markdown-it.min.js";
import {
  readFileSync,
  writeFileSync,
  existsSync,
  readdirSync,
  mkdirSync,
  rmSync,
  statSync,
  copyFileSync,
} from "node:fs";
import { join, resolve, extname } from "node:path";
import { commands as commandsDef } from "./gen-commands";

const docsDir = "docs";
const mdDir = join(docsDir, "md");
const wwwOutDir = join(docsDir, "www");

const mdProcessed = new Map<string, string>();
const mdToProcess: string[] = [];

const searchJS = `<script>${readFileSync(join("cmd", "html-helpers", "gen_docs.search.js"), "utf-8")}</script>`;
const searchHTML = readFileSync(join("cmd", "html-helpers", "gen_docs.search.html"), "utf-8");
const tmplManual = readFileSync(join(docsDir, "manual.tmpl.html"), "utf-8");

const h1BreadcrumbsStart = `
  <div class="breadcrumbs">
    <div><a href="SumatraPDF-documentation.html">SumatraPDF documentation</a></div>
    <div>/</div>
  <div>`;
const h1BreadcrumbsEnd = `</div>
</div>
`;

let breadCrumbs = "";

function slugify(text: string): string {
  return text
    .toLowerCase()
    .replace(/[^\w -]/g, "")
    .replace(/ /g, "-");
}

function removeNotionId(s: string): string {
  if (s.length <= 32) return s;
  if (/^[0-9a-fA-F]{32}$/.test(s.slice(-32))) return s.slice(0, -32);
  return s;
}

function getHTMLFileName(mdName: string): string {
  const name = mdName.split("#")[0];
  const base = name.replace(/\.md$/, "");
  return removeNotionId(base).trim().replace(/ /g, "-") + ".html";
}

function parseCsv(text: string): string[][] {
  const lines = text.trim().split("\n");
  return lines.map((line) => {
    const fields: string[] = [];
    let cur = "";
    let inQ = false;
    for (let i = 0; i < line.length; i++) {
      const ch = line[i];
      if (inQ) {
        if (ch === '"' && line[i + 1] === '"') {
          cur += '"';
          i++;
        } else if (ch === '"') {
          inQ = false;
        } else {
          cur += ch;
        }
      } else if (ch === '"') {
        inQ = true;
      } else if (ch === ",") {
        fields.push(cur);
        cur = "";
      } else {
        cur += ch;
      }
    }
    fields.push(cur);
    return fields;
  });
}

function genCsvTableHTML(records: string[][]): string {
  if (!records.length) return "";
  const out: string[] = ['<table class="collection-content">'];
  const hdr = records[0];
  out.push("<thead>", "<tr>");
  for (const c of hdr) out.push(`<th>${c}</th>`);
  out.push("</tr>", "</thead>", "<tbody>");
  for (let r = 1; r < records.length; r++) {
    out.push("<tr>");
    for (let i = 0; i < records[r].length; i++) {
      const cell = records[r][i].trim();
      if (!cell) {
        out.push("<td>", "</td>");
        continue;
      }
      out.push("<td>");
      out.push(i <= 1 ? `<code>${cell}</code>` : cell);
      out.push("</td>");
    }
    out.push("</tr>");
  }
  out.push("</tbody>", "</table>");
  return out.join("\n");
}

// Replace :columns markers with HTML div tags.
// markdown-it with html:true will pass the divs through and parse
// the markdown between them normally.
function preProcess(text: string): string {
  const lines = text.split("\n");
  let inCols = false;
  return lines
    .map((line) => {
      if (line.trim() === ":columns") {
        if (!inCols) {
          inCols = true;
          return '\n<div class="doc-columns">\n';
        } else {
          inCols = false;
          return "\n</div>\n";
        }
      }
      return line;
    })
    .join("\n");
}

function getInlineText(token: MarkdownIt.Token): string {
  if (!token.children) return token.content || "";
  return token.children.map((t: MarkdownIt.Token) => t.content || "").join("");
}

function mdToHTML(name: string): string {
  if (mdProcessed.has(name)) return mdProcessed.get(name)!;

  const isMainPage = name === "SumatraPDF-documentation.md";
  let text = readFileSync(join(mdDir, name), "utf-8");
  text = preProcess(text);

  const md = new MarkdownIt({ html: true, typographer: true });

  // use <div> for paragraphs (matching Go code's ParagraphTag: "div")
  md.renderer.rules.paragraph_open = () => "<div>";
  md.renderer.rules.paragraph_close = () => "</div>\n";

  // render ```commands fenced blocks as CSV tables
  md.renderer.rules.fence = (tokens: MarkdownIt.Token[], idx: number) => {
    const t = tokens[idx];
    if (t.info.trim() === "commands") return genCsvTableHTML(parseCsv(t.content));
    return `<pre><code>${md.utils.escapeHtml(t.content)}</code></pre>\n`;
  };

  // heading state
  let seenFirstH1 = false;
  let h1Mode: "skip" | "breadcrumb" | null = null;

  md.renderer.rules.heading_open = (tokens: MarkdownIt.Token[], idx: number) => {
    const tok = tokens[idx];
    const level = Number(tok.tag[1]);
    const text = getInlineText(tokens[idx + 1]);
    const id = slugify(text);

    if (level === 1 && !seenFirstH1) {
      seenFirstH1 = true;
      if (isMainPage) {
        // skip first H1 entirely on main page
        h1Mode = "skip";
        return "<!--skip-->";
      }
      // turn first H1 into breadcrumbs on other pages
      h1Mode = "breadcrumb";
      return h1BreadcrumbsStart;
    }
    return `<${tok.tag} id="${id}">`;
  };

  md.renderer.rules.heading_close = (tokens: MarkdownIt.Token[], idx: number) => {
    const tok = tokens[idx];
    if (h1Mode === "skip") {
      h1Mode = null;
      return "<!--/skip-->";
    }
    if (h1Mode === "breadcrumb") {
      h1Mode = null;
      return h1BreadcrumbsEnd;
    }
    const text = getInlineText(tokens[idx - 1]);
    const id = slugify(text);
    return `<a class="hlink" href="#${id}"> # </a></${tok.tag}>\n`;
  };

  // rewrite links: .md → .html, external links get target="_blank"
  md.renderer.rules.link_open = (tokens: MarkdownIt.Token[], idx: number, options: any, _env: any, self: any) => {
    const tok = tokens[idx];
    let href = tok.attrGet("href") ?? "";

    const isExternal =
      (href.startsWith("https://") || href.startsWith("http://")) && !href.includes("sumatrapdfreader.org");
    if (isExternal) {
      tok.attrSet("target", "_blank");
    }

    if (!href.startsWith("https://") && !href.startsWith("http://") && !href.startsWith("mailto:")) {
      const decoded = href.replace(/%20/g, " ");
      const hashIdx = decoded.indexOf("#");
      const fileName = hashIdx >= 0 ? decoded.slice(0, hashIdx) : decoded;
      const hash = hashIdx >= 0 ? decoded.slice(hashIdx + 1) : "";
      const ext = extname(fileName).toLowerCase();

      if (ext === ".md") {
        if (!existsSync(join(mdDir, fileName))) {
          throw new Error(`linked markdown file '${fileName}' not found`);
        }
        mdToProcess.push(fileName);
        let dest = getHTMLFileName(fileName);
        if (hash) dest += "#" + hash;
        tok.attrSet("href", dest);
      }
    }
    return self.renderToken(tokens, idx, options);
  };

  // validate image references exist
  md.renderer.rules.image = (tokens: MarkdownIt.Token[], idx: number, options: any, _env: any, self: any) => {
    const tok = tokens[idx];
    const src = tok.attrGet("src") ?? "";
    if (!src.startsWith("https://") && !src.startsWith("http://")) {
      const decoded = src.replace(/%20/g, " ");
      if (!existsSync(join(mdDir, decoded))) {
        throw new Error(`image '${decoded}' not found in ${mdDir}`);
      }
      tok.attrSet("src", decoded);
    }
    return self.renderToken(tokens, idx, options);
  };

  let innerHTML = md.render(text);

  // remove skipped first H1 (main page) including inline content between markers
  innerHTML = innerHTML.replace(/<!--skip-->[\s\S]*?<!--\/skip-->/g, "");

  innerHTML = `<div class="notion-page">${innerHTML}</div>`;

  let html = tmplManual.replace("{{InnerHTML}}", innerHTML);
  const title = getHTMLFileName(name).replace(".html", "").replace(/-/g, " ");
  html = html.replace("{{Title}}", title);

  if (name === "Commands.md") {
    html = html.replace("<div>:search:</div>", searchHTML);
    html = html.replace("</body>", searchJS + "</body>");
  }

  mdProcessed.set(name, html);
  return html;
}

function removeHTMLFiles(dir: string): void {
  if (!existsSync(dir)) return;
  for (const entry of readdirSync(dir)) {
    if (entry.endsWith(".html")) {
      rmSync(join(dir, entry));
    }
  }
}

function copyDirRecursive(dst: string, src: string): void {
  mkdirSync(dst, { recursive: true });
  for (const entry of readdirSync(src, { withFileTypes: true })) {
    const s = join(src, entry.name);
    const d = join(dst, entry.name);
    if (entry.isDirectory()) {
      copyDirRecursive(d, s);
    } else {
      copyFileSync(s, d);
    }
  }
}

function writeDocsHtmlFiles(): void {
  const imgOutDir = join(wwwOutDir, "img");
  rmSync(imgOutDir, { recursive: true, force: true });
  mkdirSync(imgOutDir, { recursive: true });
  removeHTMLFiles(wwwOutDir);

  for (const [name, html] of mdProcessed) {
    const htmlName = name.replace(".md", ".html");
    const path = join(wwwOutDir, htmlName);
    writeFileSync(path, html);
    //console.log(`wrote '${path}'`);
  }

  copyDirRecursive(join(wwwOutDir, "img"), join(mdDir, "img"));
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
  let idx = docCmds.indexOf("CmdOpen");
  docCmds.splice(idx, 1);

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

function formatSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}

export async function main() {
  const timeStart = performance.now();
  console.log("genHTMLDocsFromMarkdown starting");

  // process starting from the main page, following links
  mdToHTML("SumatraPDF-documentation.md");
  while (mdToProcess.length > 0) {
    const name = mdToProcess.shift()!;
    mdToHTML(name);
  }
  writeDocsHtmlFiles();

  // create lzsa archive
  const makeLzsa = resolve(join("bin", "MakeLZSA.exe"));
  if (!existsSync(makeLzsa)) {
    throw new Error(`'${makeLzsa}' doesn't exist`);
  }
  const archive = join("docs", "manual.dat");
  rmSync(archive, { force: true });
  const proc = Bun.spawn([makeLzsa, archive, wwwOutDir], {
    stdout: "inherit",
    stderr: "inherit",
  });
  const exitCode = await proc.exited;
  if (exitCode !== 0) {
    throw new Error(`MakeLZSA failed with exit code ${exitCode}`);
  }
  const size = statSync(archive).size;
  console.log(`size of '${archive}': ${formatSize(size)}`);

  const absDir = resolve(wwwOutDir);
  console.log(`To view, open: file://${join(absDir, "SumatraPDF-documentation.html")}`);

  checkCommandsAreDocumented();

  const elapsed = ((performance.now() - timeStart) / 1000).toFixed(1);
  console.log(`genHTMLDocsFromMarkdown finished in ${elapsed}s`);
}

if (import.meta.main) {
  await main();
}
