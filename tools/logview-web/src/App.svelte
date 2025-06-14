<script>
  import { version } from "./version";
  import { onMount } from "svelte";
  import { VList } from "virtua/svelte";

  let autoScrollPaused = false;
  let btnText = $state("pause scrolling (p)");
  let filter = $state("");
  let filterLC = $derived(filter.trim().toLowerCase());

  /** @type {VList<{ id: number; size: string }>}*/
  let vlist = $state();

  let hiliRegExp = $derived(makeHilightRegExp(filter));

  /** @type {HTMLElement} */
  // let logAreaEl;
  /** @type {HTMLElement} */
  let searchEl;

  class TabInfo {
    /** @type {number} */
    appNo;
    /** @type {string[]} */
    logs = $state([]);
    tabName = $state("logs");
    // array, each value is array of 3 elements:
    // [name, type, value]
    values = $state([]);
    constructor(no) {
      this.appNo = no;
    }
  }

  /** @type {TabInfo[]} */
  let tabs = $state([]);

  /** @type {number} */
  let selectedTabIdx = $state(-1);

  let currentTabValues = $derived(selectTabValues(selectedTabIdx));
  function selectTabValues(tabIdx) {
    if (tabIdx < 0) {
      return [];
    }
    return tabs[tabIdx].values;
  }

  let filteredLogs = $derived(filterLogs(filterLC, selectedTabIdx));

  $effect(() => {
    if (!autoScrollPaused) {
      let n = len(filteredLogs);
      if (n > 0) {
        vlist.scrollToIndex(n - 1);
      }
    }
  });

  let itemsCountMsg = $derived.by(() => {
    let n = len(filteredLogs);
    let n2 = getLogsCount();
    if (n == n2) {
      return `${n2} lines`;
    }
    return `${n} of ${n2} lines`;
  });

  function genLotsOfLogs() {
    let no = 3798; // random but unique number
    for (let i = 0; i < 10000; i++) {
      let s = `this is a line number ${i}`;
      if (i % 1000 == 0) {
        s = `:v cacheSize${i} bs ${i}`;
      }
      plog(no, s);
    }
  }

  /**
   * @param {number} tabIdx
   */
  function closeTab(tabIdx) {
    tabs.splice(tabIdx, 1);
    if (len(tabs) === 0) {
      selectedTabIdx = -1;
      return;
    }
    // update selected tab
    if (selectedTabIdx >= len(tabs)) {
      --selectedTabIdx;
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
        searchEl.blur();
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
          selectedTabIdx = tabNo;
        }
      }
    }
  }

  function fetchLogsIncrements() {
    /**
     * @param {Response} rsp
     */
    function handleRsp(rsp) {
      if (!rsp.ok || rsp.status != 200) {
        console.log("fetch failed:", rsp);
        scheduleNextLogsIncrementalFetch(5000);
        return;
      }
      function handleJSON(js) {
        if (!Array.isArray(js)) {
          console.log("unexpected json result, not array:", js);
          scheduleNextLogsIncrementalFetch(1000);
          return;
        }
        let n = len(js);
        if (n === 0) {
          scheduleNextLogsIncrementalFetch(100);
          return;
        }
        let nLogs = n / 2;
        console.log(`got ${nLogs} logs`);
        for (let i = 0; i < n; i += 2) {
          plog(js[i], js[i + 1]);
        }
        if (nLogs === 1000) {
          // we got the max logs so re-get immediately
          scheduleNextLogsIncrementalFetch(1);
        } else {
          scheduleNextLogsIncrementalFetch(1000);
        }
      }
      rsp.json().then(handleJSON);
    }
    fetch("/api/getlogsincremental").then(handleRsp);
  }

  function scheduleNextLogsIncrementalFetch(timeout = 1000) {
    setTimeout(fetchLogsIncrements, timeout);
  }

  onMount(() => {
    window.addEventListener("keydown", keydown);
    window.addEventListener("beforeunload", (ev) => {
      // fetch("/kill", { method: "post" });
    });
    scheduleNextLogsIncrementalFetch();
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
    selectedTabIdx = len(tabs) - 1;
    return tab;
  }

  const valuePrefix = ":v ";

  /**
   * s looks like:
   * v: <name> <type> <value>
   * @param {string} sIn
   */
  function parseValue(sIn) {
    // console.log("parseValue:", sIn);
    let s = sIn.substring(len(valuePrefix));
    let idx = s.indexOf(" ");
    if (idx < 0) {
      console.log(`parseValue: invalid sIn='${sIn}'`);
      return null;
    }
    let name = s.substring(0, idx);
    s = s.substring(idx + 1);
    idx = s.indexOf(" ");
    let typ = s.substring(0, idx);
    s = s.substring(idx + 1);
    let v = s;
    return [name, typ, v];
  }

  /**
   * @param {number} no
   * @param {string} line
   */
  function plog(no, line) {
    line = line.trimEnd();
    let tab = findOrCreateTab(no);
    if (line.startsWith(valuePrefix)) {
      let e = parseValue(line);
      if (e === null) {
        return;
      }
      console.log("value:", e);
      let a = tab.values;
      let n = len(a);
      for (let i = 0; i < n; i++) {
        // update value in place
        if (a[i][0] == e[0]) {
          // TODO: will this trigger re-render ?
          a[i] = e;
          return;
        }
      }
      tab.values.push(e);
      return;
    }
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
   * @param {number} tabIdx
   */
  function filterLogs(filterLC, tabIdx) {
    if (tabIdx < 0) {
      return [];
    }
    let logs = tabs[tabIdx].logs;
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

  function getValues(tabIdx) {
    if (tabIdx < 0) {
      return [];
    }
    return tabs[tabIdx].values;
  }

  function len(o) {
    return o ? o.length : 0;
  }
  function clearLogs() {
    if (selectedTabIdx >= 0) {
      tabs[selectedTabIdx].logs = [];
    }
  }

  /**
   * @param {number} tabIdx
   */
  function selectTab(tabIdx) {
    selectedTabIdx = tabIdx;
  }

  function aboutClicked() {
    let uri = "https://www.sumatrapdfreader.org/docs/Logview";
    // @ts-ignore
    window.runtime?.BrowserOpenURL(uri);
  }
  // @ts-ignore
  //window.runtime.EventsOn("plog", plog);
  function getKeyFilteredLogs(item, _) {
    // console.log("getKeyFilteredLogs:", item, i);
    return item;
    // return filteredLogs[i];
  }
  function getLogsCount() {
    if (selectedTabIdx < 0) {
      return 0;
    }
    return len(tabs[selectedTabIdx].logs);
  }
</script>

{#if len(currentTabValues) > 0}
  <div class="values">
    {#each currentTabValues as v, idx (idx)}
      {@const name = v[0]}
      {@const val = v[2]}
      <div class="val-item">
        <div>{name}</div>
        <div>{val}</div>
      </div>
    {/each}
  </div>
{/if}

<main class="flex flex-col">
  <div class="top">
    <div></div>
    <div class="relative">
      <input
        type="text"
        placeholder="filter '/'"
        bind:this={searchEl}
        bind:value={filter}
        class="py-1 px-2 bg-white w-full mb-1 rounded-xs"
        style="min-width: 70vw"
      />
      <div class="absolute right-[0.5rem] top-[0.25rem] italic text-gray-400">
        {itemsCountMsg}
      </div>
    </div>

    <div class="mr-2 menu-trigger dropdown">
      ☰
      <div class="dropdown-content">
        <button class="btn-pause text-xs" onclick={togglePauseScrolling}
          >{btnText}</button
        >
        <button onclick={clearLogs} class="text-xs">clear</button>
        <button class="hidden2 text-xs" onclick={() => genLotsOfLogs()}
          >gen test data</button
        >
        <button class="mr-1 text-xs" onclick={aboutClicked}>about</button>
      </div>
    </div>
  </div>

  <div class="tabs">
    {#each tabs as tab, idx}
      {#if idx === selectedTabIdx}
        <button class="tab tab-selected"
          >{tab.tabName} <span class="kbd">[{idx + 1}]</span>
          <!-- svelte-ignore a11y_click_events_have_key_events -->
          <!-- svelte-ignore a11y_no_static_element_interactions -->
          <span onclick={() => closeTab(idx)} class="tab-close">×</span>
        </button>
      {:else}
        <button onclick={() => selectTab(idx)} class="tab"
          >{tab.tabName} <span class="kbd">[{idx + 1}]</span>
          <!-- svelte-ignore a11y_click_events_have_key_events -->
          <!-- svelte-ignore a11y_no_static_element_interactions -->
          <span onclick={() => closeTab(idx)} class="tab-close">×</span>
        </button>
      {/if}
    {/each}
  </div>

  {#if selectedTabIdx < 0}
    <div class="grow bg-white">
      <div class="no-results">No logs yet</div>
    </div>
  {:else if len(filteredLogs) == 0}
    {#if getLogsCount() == 0}
      <div class="grow bg-white">
        <div class="no-results">No logs yet</div>
      </div>
    {:else}
      <div class="grow bg-white">
        <div class="no-results">No results matching '<b>{filter}</b>'</div>
      </div>
    {/if}
  {:else}
    <VList
      bind:this={vlist}
      style="flex-grow: 1; font-family: monospace; background-color: white;"
      data={filteredLogs}
      getKey={getKeyFilteredLogs}
    >
      {#snippet children(item, index)}
        {@const logs = tabs[selectedTabIdx].logs}
        {@const line = logs[item]}
        {#if filter === ""}
          <span class="log-line">{line}</span>
          <!-- <div class="log-line">{line}</div> -->
        {:else}
          {@const hili = hilightText(line, hiliRegExp)}
          <span class="log-line">{@html hili}</span>
          <!-- <div class="log-line">{@html hili}</div> -->
        {/if}
      {/snippet}
    </VList>
  {/if}
</main>

<style>
  .values {
    display: flex;
    flex-direction: column;
    position: fixed;
    padding: 2px 8px;
    top: 64px;
    right: 22px;
    min-width: 200px;
    /* min-height: 200px; */
    background-color: rgba(255, 255, 255, 200);
    z-index: 10;
    border: 1px solid #c0c0c0;
  }

  .val-item {
    display: flex;
    justify-content: space-between;
    &:hover {
      background-color: #e3e3e3;
    }
  }

  .menu-trigger {
    border: 0;
    font-size: 16px;
    padding: 1px 4px;
    cursor: pointer;
    &:hover {
      background-color: #e3e3e3;
    }
  }

  main {
    min-height: 0;
    height: 100vh;
    overflow: auto;
  }
  button {
    cursor: pointer;
  }

  .dropdown {
    position: relative;
    display: inline-block;
  }

  .dropdown-content {
    display: none;
    position: absolute;
    right: 0px;
    top: 0.75rem;
    background-color: #f0f0f0;
    padding: 4px 8px;
    min-width: 160px;
    z-index: 40;
    border: 1px solid #444;
    flex-direction: column;
    /* justify-items: stretch; */

    button {
      border: 0;
      width: 100%;
      &:hover {
        background-color: #ddd;
      }
    }
  }

  .dropdown:hover .dropdown-content {
    display: flex;
  }

  .top {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    column-gap: 0.5rem;
    padding-top: 4px;
    padding-bottom: 6px;
  }

  .tabs {
    display: flex;
  }

  .kbd {
    color: gray;
    margin-left: 0.25rem;
  }

  .tab {
    display: flex;
    align-items: center;
    border: 0;
    padding: 4px 0.5rem;
    cursor: pointer;
    &:hover {
      background-color: rgba(128, 128, 128, 0.2);
    }
  }
  .tab-selected {
    background-color: white;
    cursor: default;
    &:hover {
      background-color: white;
    }
  }
  .tab-close {
    padding: 1px 4px;
    &:hover {
      background-color: rgba(128, 128, 128, 0.4);
    }
  }

  .log-line {
    white-space: pre-wrap;
    word-break: break-all;
    padding: 0px 0.5rem;
    &:hover {
      background-color: lightgray;
    }
  }
  .no-results {
    text-align: center;
    font-size: 120%;
    margin-top: 30%; /* from eyeballing */
  }
  .btn-pause {
    min-width: 8rem;
  }

  :global(.hili) {
    background-color: yellow;
  }
</style>
