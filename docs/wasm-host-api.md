# Hooks for pages embedding the wasm build

A page that hosts SumatraPDF ng's wasm build can take over some browser operations. Define `window.sumatraHost` before `SumatraPDF.js` loads:

```js
window.sumatraHost = {
  apiVersion: 1,
  pickFile(done) {
    showOurPicker((file) => done(file ? { name: file.name, bytes: file.bytes } : null));
    return true;
  },
  download({ name, path, bytes }) {
    saveToServer(name, bytes());
    return true;
  },
  openUrl({ url }) {
    window.open(url);
    return true;
  },
  fetchUrl({ url, name, done }) {
    fetchThroughOurServer(url).then(
      (bytes) => done({ name, bytes }),
      () => done(null),
    );
    return true;
  },
  exit({ saved }) {
    location.href = "/";
    return true;
  },
};
```

Every hook is optional. Return a truthy value to say the host handled the call. If a hook is missing, returns a falsy value or throws, the app does what it does without a host.

| Hook                            | Called when                                 | Without a host                 |
| ------------------------------- | ------------------------------------------- | ------------------------------ |
| `pickFile(done)`                | the app wants a file (`Open...`)            | browser file picker            |
| `download({name, path, bytes})` | the app saves a file for the user           | browser download               |
| `openUrl({url})`                | the app opens a link                        | `window.open` in a new tab     |
| `fetchUrl({url, name, done})`   | Open from URL... downloads a document       | `fetch(url)` in the page       |
| `exit({saved})`                 | the app quit and finished writing its state | a "SumatraPDF was closed" page |

- `pickFile`: call `done({ name, bytes })` with a `Uint8Array` or `ArrayBuffer`, or `done(null)` to cancel. The file is stored in `/uploads` under `name`, without any directory part, and opened. A different file already stored under that name is kept, and the new one gets a numbered name (`report (2).pdf`). Only one pick runs at a time.
- `fetchUrl`: `name` is the file name the app made from the URL. Call `done({ name, bytes })` like `pickFile`'s `done`: the file is stored in `/uploads` the same way, under a name that isn't taken, and opened. `done(null)` reports a failed download. The default `fetch` fails for a site that doesn't allow cross-origin requests; a host can fetch through its own server instead.
- `download`: `path` is the file's path in MEMFS. `bytes()` returns a copy of its content, so a hook that declines doesn't copy a large document.
- `exit`: `saved` is false if writing to OPFS failed. The app has already closed its windows, so the page can navigate away.

Hooks run on the main thread. A browser only allows some things during a user gesture, for example opening a file input, so do those synchronously in the hook.

`apiVersion` is 1. The app logs a warning when it differs. Change it when a change here breaks hosts.

## Storage shared with the page

Documents are in the OPFS directory `uploads`, settings in `settings` and downloaded fonts in `fonts`. A page on the same origin can read and write `uploads` too, and so can other tabs running the app. The app deletes only files it loaded or wrote itself, so a file the page adds while the app is open survives. Take the Web Lock `sumatra-opfs` (`navigator.locks.request("sumatra-opfs", ...)`) around writes, as the app does around its saves.

## Fonts

PDFs that don't embed their fonts use fonts the app downloads from `files.sumatrapdfreader.org` (the Cloudflare R2 bucket `files`), with a synchronous XHR. That needs a CORS rule on the bucket allowing `GET` from any origin. Without it such PDFs open with blank pages. The rule exists only in the Cloudflare dashboard; `tests/ad-hoc-wasm-commands.ts` checks it.

## Test hooks

With `?test-hooks` in the page URL, `Module._sumatra_test_visibility()`, `_sumatra_test_run(id)`, `_sumatra_test_ping()` and `_sumatra_test_pong()` let `tests/ad-hoc-wasm-commands.ts` list and run every command. Without the parameter they do nothing.
