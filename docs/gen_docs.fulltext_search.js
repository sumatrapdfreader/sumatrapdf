(function () {
  const isMac = navigator.platform.toUpperCase().indexOf("MAC") >= 0;
  const kAllDocsFile = "all-docs.md";
  const kMaxResults = 32;

  let dialog = null;
  let input = null;
  let resultsDiv = null;
  let debounceTimer = null;
  let selectedIndex = -1;
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
      markers.push({ file: match[1].trim(), start: match.index + match[0].length });
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
        <div id="search-input-wrap">
          <svg class="search-dialog-icon" viewBox="0 0 24 24" aria-hidden="true">
            <path d="m21 21-4.35-4.35m2.35-5.15a7.5 7.5 0 1 1-15 0 7.5 7.5 0 0 1 15 0Z"></path>
          </svg>
          <input id="search-input" type="text" placeholder="Search documentation" autocomplete="off" />
          <button id="search-close-button" type="button" aria-label="Close search" title="Close">
            <svg viewBox="0 0 24 24" aria-hidden="true">
              <path d="M8 8h8v8H8z"></path>
              <path d="m10 10 4 4m0-4-4 4"></path>
            </svg>
          </button>
        </div>
        <div id="search-results"></div>
        <div id="search-help">
          <span class="search-help-item"><kbd>↑</kbd><kbd>↓</kbd> to navigate</span>
          <span class="search-help-item"><kbd>↵</kbd> to select</span>
          <span class="search-help-item"><kbd>esc</kbd> to close</span>
        </div>
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
        background: #fff;
        border-radius: 5px;
        box-shadow: 0 18px 50px rgba(0, 0, 0, 0.28);
        box-sizing: border-box;
        overflow: hidden;
      }
      #search-input-wrap {
        display: flex;
        align-items: center;
        height: 38px;
        border: 1px solid #dedede;
        border-radius: 3px;
        background: #fff;
        box-sizing: border-box;
      }
      .search-dialog-icon {
        flex: 0 0 auto;
        width: 18px;
        height: 18px;
        margin-left: 20px;
        margin-right: 14px;
        stroke: #111827;
        stroke-width: 2;
        fill: none;
        stroke-linecap: round;
        stroke-linejoin: round;
      }
      #search-input {
        flex: 1 1 auto;
        min-width: 0;
        height: 100%;
        padding: 0;
        color: #202124;
        font-size: 16px;
        line-height: 38px;
        border: none;
        outline: none;
        background: transparent;
        box-sizing: border-box;
      }
      #search-input::placeholder {
        color: #969696;
        opacity: 1;
      }
      #search-close-button {
        flex: 0 0 auto;
        width: 30px;
        height: 30px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        margin: 0 4px 0 0;
        padding: 0;
        border: 0;
        border-radius: 3px;
        background: transparent;
        cursor: pointer;
      }
      #search-close-button:hover {
        background: #f4f4f4;
      }
      #search-close-button svg {
        width: 19px;
        height: 19px;
        fill: none;
        stroke: #a6a6a6;
        stroke-width: 2;
        stroke-linecap: round;
        stroke-linejoin: round;
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
      .search-result {
        padding: 0.65rem 0.75rem;
        cursor: pointer;
        border-radius: 4px;
      }
      .search-result.selected {
        background: #f4f6ff;
      }
      .search-result-file {
        color: #1f2937;
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
        color: #60646c;
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
        color: #8a8f98;
        text-align: center;
      }
      .search-load-error {
        padding: 1rem 0.75rem 0;
        color: #b42318;
        text-align: center;
      }
      #search-help {
        display: flex;
        flex-wrap: wrap;
        gap: 16px;
        align-items: center;
        min-height: 22px;
        margin-top: 32px;
        color: #666;
        font-size: 14px;
        line-height: 1;
      }
      .search-help-item {
        display: inline-flex;
        align-items: center;
        gap: 4px;
        white-space: nowrap;
      }
      #search-help kbd {
        min-width: 18px;
        height: 22px;
        display: inline-flex;
        align-items: center;
        justify-content: center;
        padding: 0 6px;
        border: 1px solid #dedede;
        border-radius: 4px;
        background: #f7f7f7;
        box-shadow: 0 1px 1px rgba(0, 0, 0, 0.06);
        color: #646464;
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
        .search-dialog-icon {
          margin-left: 12px;
          margin-right: 10px;
        }
        #search-help {
          gap: 10px;
          margin-top: 20px;
          font-size: 13px;
        }
      }
    `;
    document.head.appendChild(style);

    input = document.getElementById("search-input");
    resultsDiv = document.getElementById("search-results");
    document.getElementById("search-close-button").addEventListener("click", closeDialog);

    input.addEventListener("input", function () {
      clearTimeout(debounceTimer);
      debounceTimer = setTimeout(doSearch, 250);
    });

    input.addEventListener("keydown", function (e) {
      if (e.key === "Escape") {
        closeDialog();
        return;
      }
      const items = resultsDiv.querySelectorAll(".search-result");
      if (items.length === 0) {
        return;
      }
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
        if (!resultsDiv.contains(e.target)) {
          e.preventDefault();
        }
      },
      { passive: false },
    );
  }

  function setSelected(index, items) {
    if (!items) {
      items = resultsDiv.querySelectorAll(".search-result");
    }
    if (selectedIndex >= 0 && selectedIndex < items.length) {
      items[selectedIndex].classList.remove("selected");
    }
    selectedIndex = index;
    if (selectedIndex >= 0 && selectedIndex < items.length) {
      items[selectedIndex].classList.add("selected");
      items[selectedIndex].scrollIntoView({ block: "nearest" });
    }
  }

  function openDialog() {
    if (!dialog) {
      createDialog();
    }
    dialog.style.display = "block";

    input.value = "";
    resultsDiv.innerHTML = "";
    selectedIndex = -1;
    input.focus();
    ensureAllDocsLoaded().catch(function () {
      /* shown on first search */
    });
  }

  window.openSearchDialog = openDialog;

  function closeDialog() {
    if (!dialog) {
      return;
    }
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
    const query = input.value.trim();
    if (query.length === 0) {
      resultsDiv.innerHTML = "";
      selectedIndex = -1;
      return;
    }
    ensureAllDocsLoaded()
      .then(function (files) {
        renderResults(searchAllDocs(files, query), query);
      })
      .catch(function () {
        resultsDiv.innerHTML =
          '<div class="search-load-error">Could not load documentation index</div>';
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

  const initQuery = new URLSearchParams(window.location.search).get("ftsearch");
  if (initQuery) {
    if (!dialog) {
      createDialog();
    }
    dialog.style.display = "block";
    input.value = initQuery;
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