<script>
  import { version } from "./version";
  import { onMount } from "svelte";

  let autoScrollPaused = false;
  let btnText = $state("pause scrolling");
  let filter = $state("");
  let filterLC = $derived(filter.trim().toLowerCase());

  let hiliRegExp = $derived(makeHilightRegExp(filter));

  /** @type {HTMLElement} */
  let logAreaEl;
  /** @type {HTMLElement} */
  let searchEl;

  class TabInfo {
    /** @type {number} */
    appNo;
    /** @type {string[]} */
    logs = $state([]);
    tabName = $state("logs");
    constructor(no) {
      this.appNo = no;
    }
  }

  /** @type {TabInfo[]} */
  let tabs = $state([]);

  /** @type {TabInfo} */
  let selectedTab = $state(new TabInfo(-1));

  let filteredLogs = $derived(filterLogs(filterLC, selectedTab.logs));

  $effect(() => {
    if (!autoScrollPaused && len(filteredLogs) > 0) {
      scrollToBottom(logAreaEl);
    }
  });

  function genLotsOfLogs() {
    let no = len(tabs) + 1;
    for (let i = 0; i < 10000; i++) {
      let s = `this is a line number ${i}`;
      plog(no, s);
    }
  }

  /**
   * @param {KeyboardEvent} ev
   */
  function keydown(ev) {
    if (ev.key === "/") {
      if (searchEl) {
        searchEl.focus();
      }
      ev.preventDefault();
      return;
    }
    let isSearchFocused = document.activeElement === searchEl;
    if (isSearchFocused) {
      if (ev.key === "Escape") {
        // if searchEl is focused, return focus to logAreaEl
        logAreaEl.focus();
        filter = "";
        return;
      }
    } else {
      if (ev.key === "p") {
        togglePauseScrolling();
        ev.preventDefault();
        return;
      }
      let tabNo = ev.keyCode - "0".charCodeAt(0) - 1;
      // console.log(ev.keyCode, tabNo);
      if (tabNo >= 0 && tabNo < 9) {
        if (len(tabs) > tabNo) {
          selectedTab = tabs[tabNo];
        }
      }
    }
  }

  onMount(() => {
    window.addEventListener("keydown", keydown, true);
    window.addEventListener("beforeunload", (ev) => {
      // fetch("/kill", { method: "post" });
    });
    // console.log("registering EventSource");
    const source = new EventSource("/sse");
    source.onopen = (ev) => {
      // console.log("event source opened", ev);
    };
    source.onerror = (ev) => {
      console.log("event source error", ev);
    };

    /**
     * @param {MessageEvent} ev
     */
    source.onmessage = (ev) => {
      // console.log(ev.data);
      let js = JSON.parse(ev.data);
      plog(js.ConnNo, js.Line);
    };
  });

  /**
   * @param {number} appNo
   * @returns {TabInfo}
   */
  function findOrCreateTab(appNo) {
    for (let tab of tabs) {
      if (tab.appNo === appNo) {
        return tab;
      }
    }
    let tab = new TabInfo(appNo);
    tabs.push(tab);
    selectedTab = tab;
    return tab;
  }

  /**
   * @param {number} no
   * @param {string} line
   */
  function plog(no, line) {
    line = line.trim();
    let tab = findOrCreateTab(no);
    tab.logs.push(line);
    if (line.startsWith("app: ")) {
      tab.tabName = line.slice(4);
    }
  }

  /**
   * @param {number} n
   * @returns {number[]}
   */
  function mkArrayOfNumbers(n) {
    let res = Array(n);
    for (let i = 0; i < n; i++) {
      res[i] = i;
    }
    return res;
  }

  /**
   * @param {string} filterLC
   * @param {string[]} logs
   */
  function filterLogs(filterLC, logs) {
    let n = len(logs);
    if (filterLC === "") {
      return mkArrayOfNumbers(n);
    }
    let parts = filterLC.split(" ");
    for (let i = 0; i < parts.length; i++) {
      parts[i] = parts[i].trim();
    }
    let res = [];
    for (let i = 0; i < n; i++) {
      let line = logs[i];
      if (matches(line, parts)) {
        res.push(i);
      }
    }
    return res;
  }

  /**
   * @param {string} s
   * @param {string[]} searchTerms
   */
  function matches(s, searchTerms) {
    s = s.toLowerCase();
    for (let term of searchTerms) {
      if (!s.includes(term)) {
        return false;
      }
    }
    return true;
  }

  /**
   * @param {string} filter
   * @returns {RegExp}
   */
  export function makeHilightRegExp(filter) {
    let parts = filter.split(" ");
    let a = [];
    for (let s of parts) {
      s = s.trim().toLowerCase();
      let escaped = s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
      a.push(escaped);
    }
    let s = a.join("|");
    return new RegExp(`(${s})`, "gi");
  }

  export function hilightText(s, regexp) {
    // console.log("hilightText:", s, regexp);
    return s.replace(regexp, '<span class="hili">$1</span>');
  }

  /**
   * @param {HTMLElement} node
   */
  function scrollToBottom(node) {
    // node.scroll({ top: node.scrollHeight, behavior: "smooth" });
    node.scroll({ top: node.scrollHeight });
  }

  let windowTitle = "Logview " + version;
  // @ts-ignore
  window.runtime?.WindowSetTitle(windowTitle);

  function togglePauseScrolling() {
    autoScrollPaused = !autoScrollPaused;
    if (autoScrollPaused) {
      btnText = "unpause scrolling (p)";
    } else {
      btnText = "pause scrolling (p)";
    }
  }

  function len(o) {
    return o ? o.length : 0;
  }
  function clearLogs() {
    if (selectedTab) {
      selectedTab.logs = [];
    }
  }

  function selectTab(tab) {
    selectedTab = tab;
  }

  function aboutClicked() {
    let uri = "https://www.sumatrapdfreader.org/docs/Logview";
    // @ts-ignore
    window.runtime?.BrowserOpenURL(uri);
  }
  // @ts-ignore
  //window.runtime.EventsOn("plog", plog);
