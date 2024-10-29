<script>
  import { run, preventDefault } from "svelte/legacy";

  import { version } from "./version";

  let idx = 2;
  /** @type {[string, number][]} */
  let logs = $state([]);
  let filteredLogs = $state([]);
  let autoScrollPaused = false;
  let btnText = $state("pause scrolling");
  let searchTerm = $state("");
  let searchTermLC = "";
  /** @type {HTMLElement} */
  let logAreaEl;

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
   * @param {string} s
   * @param {number} no
   */
  function plog(s, no) {
    // a single write can contain multiple lines
    s = s.trimEnd();
    let lines = s.split("\n");
    let didMatch = false;
    console.log(`lines: ${lines.length}`);
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
   * @param {string} searchTerm
   */
  function filterLogs(searchTerm) {
    searchTerm = searchTerm.trim();
    searchTermLC = searchTerm.toLowerCase();
    if (searchTermLC === "") {
      filteredLogs = logs;
      return;
    }
    let res = [];
    for (let el of logs) {
      if (matches(el[0], searchTermLC)) {
        res.push(el);
      }
    }
    filteredLogs = res;
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
  window.runtime.WindowSetTitle(windowTitle);

  function pauseClicked() {
    autoScrollPaused = !autoScrollPaused;
    if (autoScrollPaused) {
      btnText = "unpause scrolling";
    } else {
      btnText = "pause scrolling";
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
    window.runtime.BrowserOpenURL(uri);
  }
  // @ts-ignore
  window.runtime.EventsOn("plog", plog);
  run(() => {
    filterLogs(searchTerm);
  });
</script>

<svelte:window title={windowTitle} />
<main>
  <div class="top">
    <div style="flex-grow: 1"></div>
    <input type="text" placeholder="search term..." bind:value={searchTerm} />
    <button class="btn-pause" onclick={pauseClicked}>{btnText}</button>
    <button onclick={clearLogs}>clear</button>
    <div>{len(logs)} line, {len(filteredLogs)} shown</div>
    <div style="flex-grow: 1"></div>
    <button onclick={aboutClicked}>about</button>
  </div>
  <div bind:this={logAreaEl} class="log-area">
    {#if len(filteredLogs) == 0}
      <div class="no-results">No results matching '<b>{searchTerm}</b>'</div>
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
