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
  let scriptExamplesCleanup = null;
  const kPDFScriptsURL = /*PDF_SCRIPTS_URL*/ "";
  const kScriptExamplesPage = "Tool-run-javascript-examples.md";
  let featuresCleanup = null;
  const kFeaturesPage = "Features.md";

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
    return '\n<div class="doc-video">' + youTubeIframeHTML(youTubeId, "Video", "") + "</div>\n";
  }

  function youTubeIframeHTML(youTubeId, title, cls) {
    return (
      "<iframe" +
      (cls ? ' class="' + cls + '"' : "") +
      ' src="https://www.youtube-nocookie.com/embed/' +
      youTubeId +
      '" title="' +
      title +
      '" loading="lazy" allow="accelerometer; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>'
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
        if (line.trim() === ":ask-ai-to-write-script") {
          return '\n<div class="askai" data-script-examples-url="' + kPDFScriptsURL + '"></div>\n';
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
      // a diagram drawn inline; colors come from the doc-svg classes in sumatra.css
      // (see renderSVGCodeBlock() in the website's server/gen_manual.go, which also checks it)
      if (lang === "svg") return '<div class="doc-svg">' + t.content.trim() + "</div>\n";
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

    const md = global.markdownit({ html: true, typographer: true });
    createMarkdownRenderer(md);
    if (mdName === kScriptExamplesPage) {
      text = renderScriptExamples(text, md.utils.escapeHtml);
    }
    if (mdName === kFeaturesPage) {
      text = renderFeatures(text, md.utils.escapeHtml);
    }
    text = preProcess(text);
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

  function renderScriptExamples(text, escape) {
    return text.replace(/^## Examples\r?\n([\s\S]*?)(?=^## |$(?![\s\S]))/m, function (_, list) {
      const links = Array.from(list.matchAll(/^- \[([^\]]+)\]\(scripts\/([a-zA-Z0-9_-]+\.js)\)\s*$/gm));
      if (!links.length) throw new Error("No script examples listed.");
      const items = links.map(function (link, index) {
        const title = escape(link[1]);
        const name = escape(link[2]);
        const label =
          index === 0 ? '<span aria-current="true">' + title + "</span>" : '<a href="#' + name + '">' + title + "</a>";
        return '<span data-script="' + name + '" data-title="' + title + '">' + label + "</span>";
      });
      return (
        '<div class="script-examples" data-selected="">\n' +
        '<nav class="script-example-list" aria-label="JavaScript examples">' +
        items.join('<span class="script-example-dot" aria-hidden="true"> &bull; </span>') +
        "</nav>\n" +
        '<p class="script-example-hint">Choose an example, or use the left and right arrow keys.</p>\n' +
        '<div class="script-example-head"><h2 class="script-example-title" id="script-example-title">' +
        escape(links[0][1]) +
        '</h2><div class="script-example-arrows">' +
        '<button type="button" data-step="-1" aria-label="Previous example" title="Previous example (left arrow)">&larr;</button>' +
        '<button type="button" data-step="1" aria-label="Next example" title="Next example (right arrow)">&rarr;</button>' +
        "</div></div>\n" +
        '<div class="script-example-status" role="status"></div>\n' +
        '<div class="script-example-content"><p>Loading...</p></div>\n</div>\n\n'
      );
    });
  }

  function scriptContentHTML(name, source) {
    const escape = global.markdownit().utils.escapeHtml;
    let info = "";
    const args = [];
    const discussions = [];
    source.split("\n").forEach(function (line) {
      line = line.trim();
      const discussion = /^\/\/ Discussion #([0-9]+):/.exec(line);
      if (discussion) discussions.push(discussion[1]);
      if (line.startsWith("// Info:")) info = line.slice("// Info:".length).trim();
      if (line.startsWith("// Arg:")) {
        const spec = line.slice("// Arg:".length);
        const colon = spec.indexOf(" : ");
        args.push({
          value: colon < 0 ? "" : spec.slice(0, colon).trim(),
          hint: (colon < 0 ? spec : spec.slice(colon + " : ".length)).trim(),
        });
      }
    });
    let html = info ? "<p>" + escape(info) + "</p>" : "";
    const discussion = /^discussion-([0-9]+)-/.exec(name);
    if (!discussions.length && discussion) discussions.push(discussion[1]);
    if (discussions.length) {
      html +=
        "<p>From " +
        discussions
          .map(function (number) {
            return (
              '<a href="https://github.com/sumatrapdfreader/sumatrapdf/discussions/' +
              number +
              '" target="_blank" rel="noopener noreferrer">discussion #' +
              number +
              "</a>"
            );
          })
          .join(", ") +
        "</p>";
    }
    html +=
      '<p><a href="https://www.sumatrapdfreader.org/docs/scripts/' +
      escape(name) +
      '" target="_blank" rel="noopener noreferrer">Download script</a></p>';
    if (args.length) {
      html +=
        '<ol class="script-example-args" aria-label="Script arguments">' +
        args
          .map(function (arg) {
            const initial = arg.value ? " (default: <code>" + escape(arg.value) + "</code>)" : "";
            return "<li>" + escape(arg.hint) + initial + "</li>";
          })
          .join("") +
        "</ol>";
    }
    return html + genCodeBlockHTML(escape(source), "language-js");
  }

  function initScriptExamples(root) {
    if (scriptExamplesCleanup) scriptExamplesCleanup();
    scriptExamplesCleanup = null;
    const examples = root && root.querySelector(".script-examples");
    if (!examples) return Promise.resolve();
    const items = Array.from(examples.querySelectorAll("[data-script]"));
    const panel = examples.querySelector(".script-example-content");
    const title = examples.querySelector(".script-example-title");
    const status = examples.querySelector(".script-example-status");
    const cache = new Map();
    let selected = 0;
    let requested = 0;
    let requestID = 0;
    let disposed = false;

    async function select(index) {
      requested = index;
      const currentRequest = ++requestID;
      const name = items[index].dataset.script;
      panel.setAttribute("aria-busy", "true");
      status.textContent = "";
      try {
        if (!cache.has(name)) {
          cache.set(name, scriptContentHTML(name, await fetchText("scripts/" + name)));
        }
        if (disposed || currentRequest !== requestID) return;
        selected = index;
        items.forEach(function (item, i) {
          const label = document.createElement(i === index ? "span" : "a");
          label.textContent = item.dataset.title;
          if (i === index) label.setAttribute("aria-current", "true");
          else label.href = "#" + item.dataset.script;
          item.replaceChildren(label);
        });
        title.textContent = items[index].dataset.title;
        panel.innerHTML = cache.get(name);
        examples.dataset.selected = name;
        if (typeof global.rebuildPageToc === "function") global.rebuildPageToc();
      } catch (err) {
        if (disposed || currentRequest !== requestID) return;
        requested = selected;
        status.textContent = "Could not load that example. Choose it again to retry.";
      } finally {
        if (!disposed && currentRequest === requestID) panel.removeAttribute("aria-busy");
      }
    }

    function navigate(index) {
      global.history.pushState(null, "", "#" + items[index].dataset.script);
      return select(index);
    }
    function step(direction) {
      return navigate((requested + direction + items.length) % items.length);
    }
    examples.querySelectorAll("[data-step]").forEach(function (button) {
      button.addEventListener("click", function () {
        step(Number(button.dataset.step));
      });
    });
    examples.querySelector("nav").addEventListener("click", function (event) {
      const link = event.target.closest("a");
      if (!link || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
      const index = items.indexOf(link.closest("[data-script]"));
      if (index < 0) return;
      event.preventDefault();
      navigate(index);
    });
    function onKey(event) {
      if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
      if (event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) return;
      if (event.target.closest("input, textarea, select, [contenteditable]")) return;
      event.preventDefault();
      step(event.key === "ArrowRight" ? 1 : -1);
    }
    function selectHash() {
      const index = items.findIndex(function (item) {
        return "#" + item.dataset.script === global.location.hash;
      });
      return select(index < 0 ? 0 : index);
    }
    document.addEventListener("keydown", onKey);
    global.addEventListener("popstate", selectHash);
    global.addEventListener("hashchange", selectHash);
    scriptExamplesCleanup = function () {
      disposed = true;
      document.removeEventListener("keydown", onKey);
      global.removeEventListener("popstate", selectHash);
      global.removeEventListener("hashchange", selectHash);
    };
    return selectHash();
  }

  // Features.md: "- [title](Doc.md) <youtube link> <r2 link>" lines, shown as a "&bull;" separated
  // list of feature names above one video; clicking a name switches the video (initFeatures).
  // Same as renderFeatures() in the website's server/features.go
  function renderFeatures(text, escape) {
    return text.replace(/^## Features\r?\n([\s\S]*?)(?=^## |$(?![\s\S]))/m, function (_, list) {
      const features = [];
      list.split("\n").forEach(function (line) {
        line = line.trim();
        if (!line) return;
        const m = /^- \[([^\]]+)\]\(([a-zA-Z0-9_-]+\.md)\)[ \t]+(\S+)[ \t]+(\S+)$/.exec(line);
        const yt = m && /^https:\/\/(?:youtu\.be\/|(?:www\.)?youtube\.com\/watch\?v=)([A-Za-z0-9_-]{11})/.exec(m[3]);
        if (!m || !yt || !/^https:\/\/files\.sumatrapdfreader\.org\/\S+\.mp4$/.test(m[4])) {
          throw new Error(kFeaturesPage + ": bad line " + line);
        }
        features.push({
          slug: m[1].toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, ""),
          title: escape(m[1]),
          doc: escape(getHTMLFileName(m[2])),
          youtube: yt[1],
          r2: escape(m[4]),
        });
      });
      if (!features.length) throw new Error(kFeaturesPage + ": no features");
      const items = features.map(function (f, index) {
        const label =
          index === 0 ? '<span aria-current="true">' + f.title + "</span>" : '<a href="#' + f.slug + '">' + f.title + "</a>";
        return (
          '<span data-feature="' +
          f.slug +
          '" data-title="' +
          f.title +
          '" data-doc="' +
          f.doc +
          '" data-youtube="' +
          f.youtube +
          '" data-r2="' +
          f.r2 +
          '">' +
          label +
          "</span>"
        );
      });
      const first = features[0];
      return (
        '<div class="feature-videos" data-selected="' +
        first.slug +
        '">\n' +
        '<nav class="feature-list" aria-label="Features">' +
        items.join('<span class="feature-dot" aria-hidden="true"> &bull; </span>') +
        "</nav>\n" +
        '<p class="feature-hint">Choose a feature, or use the left and right arrow keys.</p>\n' +
        '<div class="feature-head"><h2 class="feature-title">' +
        first.title +
        '</h2><div class="feature-arrows">' +
        '<button type="button" data-step="-1" aria-label="Previous feature" title="Previous feature (left arrow)">&larr;</button>' +
        '<button type="button" data-step="1" aria-label="Next feature" title="Next feature (right arrow)">&rarr;</button>' +
        "</div></div>\n" +
        '<div class="doc-video">' +
        youTubeIframeHTML(first.youtube, first.title, "feature-player") +
        "</div>\n" +
        '<p class="feature-links"><a class="feature-doc" href="' +
        first.doc +
        '">Read more about ' +
        first.title +
        "</a> &bull; " +
        '<a class="feature-r2" href="' +
        first.r2 +
        '" target="_blank" rel="noopener noreferrer">Watch without YouTube</a></p>\n</div>\n\n'
      );
    });
  }

  function initFeatures(root) {
    if (featuresCleanup) featuresCleanup();
    featuresCleanup = null;
    const features = root && root.querySelector(".feature-videos");
    if (!features) return;
    const items = Array.from(features.querySelectorAll("[data-feature]"));
    const title = features.querySelector(".feature-title");
    const player = features.querySelector(".feature-player");
    const docLink = features.querySelector(".feature-doc");
    const r2Link = features.querySelector(".feature-r2");
    let selected = -1;

    function select(index) {
      if (index === selected) return;
      selected = index;
      const item = items[index].dataset;
      items.forEach(function (el, i) {
        const label = document.createElement(i === index ? "span" : "a");
        label.textContent = el.dataset.title;
        if (i === index) label.setAttribute("aria-current", "true");
        else label.href = "#" + el.dataset.feature;
        el.replaceChildren(label);
      });
      title.textContent = item.title;
      const src = "https://www.youtube-nocookie.com/embed/" + item.youtube;
      if (player.getAttribute("src") !== src) player.setAttribute("src", src);
      player.title = item.title;
      docLink.href = item.doc;
      docLink.textContent = "Read more about " + item.title;
      r2Link.href = item.r2;
      features.dataset.selected = item.feature;
    }

    function navigate(index) {
      global.history.pushState(null, "", "#" + items[index].dataset.feature);
      select(index);
    }
    function step(direction) {
      navigate((selected + direction + items.length) % items.length);
    }
    features.querySelectorAll("[data-step]").forEach(function (button) {
      button.addEventListener("click", function () {
        step(Number(button.dataset.step));
      });
    });
    features.querySelector("nav").addEventListener("click", function (event) {
      const link = event.target.closest("a");
      if (!link || event.ctrlKey || event.metaKey || event.shiftKey || event.altKey) return;
      const index = items.indexOf(link.closest("[data-feature]"));
      if (index < 0) return;
      event.preventDefault();
      navigate(index);
    });
    function onKey(event) {
      if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
      if (event.ctrlKey || event.metaKey || event.altKey || event.shiftKey) return;
      if (event.target.closest("input, textarea, select, [contenteditable]")) return;
      event.preventDefault();
      step(event.key === "ArrowRight" ? 1 : -1);
    }
    function selectHash() {
      const index = items.findIndex(function (item) {
        return "#" + item.dataset.feature === global.location.hash;
      });
      select(index < 0 ? 0 : index);
    }
    document.addEventListener("keydown", onKey);
    global.addEventListener("popstate", selectHash);
    global.addEventListener("hashchange", selectHash);
    featuresCleanup = function () {
      document.removeEventListener("keydown", onKey);
      global.removeEventListener("popstate", selectHash);
      global.removeEventListener("hashchange", selectHash);
    };
    selectHash();
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
        initFeatures(innerSlot);
        if (mdName === "Commands.md") {
          initScriptExamples(innerSlot);
          return injectCommandsSearch(innerSlot);
        }
        return initScriptExamples(innerSlot);
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
