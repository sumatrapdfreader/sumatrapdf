// ng: spliced into ext/gpui/web/shell.html by cmd/helper/compile.ts, ahead of
// the module script. It loads /settings from OPFS into MEMFS, lists /uploads
// (documents are read when first opened, see addLazyFile) and holds main()
// back (addRunDependency) until that finishes, so GlobalPrefs sees the
// settings saved on an earlier visit. C++ keeps using synchronous
// MEMFS calls; writes are copied back to OPFS. The sync OPFS API only exists
// in a worker, and this page runs on the main thread.
//
// Sample documents are in the preloaded MEMFS image. Built-in fonts that were
// not packed in the archive are cached under /fonts, also mirrored to OPFS.
//
// This runs as a page script, outside the module's own scope, so FS and the
// run-dependency pair are reached through Module. window.__sumatraStorage
// says how the load went.
(function () {
  var Module = (window.Module = window.Module || {});
  // OPFS directories at the origin root. Same names as the MEMFS mounts, so
  // another page on this origin can open the same files.
  var roots = ["/uploads", "/settings", "/fonts"];
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

  // Documents aren't read at startup, only listed: a file's bytes are read the
  // first time something reads it (MEMFS gets them through node.contents), so
  // startup time and memory don't grow with the library. A synchronous XHR of a
  // blob URL is the one synchronous way to read a File on the main thread.
  // path -> true for documents whose bytes were read, for tests
  var lazyLoaded = (Module.sumatraLazyLoaded = {});
  // path -> true when reading failed: the empty contents must not be saved
  // over the stored file
  var lazyFailed = {};

  function readFileSync(path, file) {
    var url = URL.createObjectURL(file);
    try {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url, false);
      xhr.overrideMimeType("text/plain; charset=x-user-defined");
      xhr.send(null);
      var text = xhr.responseText;
      if (text.length !== file.size) throw new Error("read " + text.length + " of " + file.size + " bytes");
      var bytes = new Uint8Array(text.length);
      for (var i = 0; i < text.length; i++) bytes[i] = text.charCodeAt(i) & 255;
      return bytes;
    } finally {
      URL.revokeObjectURL(url);
    }
  }

  function addLazyFile(path, file) {
    fs().writeFile(path, new Uint8Array(0));
    var node = fs().lookupPath(path).node;
    var data = null;
    Object.defineProperty(node, "contents", {
      configurable: true,
      enumerable: true,
      get: function () {
        if (data !== null) return data;
        try {
          data = readFileSync(path, file);
          lazyLoaded[path] = true;
        } catch (e) {
          // e.g. another tab replaced the file since this one listed it
          console.error("reading " + path + " from OPFS failed", e);
          lazyFailed[path] = true;
          data = new Uint8Array(0);
          node.usedBytes = 0;
        }
        return data;
      },
      set: function (v) {
        data = v;
        delete lazyFailed[path];
      },
    });
    node.usedBytes = file.size;
    node.atime = node.mtime = node.ctime = file.lastModified;
  }

  async function loadDir(dirHandle, memPath, lazy) {
    var n = 0;
    ensureDir(memPath);
    for await (var entry of dirHandle.entries()) {
      var name = entry[0];
      var handle = entry[1];
      var path = memPath + "/" + name;
      if (handle.kind === "directory") {
        n += await loadDir(handle, path, lazy);
        continue;
      }
      var file = await handle.getFile();
      if (lazy) {
        addLazyFile(path, file);
      } else {
        fs().writeFile(path, new Uint8Array(await file.arrayBuffer()));
      }
      n++;
    }
    return n;
  }

  async function opfsDir(memPath, create) {
    var root = await navigator.storage.getDirectory();
    return root.getDirectoryHandle(memPath.slice(1), { create: create });
  }

  // Safari before 26 has no createWritable: there an OPFS file can only be
  // written from a worker, with a sync access handle (Safari 15.2+)
  var kWriterSource =
    "onmessage = async function (e) {\n" +
    "  var m = e.data;\n" +
    "  try {\n" +
    "    var dir = await navigator.storage.getDirectory();\n" +
    "    for (var i = 0; i < m.dirs.length; i++) dir = await dir.getDirectoryHandle(m.dirs[i], { create: true });\n" +
    "    var fh = await dir.getFileHandle(m.name, { create: true });\n" +
    "    var h = await fh.createSyncAccessHandle();\n" +
    "    try { h.truncate(0); h.write(m.bytes, { at: 0 }); h.flush(); } finally { h.close(); }\n" +
    "    postMessage({ id: m.id });\n" +
    "  } catch (err) {\n" +
    "    postMessage({ id: m.id, error: String((err && err.message) || err) });\n" +
    "  }\n" +
    "};\n";
  var writer = null;
  var writerCalls = {};
  var writerNextId = 1;

  function writeInWorker(path, data) {
    if (!writer) {
      var url = URL.createObjectURL(new Blob([kWriterSource], { type: "text/javascript" }));
      writer = new Worker(url);
      writer.onmessage = function (e) {
        var call = writerCalls[e.data.id];
        delete writerCalls[e.data.id];
        if (!call) return;
        if (e.data.error) call.reject(new Error(e.data.error));
        else call.resolve();
      };
    }
    var parts = path.slice(1).split("/");
    var id = writerNextId++;
    return new Promise(function (resolve, reject) {
      writerCalls[id] = { resolve: resolve, reject: reject };
      writer.postMessage({ id: id, dirs: parts.slice(0, -1), name: parts[parts.length - 1], bytes: data });
    });
  }

  async function writeOpfsFile(fh, path, data) {
    if (!fh.createWritable) {
      await writeInWorker(path, data);
      return;
    }
    var writable = await fh.createWritable();
    try {
      await writable.write(data);
    } finally {
      await writable.close();
    }
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
      if (stamp[path] === key || lazyFailed[path]) continue;
      var fh = await dirHandle.getFileHandle(name, { create: true });
      // copy out of the wasm heap before the await
      var data = new Uint8Array(fs().readFile(path));
      await writeOpfsFile(fh, path, data);
      // A browser can take the write and keep nothing (WebKit's Windows port
      // keeps an empty file). Fail the save, so it's reported, rather than let
      // a document look saved.
      var kept = (await fh.getFile()).size;
      if (kept !== data.length) {
        throw new Error("the browser kept " + kept + " of " + data.length + " bytes of " + path);
      }
      stamp[path] = key;
    }
    // Only what this tab loaded or wrote and has since deleted. Another tab, or
    // the site's own pages, may have added entries this tab never saw.
    var doomed = [];
    for await (var entry of dirHandle.entries()) {
      if (live[entry[0]]) continue;
      if (knew(memPath + "/" + entry[0])) doomed.push(entry[0]);
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

  function knew(path) {
    if (stamp[path] !== undefined) return true;
    var prefix = path + "/";
    for (var key in stamp) {
      if (key.indexOf(prefix) === 0) return true;
    }
    return false;
  }

  // Other tabs of this origin, and the site's pages, take the same lock
  // (navigator.locks is per origin) before they change these directories.
  function withLock(fn) {
    if (!navigator.locks) return fn();
    return navigator.locks.request("sumatra-opfs", fn);
  }

  async function saveTrees() {
    for (var i = 0; i < roots.length; i++) {
      var handle = await opfsDir(roots[i], true);
      await reconcile(roots[i], handle);
    }
  }

  function saveTreesLocked() {
    return withLock(saveTrees);
  }

  // Set when OPFS didn't load. Saving then would write over the documents and
  // settings that are still there with whatever this tab has.
  var loadError = "";

  // The first failure is reported to the app; later ones wait for a save to
  // work again, so a broken storage doesn't put up a notification per save.
  var reportedError = false;
  function reportStorageError(err) {
    if (reportedError) return;
    reportedError = true;
    Module.__storageError = String((err && err.message) || err);
    var report = Module._sumatra_wasm_storage_error;
    if (report) report();
  }

  function pump() {
    if (saving || queued.length === 0) return;
    var batch = queued;
    queued = [];
    saving = true;
    saveTreesLocked().then(
      function () {
        saving = false;
        reportedError = false;
        for (var i = 0; i < batch.length; i++) batch[i](null);
        pump();
      },
      function (err) {
        saving = false;
        console.error("saving to OPFS failed", err);
        reportStorageError(err);
        for (var i = 0; i < batch.length; i++) batch[i](err);
        pump();
      },
    );
  }

  Module.sumatraStorageSave = function (done) {
    done = done || function () {};
    if (loadError) {
      done("storage didn't load: " + loadError);
      return;
    }
    queued.push(done);
    pump();
  };

  function saveAsync() {
    return new Promise(function (resolve) {
      Module.sumatraStorageSave(resolve);
    });
  }

  function sameBytes(a, b) {
    if (a.length !== b.length) return false;
    for (var i = 0; i < a.length; i++) {
      if (a[i] !== b[i]) return false;
    }
    return true;
  }

  // "report.pdf", 2 -> "report (2).pdf"
  function numberedName(name, n) {
    var dot = name.lastIndexOf(".");
    if (dot <= 0) return name + " (" + n + ")";
    return name.slice(0, dot) + " (" + n + ")" + name.slice(dot);
  }

  function memSize(path) {
    try {
      return fs().stat(path).size;
    } catch (e) {
      return -1;
    }
  }

  function memBytes(path) {
    try {
      return fs().readFile(path);
    } catch (e) {
      return null;
    }
  }

  async function opfsBytes(dirHandle, name) {
    if (!dirHandle) return null;
    try {
      var fh = await dirHandle.getFileHandle(name);
      var file = await fh.getFile();
      return new Uint8Array(await file.arrayBuffer());
    } catch (e) {
      return null;
    }
  }

  // Puts an uploaded or dropped file into dirPath (MEMFS, then OPFS) and
  // answers its path. A different file under the same name, in this tab or
  // stored by another one, gets a numbered name; the same bytes reuse it.
  var asked = false;
  Module.sumatraStoreUpload = async function (dirPath, name, data) {
    var bytes = new Uint8Array(data);
    name = String(name);
    name = name.substring(name.lastIndexOf("/") + 1) || "document";
    ensureDir(dirPath);
    var path = await withLock(async function () {
      var dirHandle = null;
      if (!loadError && navigator.storage && navigator.storage.getDirectory) {
        try {
          dirHandle = await opfsDir(dirPath, true);
        } catch (e) {
          dirHandle = null;
        }
      }
      for (var n = 1; ; n++) {
        var candidate = n === 1 ? name : numberedName(name, n);
        var p = dirPath + "/" + candidate;
        var size = memSize(p);
        // a stored document's bytes are read only when the sizes match
        if (size >= 0 && size !== bytes.length) continue;
        var inMem = size >= 0 ? memBytes(p) : null;
        var existing = inMem || (await opfsBytes(dirHandle, candidate));
        if (existing && !sameBytes(existing, bytes)) continue;
        if (!inMem) fs().writeFile(p, bytes);
        return p;
      }
    });
    var err = await saveAsync();
    if (err) {
      console.error("saving " + path + " to OPFS failed", err);
      reportStorageError(err);
    } else if (!asked && navigator.storage && navigator.storage.persist) {
      // documents are the user's; ask the browser not to evict them under
      // storage pressure. It may say no, which changes nothing.
      asked = true;
      navigator.storage.persist().catch(function () {});
    }
    return path;
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
        var n = await loadDir(handle, memPath, memPath === "/uploads");
        if (n > 0) continue;
        var rows = await readLegacyDb(memPath);
        if (importLegacy(memPath, rows) > 0) imported.push(memPath);
      }
      if (imported.length > 0) {
        await saveTreesLocked();
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

  // Asks the page embedding us (docs/wasm-host-api.md). True when the host took
  // the call; otherwise the caller does what it does without a host.
  var kHostApiVersion = 1;
  var hostChecked = false;
  Module.sumatraHostCall = function (name, arg) {
    var host = window.sumatraHost;
    if (!host) return false;
    if (!hostChecked) {
      hostChecked = true;
      if (host.apiVersion !== kHostApiVersion) {
        console.warn("sumatraHost.apiVersion is", host.apiVersion, "want", kHostApiVersion);
      }
    }
    if (typeof host[name] !== "function") return false;
    try {
      return !!host[name](arg);
    } catch (e) {
      console.error("sumatraHost." + name + " failed", e);
      return false;
    }
  };

  Module.preRun = Module.preRun || [];
  Module.preRun.push(function () {
    try {
      fs().mkdir("/settings");
      fs().mkdir("/uploads");
      fs().mkdir("/fonts");
    } catch (e) {
      window.__sumatraStorage = "mount failed: " + e;
      console.error("creating storage directories failed", e);
      return;
    }
    Module.addRunDependency("opfs-data");
    Module.sumatraStorageLoad(function (err) {
      window.__sumatraStorage = err ? "load failed: " + err : "ready";
      if (err) {
        loadError = String((err && err.message) || err);
        console.error("loading OPFS directories failed", err);
      }
      Module.removeRunDependency("opfs-data");
    });
  });

  // What didn't reach OPFS yet is lost with the tab. A hidden page may be
  // discarded without another event, and OPFS writes are async, so they
  // can't be counted on to finish in pagehide: save when the page is hidden.
  function flushNow() {
    if (Module.__exited || loadError) return;
    var flush = Module._sumatra_wasm_flush;
    if (!flush) return;
    flush();
    if (Module.__persistTimer) {
      clearTimeout(Module.__persistTimer);
      Module.__persistTimer = 0;
    }
    Module.sumatraStorageSave();
  }
  document.addEventListener("visibilitychange", function () {
    if (document.visibilityState === "hidden") flushNow();
  });
  window.addEventListener("pagehide", flushNow);
  // annotations live in the document's bytes, which only Save writes
  window.addEventListener("beforeunload", function (e) {
    if (Module.__exited) return;
    var has = Module._sumatra_wasm_has_unsaved;
    if (!has || !has()) return;
    e.preventDefault();
    e.returnValue = "";
  });

  // Files dropped on the page. gpui's own drop handler (on the canvas) passes
  // only the names, which aren't paths here, so it never sees the event.
  function hasFiles(dt) {
    var types = (dt && dt.types) || [];
    for (var i = 0; i < types.length; i++) {
      if (types[i] === "Files") return true;
    }
    return false;
  }
  document.addEventListener(
    "dragover",
    function (e) {
      if (!hasFiles(e.dataTransfer)) return;
      // without this the browser opens the file in place of the page
      e.preventDefault();
      e.dataTransfer.dropEffect = "copy";
    },
    false,
  );
  document.addEventListener(
    "drop",
    function (e) {
      var files = e.dataTransfer && e.dataTransfer.files;
      if (!files || files.length === 0) return;
      e.preventDefault();
      e.stopPropagation();
      // gpui saw the drag enter; tell it the drag is over
      var canvas = document.getElementById("gpui-canvas");
      if (canvas) canvas.dispatchEvent(new DragEvent("dragleave", { clientX: e.clientX, clientY: e.clientY }));
      var list = Array.prototype.slice.call(files);
      (async function () {
        var paths = [];
        for (var i = 0; i < list.length; i++) {
          try {
            var bytes = await list[i].arrayBuffer();
            paths.push(await Module.sumatraStoreUpload("/uploads", list[i].name, bytes));
          } catch (err) {
            console.error("reading the dropped file " + list[i].name + " failed", err);
          }
        }
        if (paths.length === 0) return;
        Module.__droppedPaths = paths;
        var open = Module._sumatra_wasm_open_dropped;
        if (open) open();
        focusCanvas();
      })();
    },
    true,
  );

  // gpui delivers keys only to #gpui-canvas, and its mousedown handler cancels
  // the click, which is what would have focused the canvas. The control inside
  // the frame still shows a caret and a ring; the key goes to the page.
  function focusCanvas() {
    var canvas = document.getElementById("gpui-canvas");
    if (!canvas || document.activeElement === canvas) return;
    canvas.focus();
  }
  Module.sumatraFocusCanvas = focusCanvas;
  document.addEventListener(
    "pointerdown",
    function (e) {
      var t = e.target;
      if (t && t.tagName === "INPUT") return;
      focusCanvas();
    },
    true,
  );
  var onRuntimeInitialized = Module.onRuntimeInitialized;
  Module.onRuntimeInitialized = function () {
    if (onRuntimeInitialized) onRuntimeInitialized();
    focusCanvas();
  };

  // gpui leaves every Ctrl/Cmd chord except A, C, X, Y, Z with the browser, and
  // F1-F12 (BrowserKeepsIt in gpui). So Ctrl+F would open the browser's search
  // and Ctrl+P print the page. For each such key the page asks the app whether
  // it's bound (custom shortcuts included) or a focused text field uses it, and
  // if so hands it to the app instead. The app's answer leaves out the keys
  // people need the browser to keep (IsBrowserReservedShortcut).
  var mac =
    /Mac|iPhone|iPad|iPod/.test(navigator.platform || "") ||
    (navigator.userAgentData && navigator.userAgentData.platform === "macOS");
  var kEditingChords = { 65: 1, 67: 1, 88: 1, 89: 1, 90: 1 };
  function browserGetsIt(e) {
    var vk = e.keyCode;
    var altGraph = e.ctrlKey && e.altKey && e.key && e.key.length === 1;
    if (altGraph) return false;
    if (e.ctrlKey || e.metaKey) return !kEditingChords[vk] || (e.shiftKey && vk === 67);
    return vk >= 112 && vk <= 123;
  }
  // on mac the app's Ctrl shortcuts are on Cmd, as gpui does for A/C/X/Y/Z
  function chordOf(e) {
    if (mac) return e.metaKey && !e.ctrlKey ? 1 : e.ctrlKey || e.metaKey ? -1 : 0;
    return e.ctrlKey && !e.metaKey ? 1 : e.metaKey ? -1 : 0;
  }
  var taken = {};
  document.addEventListener(
    "keydown",
    function (e) {
      if (!browserGetsIt(e)) return;
      var ctrl = chordOf(e);
      if (ctrl < 0) return;
      var wanted = Module._sumatra_wasm_key_wanted;
      var send = Module._sumatra_wasm_key;
      if (!wanted || !send) return;
      var ch = e.key && e.key.length === 1 ? e.key.codePointAt(0) : 0;
      if (!wanted(e.keyCode, ch, e.shiftKey ? 1 : 0, ctrl, e.altKey ? 1 : 0)) return;
      e.preventDefault();
      e.stopPropagation();
      taken[e.keyCode] = true;
      focusCanvas();
      send(e.keyCode, e.shiftKey ? 1 : 0, ctrl, e.altKey ? 1 : 0, 1, e.repeat ? 1 : 0);
    },
    true,
  );
  document.addEventListener(
    "keyup",
    function (e) {
      if (!taken[e.keyCode]) return;
      delete taken[e.keyCode];
      e.preventDefault();
      e.stopPropagation();
      var send = Module._sumatra_wasm_key;
      if (send) send(e.keyCode, e.shiftKey ? 1 : 0, chordOf(e) > 0 ? 1 : 0, e.altKey ? 1 : 0, 0, 0);
    },
    true,
  );
  // Other browsers report a trackpad pinch as Ctrl+wheel, which gpui cancels
  // and the app zooms the document on. Safari sends gesture events instead,
  // and zooms the whole page unless they're cancelled. Pass each change on as
  // that Ctrl+wheel; the app adds up the deltas of a gesture into its zoom.
  var gestureScale = 1;
  function onGesture(e) {
    e.preventDefault();
    if (e.type === "gesturestart") gestureScale = 1;
    if (e.type !== "gesturechange" || !e.scale) return;
    var deltaY = (gestureScale - e.scale) * 100;
    gestureScale = e.scale;
    var canvas = document.getElementById("gpui-canvas");
    if (!canvas || !deltaY) return;
    var wheel = {
      deltaY: deltaY,
      deltaMode: 0,
      ctrlKey: true,
      clientX: e.clientX,
      clientY: e.clientY,
      bubbles: true,
      cancelable: true,
    };
    canvas.dispatchEvent(new WheelEvent("wheel", wheel));
  }
  ["gesturestart", "gesturechange", "gestureend"].forEach(function (type) {
    document.addEventListener(type, onGesture, { passive: false });
  });

  // The browser leaves fullscreen on its own (Esc, or its "exit full screen"
  // UI) and doesn't pass that Esc to the page, or refuses to enter it (e.g.
  // the request came too long after the key press): tell the app, so it
  // leaves its fullscreen or presentation mode too.
  function fullscreenLeft() {
    if (document.fullscreenElement) return;
    var left = Module._sumatra_wasm_fullscreen_left;
    if (left) left();
  }
  document.addEventListener("fullscreenchange", fullscreenLeft);
  document.addEventListener("fullscreenerror", fullscreenLeft);

  // The native menu clashes with the app's context menu, which opens on the
  // right-button down.
  document.addEventListener(
    "contextmenu",
    function (e) {
      e.preventDefault();
    },
    true,
  );
})();
