// In-app manual renderer (issue #5712). Uses markdown-it 14.1.0, matching cmd/gen-docs.ts.
(function (global) {
  "use strict";

  /*COMMANDS_SEARCH_BUNDLE*/

  // hides / shows the sidebar TOC, at the start of the breadcrumbs above the page
  // (see setupSidebarToggle)
  const tocToggleHTML =
    '<button type="button" class="toc-toggle" aria-label="Hide sidebar" title="Hide sidebar">' +
    '<svg viewBox="0 0 16 16" width="16" height="16" fill="none" stroke="currentColor" stroke-width="1.3" aria-hidden="true">' +
    '<rect x="1.5" y="2.5" width="13" height="11" rx="1.5"/><path d="M6 2.5v11"/></svg></button>';
  const h1BreadcrumbsStart =
    '<div class="breadcrumbs"><div><a href="SumatraPDF-documentation.html">SumatraPDF documentation</a></div><div>/</div><div>';
  const h1BreadcrumbsStartWithToggle =
    '<div class="breadcrumbs">' +
    tocToggleHTML +
    '<div><a href="SumatraPDF-documentation.html">SumatraPDF documentation</a></div><div>/</div><div>';
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

  // ":video <youtube link> <r2 link>": a video recorded for the docs, shown as an embedded
  // YouTube player; the r2 link is the same video on files.sumatrapdfreader.org
  // (see youTubeEmbedHTML() in the website's server/gen_manual.go)
  const rxVideoLine =
    /^:video[ \t]+https:\/\/(?:youtu\.be\/|(?:www\.)?youtube\.com\/watch\?v=)([A-Za-z0-9_-]{11})\S*[ \t]+https:\/\/files\.sumatrapdfreader\.org\/\S+[ \t]*$/;

  function videoHTML(youTubeId) {
    return (
      '\n<div class="doc-video"><iframe src="https://www.youtube-nocookie.com/embed/' +
      youTubeId +
      '" title="Video" loading="lazy" allow="accelerometer; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe></div>\n'
    );
  }

  function preProcess(text) {
    const lines = text.split("\n");
    let inCols = false;
    return lines
      .map(function (line) {
        const video = rxVideoLine.exec(line.trim());
        if (video) {
          return videoHTML(video[1]);
        }
        if (line.trim() === ":columns") {
          if (!inCols) {
            inCols = true;
            return '\n<div class="doc-columns">\n';
          }
          inCols = false;
          return "\n</div>\n";
        }
        if (line.trim() === ":askai") {
          return '\n<div class="askai"></div>\n';
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
    const commandColumnCount = 3;
    const out = ['<table class="collection-content">'];
    const hdr = records[0];
    out.push("<thead>", "<tr>");
    for (let i = 0; i < commandColumnCount; i++) out.push("<th>" + (hdr[i] || "") + "</th>");
    out.push("</tr>", "</thead>", "<tbody>");
    for (let r = 1; r < records.length; r++) {
      const row = records[r];
      const notes = (row[commandColumnCount] || "").trim();
      out.push('<tr class="command-row' + (notes ? " command-has-notes" : "") + '">');
      for (let i = 0; i < commandColumnCount; i++) {
        const cell = (row[i] || "").trim();
        if (!cell) {
          out.push("<td>", "</td>");
          continue;
        }
        out.push("<td>");
        out.push(i <= 1 ? "<code>" + cell + "</code>" : cell);
        out.push("</td>");
      }
      out.push("</tr>");
      if (notes) {
        out.push('<tr class="command-notes">', '<td colspan="' + commandColumnCount + '">' + notes + "</td>", "</tr>");
      }
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
    // "## Section" heading of the index, shown above its links
    let section = "";
    for (let li = 0; li < lines.length; li++) {
      const line = lines[li];
      if (line.startsWith("## ")) {
        section = line.slice(3).trim();
        continue;
      }
      if (line.trim() === ":columns") {
        inColumns = !inColumns;
        if (inColumns && section) {
          items.push('<div class="toc-section">' + section + "</div>");
          section = "";
        }
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
    return '<nav class="sidebar-toc">\n' + items.join("\n") + "\n</nav>";
  }

  // Keep the sidebar's scroll position when a sidebar link loads another page.
  const kSidebarScrollKey = "docs-sidebar-scroll";

  function keepSidebarScroll(toc) {
    toc.addEventListener("click", function (e) {
      if (!e.target.closest("a")) {
        return;
      }
      try {
        localStorage.setItem(kSidebarScrollKey, JSON.stringify({ top: toc.scrollTop, time: Date.now() }));
      } catch (err) {}
    });
    try {
      const saved = JSON.parse(localStorage.getItem(kSidebarScrollKey) || "null");
      localStorage.removeItem(kSidebarScrollKey);
      if (saved && Date.now() - saved.time < 60 * 1000) {
        toc.scrollTop = saved.top;
      }
    } catch (err) {}
  }

  // Hide / show the sidebar: the button at the start of the breadcrumbs or
  // Ctrl + B (Cmd + B on Mac). While hidden, the mouse at the left edge of the
  // window shows it over the page until the mouse leaves it. manual.shell.html
  // applies the saved state before the first paint.
  const kSidebarCollapsedKey = "docs-sidebar-collapsed";
  let sidebarToggleReady = false;

  function setupSidebarToggle(toc) {
    if (sidebarToggleReady) {
      return;
    }
    sidebarToggleReady = true;
    const root = document.documentElement;
    const btn = document.querySelector(".toc-toggle");
    const isMac = /Mac|iPhone|iPad/.test(navigator.platform || navigator.userAgent);
    const shortcut = isMac ? "Cmd + B" : "Ctrl + B";
    const kEdgePx = 8;
    function isCollapsed() {
      return root.classList.contains("toc-collapsed");
    }
    function update() {
      if (!btn) {
        return;
      }
      const label = isCollapsed() ? "Show sidebar" : "Hide sidebar";
      btn.title = label + " (" + shortcut + ")";
      btn.setAttribute("aria-label", label);
      btn.setAttribute("aria-expanded", String(!isCollapsed()));
    }
    function toggle() {
      const collapsed = root.classList.toggle("toc-collapsed");
      root.classList.remove("toc-peek");
      try {
        localStorage.setItem(kSidebarCollapsedKey, collapsed ? "1" : "0");
      } catch (err) {}
      update();
    }
    if (btn) {
      btn.addEventListener("click", toggle);
    }
    document.addEventListener("keydown", function (e) {
      if (e.key.toLowerCase() !== "b" || e.altKey || e.shiftKey) {
        return;
      }
      if (!(isMac ? e.metaKey : e.ctrlKey)) {
        return;
      }
      e.preventDefault();
      toggle();
    });
    document.addEventListener("mousemove", function (e) {
      if (!isCollapsed()) {
        return;
      }
      if (!root.classList.contains("toc-peek")) {
        if (e.clientX <= kEdgePx) {
          root.classList.add("toc-peek");
        }
        return;
      }
      // shown but the mouse never went into it (e.g. moved right from the edge)
      if (e.clientX > toc.getBoundingClientRect().right) {
        root.classList.remove("toc-peek");
      }
    });
    toc.addEventListener("mouseleave", function () {
      root.classList.remove("toc-peek");
    });
    update();
  }

  // docs screenshots are stored in R2 under assets/sumatrapdf/docs/img/
  const kDocsImgCdn = "https://files.sumatrapdfreader.org/assets/sumatrapdf/docs/img/";

  function docsImgToCdnUrl(src) {
    let s = (src || "").replace(/%20/g, " ").replace(/\\/g, "/");
    if (s.indexOf("https://") === 0 || s.indexOf("http://") === 0) {
      return s;
    }
    if (s.indexOf("./") === 0) {
      s = s.slice(2);
    }
    if (s.indexOf("/img/") === 0) {
      s = s.slice(1);
    }
    if (s.indexOf("img/") === 0) {
      return kDocsImgCdn + s.slice(4);
    }
    return s;
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

    md.renderer.rules.image = function (tokens, idx, options, env, self) {
      const tok = tokens[idx];
      tok.attrSet("src", docsImgToCdnUrl(tok.attrGet("src") || ""));
      return self.renderToken(tokens, idx, options);
    };

    md.renderer.rules.link_open = function (tokens, idx, options, env, self) {
      const tok = tokens[idx];
      let href = tok.attrGet("href") || "";

      const isAbsolute = href.startsWith("https://") || href.startsWith("http://") || href.startsWith("mailto:");

      if (!isAbsolute) {
        const decoded = href.replace(/%20/g, " ");
        const hashIdx = decoded.indexOf("#");
        const fileName = hashIdx >= 0 ? decoded.slice(0, hashIdx) : decoded;
        const hash = hashIdx >= 0 ? decoded.slice(hashIdx + 1) : "";
        const ext = fileName.slice(fileName.lastIndexOf(".")).toLowerCase();
        if (ext === ".md") {
          if (fileName === "SumatraPDF-all-docs-for-llm-ai.md") {
            tok.attrSet("href", "https://www.sumatrapdfreader.org/docs/SumatraPDF-all-docs-for-llm-ai.md");
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
        finalHref.startsWith("https://") || finalHref.startsWith("http://") || finalHref.startsWith("mailto:");
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
      const bcTop = h1BreadcrumbsStartWithToggle + h1Text + h1BreadcrumbsEnd;
      const bc = h1BreadcrumbsStart + h1Text + h1BreadcrumbsEnd;
      innerHTML = bcTop + innerHTML + "<div>&nbsp;</div>" + bc;
    } else {
      // the main page has no breadcrumbs, only the sidebar toggle
      innerHTML = '<div class="breadcrumbs">' + tocToggleHTML + "</div>" + innerHTML;
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
      const js = typeof kCommandsSearchJs === "string" && kCommandsSearchJs ? kCommandsSearchJs : null;
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
        innerSlot.innerHTML = innerSlot.innerHTML.replace("<div>:search:</div>", searchHtml);
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
          const toc = tocSlot.querySelector(".sidebar-toc");
          if (toc) {
            keepSidebarScroll(toc);
          }
        }
        if (innerSlot) {
          innerSlot.innerHTML = rendered.innerHTML;
        }
        const sidebar = tocSlot ? tocSlot.querySelector(".sidebar-toc") : null;
        if (sidebar) {
          setupSidebarToggle(sidebar);
        }
        // Ensure code Copy handlers are bound (gen_code_copy.js). Delegation
        // covers late-injected buttons; this is a safe no-op if already bound.
        if (typeof window.initCodeCopyButtons === "function") {
          window.initCodeCopyButtons();
        }
        if (typeof window.rebuildPageToc === "function") {
          window.rebuildPageToc();
        }
        if (typeof window.initAskAi === "function") {
          window.initAskAi();
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
