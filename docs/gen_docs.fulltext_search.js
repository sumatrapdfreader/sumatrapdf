(function () {
  const isMac = navigator.platform.toUpperCase().indexOf("MAC") >= 0;
  let dialog = null;
  let input = null;
  let resultsDiv = null;
  let askRow = null;
  let debounceTimer = null;
  let selectedIndex = -1;
  let mode = "search";
  const PLACEHOLDER_SEARCH = "Search documentation";
  const PLACEHOLDER_ASK = "Ask a question about SumatraPDF e.g. 'How to configure keyboard shortcuts'";
  const ASK_PREFIX =
    "This is a question about SumatraPDF application (https://www.sumatrapdfreader.org/docs/SumatraPDF-all-docs-for-llm-ai.md). Question: ";
  const AI_URLS = {
    grok: "https://grok.com/?q=",
    chatgpt: "https://chatgpt.com/?q=",
    claude: "https://claude.ai/new?q=",
  };
  const kAllDocsFile = "all-docs.md";
  const kMaxResults = 32;
  let allDocsFiles = null;
  let allDocsLoadPromise = null;

  function removeNotionId(s) {
    if (s.length <= 32) {
      return s;
    }
    if (/^[0-9a-fA-F]{32}$/.test(s.slice(-32))) {
      return s.slice(0, -32);
    }
    return s;
  }

  function mdNameToHtml(mdName) {
    const name = mdName.split("#")[0];
    const base = name.replace(/\.md$/i, "");
    return removeNotionId(base).trim().replace(/ /g, "-") + ".html";
  }

  function parseAllDocs(text) {
    const files = [];
    const re = /^::([^\n]+)\n/gm;
    const markers = [];
    let match;
    while ((match = re.exec(text)) !== null) {
      // index: start of the "::file" marker line; start: first content byte after it.
      // end of this doc is the next marker's index (not start) so we don't include "::next".
      markers.push({
        file: match[1].trim(),
        index: match.index,
        start: match.index + match[0].length,
      });
    }
    for (let i = 0; i < markers.length; i++) {
      const start = markers[i].start;
      const end = i + 1 < markers.length ? markers[i + 1].index : text.length;
      const content = text.slice(start, end);
      const lines = content.split("\n");
      let title = "";
      for (let j = 0; j < lines.length; j++) {
        if (lines[j].startsWith("# ")) {
          title = lines[j].slice(2);
          break;
        }
      }
      files.push({ file: markers[i].file, title: title, lines: lines });
    }
    return files;
  }

  function ensureAllDocsLoaded() {
    if (allDocsFiles) {
      return Promise.resolve(allDocsFiles);
    }
    if (!allDocsLoadPromise) {
      allDocsLoadPromise = fetch(kAllDocsFile)
        .then(function (r) {
          if (!r.ok) {
            throw new Error("failed to load " + kAllDocsFile);
          }
          return r.text();
        })
        .then(function (text) {
          allDocsFiles = parseAllDocs(text);
          return allDocsFiles;
        })
        .catch(function (err) {
          allDocsLoadPromise = null;
          throw err;
        });
    }
    return allDocsLoadPromise;
  }

  function lineMatchesAll(line, terms) {
    const lower = line.toLowerCase();
    return terms.every(function (t) {
      return lower.indexOf(t) >= 0;
    });
  }

  function searchAllDocs(files, query) {
    const terms = query.toLowerCase().split(/\s+/).filter(Boolean);
    if (terms.length === 0) {
      return [];
    }

    const titleResults = [];
    const contentResults = [];
    for (let i = 0; i < files.length; i++) {
      const doc = files[i];
      const title = doc.title || "";

      if (title !== "" && lineMatchesAll(title, terms)) {
        const ctx = [];
        for (let j = 0; j < doc.lines.length; j++) {
          const trimmed = doc.lines[j].trim();
          if (trimmed === "" || trimmed.startsWith("#")) {
            continue;
          }
          ctx.push(doc.lines[j]);
          if (ctx.length >= 3) {
            break;
          }
        }
        titleResults.push({
          file: doc.file,
          title: title,
          text: ctx.join("\n"),
        });
      }

      for (let j = 0; j < doc.lines.length; j++) {
        if (lineMatchesAll(doc.lines[j], terms)) {
          const start = Math.max(0, j - 1);
          const end = Math.min(doc.lines.length, j + 2);
          contentResults.push({
            file: doc.file,
            title: title,
            text: doc.lines.slice(start, end).join("\n"),
          });
          break;
        }
      }

      if (titleResults.length + contentResults.length >= kMaxResults) {
        break;
      }
    }

    const results = titleResults.concat(contentResults);
    if (results.length > kMaxResults) {
      return results.slice(0, kMaxResults);
    }
    return results;
  }

  function createDialog() {
    dialog = document.createElement("div");
    dialog.id = "search-dialog-overlay";
    dialog.innerHTML = `
      <div id="search-dialog">
        <div id="search-top">
          <div id="search-input-wrap">
            <svg class="search-dialog-icon" viewBox="0 0 24 24" aria-hidden="true">
              <path fill="currentColor" d="M10.5 3a7.5 7.5 0 015.926 12.14l3.717 3.717a1 1 0 01-1.414 1.414l-3.717-3.717A7.5 7.5 0 1110.5 3zm0 2a5.5 5.5 0 100 11 5.5 5.5 0 000-11z"></path>
            </svg>
            <input id="search-input" type="text" placeholder="${PLACEHOLDER_SEARCH}" autocomplete="off" />
          </div>
          <button id="search-close-button" type="button" aria-label="Close search" title="Close">
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="M18 6 6 18M6 6l12 12"></path>
            </svg>
          </button>
          <div id="search-mode-row">
            <div id="search-ask">
              <span class="search-ask-label">Ask:</span>
              <button type="button" class="search-ask-btn" data-ai="grok" disabled>Grok</button>
              <button type="button" class="search-ask-btn" data-ai="chatgpt" disabled>ChatGPT</button>
              <button type="button" class="search-ask-btn" data-ai="claude" disabled>Claude</button>
            </div>
            <div id="search-mode" role="radiogroup" aria-label="Search or ask">
              <button type="button" class="search-mode-btn is-selected" data-mode="search" role="radio" aria-checked="true">Search</button>
              <button type="button" class="search-mode-btn" data-mode="ask" role="radio" aria-checked="false">Ask a question</button>
            </div>
          </div>
        </div>
        <div id="search-results"></div>
        <div id="search-help-search" class="search-help">
          <span class="search-help-item"><kbd>↑</kbd><kbd>↓</kbd> to navigate</span>
          <span class="search-help-item"><kbd>↵</kbd> to select</span>
          <span class="search-help-item"><kbd>esc</kbd> to close</span>
        </div>
        <div id="search-help-ask" class="search-help">Write a question and send it to AI of your choice</div>
      </div>
    `;
    document.body.appendChild(dialog);

    const style = document.createElement("style");
    style.textContent = `
      #search-dialog-overlay {
        display: none;
        position: fixed;
        inset: 0;
        background: rgba(0, 0, 0, 0.58);
        z-index: 1000;
        overscroll-behavior: contain;
        overflow: auto;
        box-sizing: border-box;
        padding: 24px 30px;
      }
      #search-dialog {
        width: min(900px, 100%);
        margin: 0 auto;
        padding: 12px;
        background: var(--bg-elevated, #fff);
        color: var(--text-primary, #232323);
        border-radius: 5px;
        box-shadow: 0 18px 50px rgba(0, 0, 0, 0.28);
        box-sizing: border-box;
        overflow: hidden;
      }
      #search-top {
        display: grid;
        grid-template-columns: 1fr auto;
        align-items: center;
        column-gap: 8px;
        row-gap: 10px;
      }
      #search-input-wrap {
        grid-column: 1;
        grid-row: 1;
        min-width: 0;
        display: flex;
        align-items: center;
        gap: 0.3rem;
        height: 38px;
        padding: 0.3rem 0.7rem;
        border: 1px solid var(--search-border, #ddd);
        border-radius: 9999px;
        background: var(--search-bg, #f7f7f7);
        color: var(--search-color, #555);
        box-sizing: border-box;
      }
      .search-dialog-icon {
        flex: 0 0 auto;
        width: 1.05rem;
        height: 1.05rem;
        display: block;
        color: var(--search-color, #555);
      }
      #search-input {
        flex: 1 1 auto;
        min-width: 0;
        height: 100%;
        padding: 0;
        color: var(--text-primary, #232323);
        font: inherit;
        font-size: 16px;
        font-weight: 500;
        line-height: 1.2;
        border: none;
        outline: none;
        background: transparent;
        box-sizing: border-box;
        text-overflow: ellipsis;
      }
      #search-input::placeholder {
        color: var(--search-color, #555);
        font-weight: 500;
        opacity: 1;
      }
      #search-close-button {
        grid-column: 2;
        grid-row: 1;
        width: 30px;
        height: 30px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        margin: 0;
        padding: 0;
        border: 0;
        border-radius: 9999px;
        background: transparent;
        color: var(--search-color, #555);
        cursor: pointer;
      }
      #search-close-button:hover {
        color: var(--text-primary, #111);
        background: rgba(15, 23, 42, 0.06);
      }
      html[data-theme="dark"] #search-close-button:hover {
        color: #fff;
        background: rgba(255, 255, 255, 0.1);
      }
      #search-close-button svg {
        width: 19px;
        height: 19px;
        fill: none;
        stroke: currentColor;
        stroke-width: 2;
        stroke-linecap: round;
        stroke-linejoin: round;
      }
      #search-mode-row {
        grid-column: 1;
        grid-row: 2;
        display: flex;
        flex-wrap: wrap;
        align-items: center;
        justify-content: flex-end;
        gap: 8px 12px;
      }
      #search-dialog.is-ask #search-mode-row {
        justify-content: space-between;
      }
      #search-mode {
        display: inline-flex;
        align-items: center;
        padding: 3px;
        background: var(--nav-track-bg, #f3f3f1);
        border: 1px solid var(--nav-track-border, #e6e6e2);
        border-radius: 9999px;
      }
      .search-mode-btn {
        font: inherit;
        font-size: 14px;
        font-weight: 550;
        padding: 5px 12px;
        border: 0;
        border-radius: 9999px;
        background: transparent;
        color: var(--nav-btn-color, #333);
        cursor: pointer;
        white-space: nowrap;
      }
      .search-mode-btn.is-selected {
        background: #fff000;
        color: #111;
        font-weight: 650;
      }
      .search-mode-btn:not(.is-selected):hover {
        background: #fff9ad;
        color: #111;
      }
      #search-ask {
        display: none;
        flex-wrap: wrap;
        align-items: center;
        gap: 8px;
        color: #666;
        font-size: 14px;
        line-height: 1;
      }
      #search-dialog.is-ask #search-ask {
        display: flex;
      }
      .search-ask-label {
        color: inherit;
      }
      .search-ask-btn {
        display: inline-flex;
        align-items: center;
        font: inherit;
        font-size: 14px;
        font-weight: 500;
        color: var(--search-color, #555);
        padding: 0.3rem 0.7rem;
        border: 1px solid var(--search-border, #ddd);
        border-radius: 9999px;
        background: var(--search-bg, #f7f7f7);
        cursor: pointer;
        white-space: nowrap;
      }
      .search-ask-btn:hover:not(:disabled) {
        background-color: #efefef;
      }
      html[data-theme="dark"] .search-ask-btn:hover:not(:disabled) {
        background-color: #2a2a2a;
      }
      .search-ask-btn:disabled {
        opacity: 0.45;
        cursor: default;
      }
      #search-results {
        max-height: min(60vh, 520px);
        overflow-y: auto;
        overscroll-behavior: contain;
        padding: 0;
        margin-top: 8px;
      }
      #search-results:empty {
        margin-top: 0;
      }
      #search-dialog.is-ask #search-results,
      #search-dialog.is-ask #search-help-search {
        display: none;
      }
      .search-result {
        padding: 0.65rem 0.75rem;
        cursor: pointer;
        border-radius: 4px;
      }
      .search-result.selected {
        background: rgba(15, 23, 42, 0.06);
      }
      html[data-theme="dark"] .search-result.selected {
        background: rgba(255, 255, 255, 0.08);
      }
      .search-result-file {
        color: var(--text-primary, #1f2937);
        font-weight: 600;
        font-size: 0.95rem;
        line-height: 1.25;
        margin-bottom: 0.25rem;
      }
      .search-result-file mark {
        background: #fff3a3;
        color: inherit;
        border-radius: 2px;
        padding: 0 1px;
      }
      .search-result-context {
        font-size: 0.82rem;
        line-height: 1.35;
        color: var(--text-secondary, #60646c);
        white-space: pre-wrap;
        font-family: ui-monospace, SFMono-Regular, Consolas, "Liberation Mono", Menlo, monospace;
      }
      .search-result-context mark {
        background: #fff3a3;
        color: inherit;
        border-radius: 2px;
        padding: 0 1px;
      }
      .search-no-results {
        padding: 1rem 0.75rem 0;
        color: var(--text-secondary, #8a8f98);
        text-align: center;
      }
      .search-load-error {
        padding: 1rem 0.75rem 0;
        color: #b42318;
        text-align: center;
      }
      .search-help {
        display: flex;
        flex-wrap: wrap;
        gap: 16px;
        align-items: center;
        min-height: 22px;
        margin-top: 32px;
        color: #666;
        font-size: 14px;
        line-height: 1.35;
      }
      #search-help-ask {
        display: none;
      }
      #search-dialog.is-ask #search-help-ask {
        display: flex;
      }
      html[data-theme="dark"] .search-help,
      html[data-theme="dark"] #search-ask {
        color: var(--text-secondary, #c8c8c4);
      }
      .search-help-item {
        display: inline-flex;
        align-items: center;
        gap: 4px;
        white-space: nowrap;
      }
      .search-help kbd {
        min-width: 18px;
        height: 22px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        padding: 0 6px;
        border: 1px solid var(--kbd-border, #dedede);
        border-radius: 4px;
        background: var(--kbd-bg, #f7f7f7);
        box-shadow: 0 1px 1px rgba(0, 0, 0, 0.06);
        color: var(--text-secondary, #646464);
        font-family: inherit;
        font-size: 12px;
        line-height: 1;
        box-sizing: border-box;
      }
      @media (max-width: 560px) {
        #search-dialog-overlay {
          padding: 12px;
        }
        #search-dialog {
          padding: 10px;
        }
        #search-ask,
        .search-help {
          gap: 10px;
          font-size: 13px;
        }
        .search-ask-btn,
        .search-mode-btn {
          font-size: 13px;
        }
        .search-help {
          margin-top: 20px;
        }
      }
    `;
    document.head.appendChild(style);

    input = document.getElementById("search-input");
    resultsDiv = document.getElementById("search-results");
    askRow = document.getElementById("search-ask");
    document.getElementById("search-close-button").addEventListener("click", closeDialog);

    document.getElementById("search-mode").addEventListener("click", function (e) {
      const btn = e.target.closest("[data-mode]");
      if (!btn) return;
      setMode(btn.getAttribute("data-mode"));
      input.focus();
    });

    askRow.addEventListener("click", function (e) {
      const btn = e.target.closest("[data-ai]");
      if (!btn || btn.disabled) return;
      openAsk(btn.getAttribute("data-ai"));
    });

    input.addEventListener("input", function () {
      updateAskButtons();
      if (mode !== "search") return;
      clearTimeout(debounceTimer);
      debounceTimer = setTimeout(doSearch, 250);
    });

    input.addEventListener("keydown", function (e) {
      if (e.key === "Escape") {
        closeDialog();
        return;
      }
      if (mode !== "search") return;
      const items = resultsDiv.querySelectorAll(".search-result");
      if (items.length === 0) return;
      if (e.key === "ArrowDown") {
        e.preventDefault();
        setSelected(Math.min(selectedIndex + 1, items.length - 1), items);
      } else if (e.key === "ArrowUp") {
        e.preventDefault();
        setSelected(Math.max(selectedIndex - 1, 0), items);
      } else if (e.key === "Enter") {
        e.preventDefault();
        if (selectedIndex >= 0 && selectedIndex < items.length) {
          items[selectedIndex].click();
        }
      }
    });

    dialog.addEventListener("click", function (e) {
      if (e.target === dialog) {
        closeDialog();
      }
    });

    dialog.addEventListener(
      "wheel",
      function (e) {
        // allow scrolling inside results, block page scroll
        if (!resultsDiv.contains(e.target)) {
          e.preventDefault();
        }
      },
      { passive: false },
    );
  }

  function setSelected(index, items) {
    if (!items) items = resultsDiv.querySelectorAll(".search-result");
    if (selectedIndex >= 0 && selectedIndex < items.length) {
      items[selectedIndex].classList.remove("selected");
    }
    selectedIndex = index;
    if (selectedIndex >= 0 && selectedIndex < items.length) {
      items[selectedIndex].classList.add("selected");
      items[selectedIndex].scrollIntoView({ block: "nearest" });
    }
  }

  function setMode(next) {
    if (next !== "search" && next !== "ask") return;
    mode = next;
    const panel = document.getElementById("search-dialog");
    panel.classList.toggle("is-ask", mode === "ask");
    input.placeholder = mode === "ask" ? PLACEHOLDER_ASK : PLACEHOLDER_SEARCH;
    dialog.querySelectorAll(".search-mode-btn").forEach(function (btn) {
      const on = btn.getAttribute("data-mode") === mode;
      btn.classList.toggle("is-selected", on);
      btn.setAttribute("aria-checked", on ? "true" : "false");
    });
    if (mode === "ask") {
      clearTimeout(debounceTimer);
      resultsDiv.innerHTML = "";
      selectedIndex = -1;
      updateAskButtons();
    } else {
      doSearch();
    }
  }

  function openDialog() {
    if (!dialog) createDialog();
    dialog.style.display = "block";

    input.value = "";
    resultsDiv.innerHTML = "";
    selectedIndex = -1;
    setMode("search");
    updateAskButtons();
    input.focus();
    ensureAllDocsLoaded().catch(function () {
      /* shown on first search */
    });
  }

  window.openSearchDialog = openDialog;

  function updateAskButtons() {
    if (!askRow) return;
    const on = input.value.trim().length > 0;
    askRow.querySelectorAll(".search-ask-btn").forEach(function (btn) {
      btn.disabled = !on;
    });
  }

  function openAsk(provider) {
    const q = input.value.trim();
    if (!q) return;
    const base = AI_URLS[provider];
    if (!base) return;
    window.open(base + encodeURIComponent(ASK_PREFIX + q), "_blank", "noopener,noreferrer");
    closeDialog();
  }

  function closeDialog() {
    if (!dialog) return;
    dialog.style.display = "none";

    const url = new URL(window.location);
    if (url.searchParams.has("ftsearch")) {
      url.searchParams.delete("ftsearch");
      history.replaceState(null, "", url.pathname + url.search + url.hash);
    }
  }

  function renderResults(results, query) {
    if (!results || results.length === 0) {
      resultsDiv.innerHTML = '<div class="search-no-results">No results found</div>';
      selectedIndex = -1;
      return;
    }
    resultsDiv.innerHTML = "";
    selectedIndex = -1;
    results.forEach(function (item, index) {
      const div = document.createElement("div");
      div.className = "search-result";

      const fileDiv = document.createElement("div");
      fileDiv.className = "search-result-file";
      const name = item.title || item.file.replace(/\.md$/, "").replace(/-/g, " ");
      fileDiv.innerHTML = highlightText(name, query);

      const ctxDiv = document.createElement("div");
      ctxDiv.className = "search-result-context";
      ctxDiv.innerHTML = highlightText(item.text, query);

      div.appendChild(fileDiv);
      div.appendChild(ctxDiv);
      div.addEventListener("click", function () {
        const matchedLine = findMatchedLine(item.text, query);
        let url = mdNameToHtml(item.file);
        if (matchedLine) {
          url += "#:~:text=" + encodeURIComponent(matchedLine);
        }
        closeDialog();
        window.location.href = url;
      });
      div.addEventListener("mouseenter", function () {
        setSelected(index);
      });
      resultsDiv.appendChild(div);
    });
  }

  function doSearch() {
    if (mode !== "search") return;
    const query = input.value.trim();
    if (query.length === 0) {
      resultsDiv.innerHTML = "";
      selectedIndex = -1;
      return;
    }
    ensureAllDocsLoaded()
      .then(function (files) {
        if (mode !== "search") return;
        renderResults(searchAllDocs(files, query), query);
      })
      .catch(function () {
        resultsDiv.innerHTML = '<div class="search-load-error">Could not load documentation index</div>';
        selectedIndex = -1;
      });
  }

  function findMatchedLine(text, query) {
    const lines = text.split("\n");
    const terms = query.toLowerCase().split(/\s+/).filter(Boolean);
    for (let i = 0; i < lines.length; i++) {
      if (lineMatchesAll(lines[i], terms)) {
        return lines[i].trim();
      }
    }
    return "";
  }

  function escapeHtml(s) {
    const div = document.createElement("div");
    div.textContent = s;
    return div.innerHTML;
  }

  function highlightText(text, query) {
    const escaped = escapeHtml(text);
    const terms = query.split(/\s+/).filter(Boolean);
    let result = escaped;
    terms.forEach(function (term) {
      const re = new RegExp("(" + escapeRegex(term) + ")", "gi");
      result = result.replace(re, "<mark>$1</mark>");
    });
    return result;
  }

  function escapeRegex(s) {
    return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  }

  // check for ?ftsearch= on startup
  const initQuery = new URLSearchParams(window.location.search).get("ftsearch");
  if (initQuery) {
    if (!dialog) createDialog();
    dialog.style.display = "block";

    input.value = initQuery;
    setMode("search");
    updateAskButtons();
    input.focus();
    doSearch();
  }

  document.addEventListener("keydown", function (e) {
    const modKey = isMac ? e.metaKey : e.ctrlKey;
    if (modKey && e.key === "k") {
      e.preventDefault();
      if (dialog && dialog.style.display === "block") {
        closeDialog();
      } else {
        openDialog();
      }
    }
  });
})();
