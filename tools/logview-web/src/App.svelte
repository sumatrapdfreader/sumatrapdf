<script>
  import { run } from "svelte/legacy";

  import { version } from "./version";
  import { onMount } from "svelte";

  let idx = 2;
  /** @type {[string, number][]} */
  let logs = $state([]);
  let autoScrollPaused = false;
  let btnText = $state("pause scrolling");
  let searchTerm = $state("");
  let searchTermLC = $derived(searchTerm.trim().toLowerCase());
  let filteredLogs = $derived(filterLogs(logs, searchTermLC));
  /** @type {HTMLElement} */
  let logAreaEl;
  /** @type {HTMLElement} */
  let searchEl;

  /**
   * @param {string} s
   * @param {string} searchTerm
   */
  function matches(s, searchTerm) {
    if (searchTerm === "") {
      return false;
    }
    s = s.toLowerCase();
    return s.includes(searchTerm);
  }

  /**
   * @param {KeyboardEvent} ev
   */
  function keydown(ev) {
    if (ev.key === "/") {
      searchEl.focus();
      ev.preventDefault();
      return;
    }
    let isSearchFocused = document.activeElement === searchEl;
    if (isSearchFocused) {
      if (ev.key === "Escape") {
        // if searchEl is focused, return focus to logAreaEl
        logAreaEl.focus();
        searchTerm = "";
        return;
      }
    } else {
      if (ev.key === "p") {
        togglePauseScrolling();
        ev.preventDefault();
        return;
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
   * @param {number} no
   * @param {string} line
   */
  function plog(no, line) {
    line = line.trim();
    let lines = [line];
    let didMatch = false;
    for (let l of lines) {
      /** @type {[string, number]}*/
      let el = [l, idx];
      idx = idx + 1;
      logs.push(el);
      if (matches(l, searchTermLC)) {
        filteredLogs.push(el);
        didMatch = true;
      }
    }

    if (searchTermLC === "") {
      filteredLogs = logs;
      if (!autoScrollPaused) {
        scrollToBottom(logAreaEl);
      }
      return;
    }

    if (didMatch) {
      filteredLogs = filteredLogs;
      if (!autoScrollPaused) {
        scrollToBottom(logAreaEl);
      }
    }
  }

  /**
   * @param {[string, number][]} logs
   * @param {string} searchTermLC
   */
  function filterLogs(logs, searchTermLC) {
    if (searchTermLC === "") {
      return logs;
    }
    let res = [];
    for (let el of logs) {
      if (matches(el[0], searchTermLC)) {
        res.push(el);
      }
    }
    return res;
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
    logs = [];
    filteredLogs = [];
  }

  function aboutClicked() {
    let uri = "https://www.sumatrapdfreader.org/docs/Logview";
    // @ts-ignore
    window.runtime?.BrowserOpenURL(uri);
  }
  // @ts-ignore
  //window.runtime.EventsOn("plog", plog);
</script>

<main>
  <div class="top">
    <div style="flex-grow: 1"></div>
    <input
      type="text"
      placeholder="filter /"
      bind:this={searchEl}
      bind:value={searchTerm}
    />
    <button class="btn-pause" onclick={togglePauseScrolling}>{btnText}</button>
    <button onclick={clearLogs}>clear</button>
    <div>{len(logs)} line, {len(filteredLogs)} shown</div>
    <div style="flex-grow: 1"></div>
    <button onclick={aboutClicked}>about</button>
  </div>
  <div bind:this={logAreaEl} tabindex="0" role="listbox" class="log-area">
    {#if len(filteredLogs) == 0}
      {#if len(logs) == 0}
        <div class="no-results">No logs yet</div>
      {:else}
        <div class="no-results">No results matching '<b>{searchTerm}</b>'</div>
      {/if}
    {:else}
      {#each filteredLogs as log (log[1])}
        <span class="log-line">{log[0]}</span><br />
      {/each}
    {/if}
  </div>
</main>

<style>
  main {
    display: flex;
    flex-direction: column;
    height: calc(100vh - 1rem);
    padding: 0.5rem 0.5rem;
    overflow: auto;
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

  .top {
    display: flex;
    justify-content: center;
    align-items: baseline;
    column-gap: 0.5rem;
  }

  .log-area {
    overflow: auto;
    margin-top: 0.5rem;
    /* background: rgb(239, 250, 254); */
    height: 100%;
    background-color: rgb(255, 255, 255);
  }
  .log-line {
    font-family: monospace;
    content-visibility: auto;
    /* contain-intrinsic-size: 1rem; */
  }
</style>
