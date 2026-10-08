// ng: spliced into ext/gpui/web/shell.html by cmd/helper/compile.ts, ahead of
// the module script. It loads /settings and /uploads from OPFS into MEMFS and
// holds main() back (addRunDependency) until that finishes, so GlobalPrefs
// sees the settings saved on an earlier visit. C++ keeps using synchronous
// MEMFS calls; writes are copied back to OPFS. The sync OPFS API only exists
// in a worker, and this page runs on the main thread.
//
// Everything else the app reads (the fonts, the sample documents) is in the
// preloaded MEMFS image.
//
// This runs as a page script, outside the module's own scope, so FS and the
// run-dependency pair are reached through Module. window.__sumatraStorage
// says how the load went.
(function () {
  var Module = (window.Module = window.Module || {});
  // OPFS directories at the origin root. Same names as the MEMFS mounts, so
  // another page on this origin can open the same files.
  var roots = ["/uploads", "/settings"];
  // path -> "size:mtime" for files already stored in OPFS. Unchanged files
  // are not rewritten; a settings save must not copy every PDF again.
  var stamp = {};
  var saving = false;
  var queued = [];

  function fs() {
    return Module.FS;
  }

  function ensureDir(path) {
    var sofar = "";
    var parts = path.split("/");
    for (var i = 0; i < parts.length; i++) {
      if (!parts[i]) continue;
      sofar += "/" + parts[i];
      try {
        fs().mkdir(sofar);
      } catch (e) {
        // already there, or a later write will report the real error
      }
    }
  }

  function stampOf(path) {
    var st = fs().stat(path);
    var mtime = st.mtime ? st.mtime.getTime() : 0;
    return st.size + ":" + mtime;
  }

  function remember(path) {
    var st;
    try {
      st = fs().stat(path);
    } catch (e) {
      return;
    }
    if (fs().isDir(st.mode)) {
      var names = fs().readdir(path);
      for (var i = 0; i < names.length; i++) {
        if (names[i] === "." || names[i] === "..") continue;
        remember(path + "/" + names[i]);
      }
      return;
    }
    stamp[path] = stampOf(path);
  }

  function isDirMode(mode) {
    return (mode & 61440) === 16384;
  }

  // emscripten's old IDBFS: database named after the mount, store FILE_DATA,
  // key is the absolute path, value is { mode, timestamp, contents }.
  function readLegacyDb(name) {
    return new Promise(function (resolve) {
      var created = false;
      var settled = false;
      function finish(rows) {
        if (settled) return;
        settled = true;
        resolve(rows);
      }
      var req;
      try {
        req = indexedDB.open(name);
      } catch (e) {
        finish([]);
        return;
      }
      req.onupgradeneeded = function () {
        created = true;
        try {
          req.transaction.abort();
        } catch (e) {
          // aborting the creation transaction also errors the request
        }
      };
      req.onerror = function () {
        finish([]);
      };
      req.onsuccess = function () {
        var db = req.result;
        if (created || !db.objectStoreNames.contains("FILE_DATA")) {
          db.close();
          if (created) indexedDB.deleteDatabase(name);
          finish([]);
          return;
        }
        var rows = [];
        var tx = db.transaction("FILE_DATA", "readonly");
        var cur = tx.objectStore("FILE_DATA").openCursor();
        cur.onerror = function () {
          db.close();
          finish(rows);
        };
        cur.onsuccess = function () {
          var c = cur.result;
          if (!c) return;
          rows.push({ path: String(c.key), value: c.value });
          c.continue();
        };
        tx.oncomplete = function () {
          db.close();
          finish(rows);
        };
        tx.onerror = function () {
          db.close();
          finish(rows);
        };
      };
    });
  }

  function importLegacy(memPath, rows) {
    var n = 0;
    var prefix = memPath + "/";
    for (var i = 0; i < rows.length; i++) {
      var path = rows[i].path;
      var value = rows[i].value || {};
      if (path === memPath) continue;
      if (path.indexOf(prefix) !== 0) continue;
      if (isDirMode(value.mode)) {
        ensureDir(path);
        n++;
        continue;
      }
      if (!value.contents) continue;
      ensureDir(path.slice(0, path.lastIndexOf("/")));
      fs().writeFile(path, new Uint8Array(value.contents));
      n++;
    }
    return n;
  }

  async function loadDir(dirHandle, memPath) {
    var n = 0;
    ensureDir(memPath);
    for await (var entry of dirHandle.entries()) {
      var name = entry[0];
      var handle = entry[1];
      var path = memPath + "/" + name;
      if (handle.kind === "directory") {
        n += await loadDir(handle, path);
      } else {
        var file = await handle.getFile();
        var bytes = new Uint8Array(await file.arrayBuffer());
        fs().writeFile(path, bytes);
        n++;
      }
    }
    return n;
  }

  async function opfsDir(memPath, create) {
    var root = await navigator.storage.getDirectory();
    return root.getDirectoryHandle(memPath.slice(1), { create: create });
  }

  async function reconcile(memPath, dirHandle) {
    var live = {};
    var names = [];
    try {
      names = fs().readdir(memPath);
    } catch (e) {
      names = [];
    }
    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      if (name === "." || name === "..") continue;
      live[name] = true;
      var path = memPath + "/" + name;
      var st = fs().stat(path);
      if (fs().isDir(st.mode)) {
        var sub = await dirHandle.getDirectoryHandle(name, { create: true });
        await reconcile(path, sub);
        continue;
      }
      var key = stampOf(path);
      if (stamp[path] === key) continue;
      var fh = await dirHandle.getFileHandle(name, { create: true });
      var writable = await fh.createWritable();
      try {
        // copy out of the wasm heap before the await
        await writable.write(new Uint8Array(fs().readFile(path)));
      } finally {
        await writable.close();
      }
      stamp[path] = key;
    }
    var doomed = [];
    for await (var entry of dirHandle.entries()) {
      if (!live[entry[0]]) doomed.push(entry[0]);
    }
    for (var j = 0; j < doomed.length; j++) {
      try {
        await dirHandle.removeEntry(doomed[j], { recursive: true });
      } catch (e) {
        if (!e || e.name !== "NotFoundError") throw e;
      }
      forget(memPath + "/" + doomed[j]);
    }
  }

  function forget(path) {
    var prefix = path + "/";
    for (var key in stamp) {
      if (key === path || key.indexOf(prefix) === 0) delete stamp[key];
    }
  }

  async function saveTrees() {
    for (var i = 0; i < roots.length; i++) {
      var handle = await opfsDir(roots[i], true);
      await reconcile(roots[i], handle);
    }
  }

  function pump() {
    if (saving || queued.length === 0) return;
    var batch = queued;
    queued = [];
    saving = true;
    saveTrees().then(
      function () {
        saving = false;
        for (var i = 0; i < batch.length; i++) batch[i](null);
        pump();
      },
      function (err) {
        saving = false;
        for (var i = 0; i < batch.length; i++) batch[i](err);
        pump();
      },
    );
  }

  Module.sumatraStorageSave = function (done) {
    queued.push(done || function () {});
    pump();
  };

  Module.sumatraStorageLoad = function (done) {
    if (!navigator.storage || !navigator.storage.getDirectory) {
      done("OPFS is not available");
      return;
    }
    (async function () {
      var imported = [];
      for (var i = 0; i < roots.length; i++) {
        var memPath = roots[i];
        var handle = await opfsDir(memPath, true);
        var n = await loadDir(handle, memPath);
        if (n > 0) continue;
        var rows = await readLegacyDb(memPath);
        if (importLegacy(memPath, rows) > 0) imported.push(memPath);
      }
      if (imported.length > 0) {
        await saveTrees();
        for (var j = 0; j < imported.length; j++) indexedDB.deleteDatabase(imported[j]);
      }
      stamp = {};
      for (var k = 0; k < roots.length; k++) remember(roots[k]);
    })().then(
      function () {
        done(null);
      },
      function (err) {
        done(err);
      },
    );
  };

  Module.preRun = Module.preRun || [];
  Module.preRun.push(function () {
    try {
      fs().mkdir("/settings");
      fs().mkdir("/uploads");
    } catch (e) {
      window.__sumatraStorage = "mount failed: " + e;
      console.error("creating storage directories failed", e);
      return;
    }
    Module.addRunDependency("opfs-data");
    Module.sumatraStorageLoad(function (err) {
      window.__sumatraStorage = err ? "load failed: " + err : "ready";
      if (err) console.error("loading OPFS directories failed", err);
      Module.removeRunDependency("opfs-data");
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