</script>

<main class="flex flex-col">
  <div class="top">
    <div style="flex-grow: 1"></div>
    <input
      type="text"
      placeholder="filter /"
      bind:this={searchEl}
      bind:value={filter}
    />
    <button class="btn-pause" onclick={togglePauseScrolling}>{btnText}</button>
    <button onclick={clearLogs}>clear</button>
    <div>showing {len(filteredLogs)} out of {len(selectedTab.logs)}</div>
    <div style="flex-grow: 1"></div>
    <button class="hidden2" onclick={() => genLotsOfLogs()}
      >gen test data</button
    >
    <button onclick={aboutClicked}>about</button>
  </div>

  <div class="tabs">
    {#each tabs as tab, idx}
      {#if tab === selectedTab}
        <button class="tab tab-selected">{tab.tabName} ({idx + 1})</button>
      {:else}
        <button onclick={() => selectTab(tab)} class="tab"
          >{tab.tabName} ({idx + 1})</button
        >
      {/if}
    {/each}
  </div>

  <div bind:this={logAreaEl} tabindex="0" role="listbox" class="log-area grow">
    {#if !selectedTab}
      <div class="no-results">No logs yet</div>
    {:else if len(filteredLogs) == 0}
      {#if len(selectedTab.logs) == 0}
        <div class="no-results">No logs yet</div>
      {:else}
        <div class="no-results">No results matching '<b>{filter}</b>'</div>
      {/if}
    {:else}
      {#each filteredLogs as logIdx (logIdx)}
        {@const line = selectedTab.logs[logIdx]}
        {#if filter === ""}
          <span class="log-line">{line}</span><br />
        {:else}
          {@const hili = hilightText(line, hiliRegExp)}
          <span class="log-line">{@html hili}</span><br />
        {/if}
      {/each}
    {/if}
  </div>
</main>

<style>
  .flex {
    display: flex;
  }
  .flex-col {
    flex-direction: column;
  }
  .grow {
    flex-grow: 1;
  }
  .hidden {
    display: none;
  }

  main {
    min-height: 0;
    height: 100vh;
    overflow: auto;
  }

  .top {
    display: flex;
    justify-content: center;
    align-items: baseline;
    column-gap: 0.5rem;
    padding-top: 4px;
    padding-bottom: 6px;
  }

  .tabs {
    display: flex;
  }

  .tab {
    border: 0;
    padding: 4px 1rem;
    cursor: pointer;
    &:hover {
      background-color: rgba(128, 128, 128, 0.2);
    }
  }
  .tab-selected {
    background-color: white;
  }
  .no-results {
    text-align: center;
    font-size: 120%;
    margin-top: 30%; /* from eyeballing */
    /* background-color: bisque; */
  }
  .btn-pause {
    min-width: 8rem;
  }

  .log-area {
    overflow: auto;
    padding: 4px 1rem;
    /* background: rgb(239, 250, 254); */
    background-color: white;
    font-family: monospace;
  }
  .log-line {
    content-visibility: auto;
    /* contain-intrinsic-size: 1rem; */
  }

  :global(.hili) {
    background-color: yellow;
  }
</style>
