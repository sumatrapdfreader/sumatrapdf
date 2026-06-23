// In-app manual renderer (issue #5712). Uses markdown-it 14.1.0, matching cmd/gen-docs.ts.
(function (global) {
  "use strict";

  /*COMMANDS_SEARCH_BUNDLE*/

  const h1BreadcrumbsStart =
    '<div class="breadcrumbs"><div><a href="SumatraPDF-documentation.html">SumatraPDF documentation</a></div><div>/</div><div>';
  const h1BreadcrumbsEnd = "</div></div>";

  let manifest = null;
  let mainDocText = null;

  function removeNotionId(s) {
    if (s.length <= 32) return s;
    if (/^[0-9a-fA-F]{32}$/.test(s.slice(-32))) return s.slice(0, -32);
    return s;
  }

  function getHTMLFileName(mdName) {
    const name = mdName.split("#")[0];
    const base = name.replace(/\.md$/i, "");
    return removeNotionId(base).trim().replace(/ /g, "-") + ".html";
  }

  function htmlFileFromLocation() {
    const path = global.location.pathname || "";
    const base = path.split("/").pop() || "SumatraPDF-documentation.html";
    return base.split("#")[0].split("?")[0];
  }

  function slugify(text) {
    return text
      .toLowerCase()
      .replace(/[^\w -]/g, "")
      .replace(/ /g, "-");
  }

  function stripMiscDocsSection(text) {
    const startMarker = "## Misc docs";
    const endMarker = "## Downloads";
    const startIdx = text.indexOf(startMarker);
    if (startIdx < 0) return text;
    const endIdx = text.indexOf(endMarker, startIdx);
    if (endIdx < 0) return text;
    return text.slice(0, startIdx) + text.slice(endIdx);
  }

  function preProcess(text) {
    const lines = text.split("\n");
    let inCols = false;
    return lines
      .map(function (line) {
        if (line.trim() === ":columns") {
          if (!inCols) {
            inCols = true;
            return '\n<div class="doc-columns">\n';
          }
          inCols = false;
          return "\n</div>\n";
        }
        return line;
      })
      .join("\n");
  }

  function parseCsv(text) {
    const lines = text.trim().split("\n");
    return lines.map(function (line) {
      const fields = [];
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

  function genCsvTableHTML(records) {
    if (!records.length) return "";
    const out = ['<table class="collection-content">'];
    const hdr = records[0];
    out.push("<thead>", "<tr>");
    for (let i = 0; i < hdr.length; i++) out.push("<th>" + hdr[i] + "</th>");
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
        out.push(i <= 1 ? "<code>" + cell + "</code>" : cell);
        out.push("</td>");
      }
      out.push("</tr>");
    }
    out.push("</tbody>", "</table>");
    return out.join("\n");
  }

  function isMultiLineCode(content) {
    return content.replace(/\r\n/g, "\n").trimEnd().includes("\n");
  }

  function genPlainCodeBlockHTML(codeInnerHtml, codeClass) {
    const cls = codeClass ? ' class="' + codeClass + '"' : "";
    return "<pre><code" + cls + ">" + codeInnerHtml + "</code></pre>\n";
  }

  function genCodeBlockHTML(codeInnerHtml, codeClass) {
    const cls = codeClass ? ' class="' + codeClass + '"' : "";
    return (
      '<div class="code-block">' +
      '<button type="button" class="sum-code-copy-btn" title="Copy to clipboard">Copy</button>' +
      "<pre><code" +
      cls +
      ">" +
      codeInnerHtml +
      "</code></pre>" +
      "</div>\n"
    );
  }

  function renderFenceCodeBlock(content, codeInnerHtml, codeClass) {
    if (!isMultiLineCode(content)) {
      return genPlainCodeBlockHTML(codeInnerHtml, codeClass);
    }
    return genCodeBlockHTML(codeInnerHtml, codeClass);
  }

  function getInlineText(token) {
    if (!token.children) return token.content || "";
    return token.children
      .map(function (t) {
        return t.content || "";
      })
      .join("");
  }

  function buildTocHTML(currentHtml) {
    if (!mainDocText) return "";
    const linkRe = /\[([^\]]+)\]\(([^)]+\.md)\)/g;
    const items = [];
    const lines = mainDocText.split("\n");
    let inColumns = false;
    for (let li = 0; li < lines.length; li++) {
      const line = lines[li];
      if (line.trim() === ":columns") {
        inColumns = !inColumns;
        continue;
      }
      if (!inColumns) continue;
      let match;
      while ((match = linkRe.exec(line)) !== null) {
        const title = match[1];
        const href = getHTMLFileName(match[2]);
        const cls = href === currentHtml ? ' class="toc-current"' : "";
        items.push("<a" + cls + ' href="' + href + '">' + title + "</a>");
      }
    }
    const searchHint =
      '<div onclick="window.openSearchDialog()" class="search-trigger-2"><kbd>Ctrl + K</kbd> to search...</div>\n';
    return (
      '<nav class="sidebar-toc">\n' +
      searchHint +
      '<div class="toc-title"></div>\n' +
      items.join("\n") +
      "\n</nav>"
    );
  }

  function createMarkdownRenderer(md) {
    md.renderer.rules.paragraph_open = function () {
      return "<div>";
    };
    md.renderer.rules.paragraph_close = function () {
      return "</div>\n";
    };

    md.renderer.rules.fence = function (tokens, idx) {
      const t = tokens[idx];
      const lang = t.info.trim().split(/\s+/)[0];
      if (lang === "commands") return genCsvTableHTML(parseCsv(t.content));
      return renderFenceCodeBlock(t.content, md.utils.escapeHtml(t.content));
    };

    md.renderer.rules.heading_open = function (tokens, idx) {
      const tok = tokens[idx];
      const text = getInlineText(tokens[idx + 1]);
      const id = slugify(text);
      return "<" + tok.tag + ' id="' + id + '">';
    };

    md.renderer.rules.heading_close = function (tokens, idx) {
      const tok = tokens[idx];
      const text = getInlineText(tokens[idx - 1]);
      const id = slugify(text);
      return '<a class="hlink" href="#' + id + '"> # </a></' + tok.tag + ">\n";
    };

    md.renderer.rules.link_open = function (tokens, idx, options, env, self) {
      const tok = tokens[idx];
      let href = tok.attrGet("href") || "";

      const isAbsolute =
        href.startsWith("https://") ||
        href.startsWith("http://") ||
        href.startsWith("mailto:");

      if (!isAbsolute) {
        const decoded = href.replace(/%20/g, " ");
        const hashIdx = decoded.indexOf("#");
        const fileName = hashIdx >= 0 ? decoded.slice(0, hashIdx) : decoded;
        const hash = hashIdx >= 0 ? decoded.slice(hashIdx + 1) : "";
        const ext = fileName.slice(fileName.lastIndexOf(".")).toLowerCase();
        if (ext === ".md") {
          if (fileName === "SumatraPDF-all-docs-for-llm-ai.md") {
            tok.attrSet(
              "href",
              "https://www.sumatrapdfreader.org/docs/SumatraPDF-all-docs-for-llm-ai.md",
            );
          } else {
            let dest = getHTMLFileName(fileName);
            if (hash) dest += "#" + hash;
            tok.attrSet("href", dest);
          }
        }
      }

      // Open non-internal links (any absolute http/https/mailto URL, including
      // ones we just rewrote to a sumatrapdfreader.org URL) in a new tab. The
      // in-app webview turns these into new-window requests and hands them to
      // the default OS browser instead of navigating the manual.
      const finalHref = tok.attrGet("href") || "";
      const isNonInternal =
        finalHref.startsWith("https://") ||
        finalHref.startsWith("http://") ||
        finalHref.startsWith("mailto:");
      if (isNonInternal) {
        tok.attrSet("target", "_blank");
        tok.attrSet("rel", "noopener noreferrer");
      }
      return self.renderToken(tokens, idx, options);
    };
  }

  function renderMarkdown(mdName, text) {
    const isMainPage = mdName === "SumatraPDF-documentation.md";
    if (isMainPage) {
      text = stripMiscDocsSection(text);
    }

    let h1Text = "";
    const h1Match = text.match(/^# (.+)$/m);
    if (h1Match) {
      h1Text = h1Match[1];
      text = text.replace(/^# .+\n?/, "");
    }

    text = preProcess(text);

    const md = global.markdownit({ html: true, typographer: true });
    createMarkdownRenderer(md);
    let innerHTML = md.render(text);

    if (h1Text && !isMainPage) {
      const bc = h1BreadcrumbsStart + h1Text + h1BreadcrumbsEnd;
      innerHTML = bc + innerHTML + '<div>&nbsp;</div>' + bc;
    }

    innerHTML = '<div class="notion-page">' + innerHTML + "</div>";
    if (mdName === "Commands.md") {
      innerHTML = replaceCommandsSearchPlaceholder(innerHTML);
    }
    return { innerHTML: innerHTML, h1Text: h1Text, isMainPage: isMainPage };
  }

  function getCommandsSearchHtml() {
    if (typeof kCommandsSearchHtml === "string" && kCommandsSearchHtml) {
      return kCommandsSearchHtml;
    }
    return null;
  }

  function replaceCommandsSearchPlaceholder(html) {
    const searchHtml = getCommandsSearchHtml();
    if (!searchHtml) {
      return html;
    }
    if (html.indexOf("<div>:search:</div>") >= 0) {
      return html.replace("<div>:search:</div>", searchHtml);
    }
    return html;
  }

  function fetchText(url) {
    return fetch(url).then(function (r) {
      if (!r.ok) throw new Error("failed to load " + url);
      return r.text();
    });
  }

  function ensureManifest() {
    if (manifest) return Promise.resolve(manifest);
    return fetchText("manifest.json").then(function (text) {
      manifest = JSON.parse(text);
      return manifest;
    });
  }

  function ensureMainDocText() {
    if (mainDocText) return Promise.resolve(mainDocText);
    return fetchText("SumatraPDF-documentation.md").then(function (text) {
      mainDocText = stripMiscDocsSection(text);
      return mainDocText;
    });
  }

  function runCommandsSearchScript(searchJs) {
    const script = document.createElement("script");
    script.textContent = searchJs;
    document.body.appendChild(script);
  }

  function injectCommandsSearch(innerSlot) {
    function runJs() {
      const js =
        typeof kCommandsSearchJs === "string" && kCommandsSearchJs
          ? kCommandsSearchJs
          : null;
      if (js) {
        runCommandsSearchScript(js);
        return Promise.resolve();
      }
      return fetchText("gen_docs.search.js").then(runCommandsSearchScript);
    }

    if (getCommandsSearchHtml() || !innerSlot) {
      return runJs();
    }
    return fetchText("gen_docs.search.html")
      .then(function (searchHtml) {
        innerSlot.innerHTML = innerSlot.innerHTML.replace(
          "<div>:search:</div>",
          searchHtml,
        );
      })
      .then(runJs);
  }

  function renderPage(mdName, currentHtml) {
    return Promise.all([ensureManifest(), ensureMainDocText()])
      .then(function () {
        return fetchText(mdName);
      })
      .then(function (text) {
        const rendered = renderMarkdown(mdName, text);
        const tocSlot = document.getElementById("toc-slot");
        const innerSlot = document.getElementById("inner-slot");
        const titleEl = document.getElementById("doc-title");
        if (tocSlot) {
          tocSlot.innerHTML = buildTocHTML(currentHtml);
        }
        if (innerSlot) {
          innerSlot.innerHTML = rendered.innerHTML;
        }
        if (typeof window.rebuildPageToc === "function") {
          window.rebuildPageToc();
        }
        if (titleEl) {
          const title = currentHtml.replace(".html", "").replace(/-/g, " ");
          titleEl.textContent = title;
        }
        if (mdName === "Commands.md") {
          return injectCommandsSearch(innerSlot);
        }
      });
  }

  function bootstrap() {
    if (typeof global.markdownit !== "function") {
      const innerSlot = document.getElementById("inner-slot");
      if (innerSlot) {
        innerSlot.textContent = "Documentation renderer failed to load.";
      }
      return;
    }

    const currentHtml = htmlFileFromLocation();
    ensureManifest()
      .then(function (m) {
        const mdName = m[currentHtml];
        if (!mdName) {
          throw new Error("unknown page " + currentHtml);
        }
        return renderPage(mdName, currentHtml);
      })
      .catch(function (err) {
        const innerSlot = document.getElementById("inner-slot");
        if (innerSlot) {
          innerSlot.textContent = String(err);
        }
      });
  }

  global.ManualDocs = {
    bootstrap: bootstrap,
    renderPage: renderPage,
    getHTMLFileName: getHTMLFileName,
    renderMarkdown: renderMarkdown,
  };
})(typeof window !== "undefined" ? window : globalThis);