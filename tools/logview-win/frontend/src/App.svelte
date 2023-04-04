<script>
  let logs = ["Logview SumatraPDF"];
  function plog(s, no) {
    console.log("plog:", s);
    logs.push(s);
    logs = logs;
  }
  // @ts-ignore
  window.runtime.EventsOn("plog", plog);
</script>

<main>
  {#each logs as log}
    <span>{log}</span><br />
  {/each}
</main>

<style>
  main {
    padding: 0.5rem 0.5rem;
  }
  span {
    font-family: monospace;
  }
  pre {
    margin: 0;
    word-wrap: break-word;
  }
</style>
