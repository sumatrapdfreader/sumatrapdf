// ng: spliced into ext/gpui/web/shell.html by cmd/helper/compile.ts, ahead of
// the module script. It mounts settings and uploads on IndexedDB and holds
// main() back (addRunDependency) until what was stored is in the file system,
// so GlobalPrefs loads the settings the user saved in an earlier visit.
// Everything else the app reads (the fonts, the sample documents) is in the
// preloaded MEMFS image.
//
// This runs as a page script, outside the module's own scope, so FS, IDBFS
// and the run-dependency pair are reached through Module; the link exports
// them for that. window.__sumatraIdbfs says how it went.
(function () {
  var Module = (window.Module = window.Module || {});
  Module.preRun = Module.preRun || [];
  Module.preRun.push(function () {
    try {
      Module.FS.mkdir("/settings");
      Module.FS.mkdir("/uploads");
      Module.FS.mount(Module.IDBFS, {}, "/settings");
      Module.FS.mount(Module.IDBFS, {}, "/uploads");
    } catch (e) {
      window.__sumatraIdbfs = "mount failed: " + e;
      console.error("mounting IndexedDB directories failed", e);
      return;
    }
    Module.addRunDependency("idbfs-data");
    Module.FS.syncfs(true, function (err) {
      window.__sumatraIdbfs = err ? "load failed: " + err : "ready";
      if (err) {
        console.error("loading IndexedDB directories failed", err);
      }
      Module.removeRunDependency("idbfs-data");
    });
  });

  // Ctrl+K focuses the address bar in Chrome and Firefox (Cmd+K on mac). gpui
  // leaves that chord with the browser, so the page takes it and asks the app
  // to open the command palette. The native menu is the same kind of clash
  // with the app's context menu, which opens on the right-button down.
  var mac =
    /Mac|iPhone|iPad|iPod/.test(navigator.platform || "") ||
    (navigator.userAgentData && navigator.userAgentData.platform === "macOS");
  document.addEventListener(
    "keydown",
    function (e) {
      if (e.altKey || e.shiftKey) return;
      if (e.code !== "KeyK") return;
      var chord = mac ? e.metaKey && !e.ctrlKey : e.ctrlKey && !e.metaKey;
      if (!chord) return;
      e.preventDefault();
      e.stopPropagation();
      if (e.repeat) return;
      var canvas = document.getElementById("gpui-canvas");
      if (canvas) canvas.focus();
      var open = Module._sumatra_wasm_command_palette;
      if (open) open();
    },
    true,
  );
  document.addEventListener(
    "contextmenu",
    function (e) {
      e.preventDefault();
    },
    true,
  );
})();
