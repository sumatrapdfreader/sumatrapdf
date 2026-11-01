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
  exit({ saved }) {
    location.href = "/";
    return true;
  },
};
```

Every hook is optional. Return a truthy value to say the host handled the call. If a hook is missing, returns a falsy value or throws, the app does what it does without a host.

| Hook                            | Called when                                 | Without a host                     |
| ------------------------------- | ------------------------------------------- | ---------------------------------- |
| `pickFile(done)`                | the app wants a file (`Open...`)            | browser file picker                |
| `download({name, path, bytes})` | the app saves a file for the user           | browser download                   |
| `openUrl({url})`                | the app opens a link                        | `window.open` in a new tab         |
| `exit({saved})`                 | the app quit and finished writing its state | nothing: the canvas stops updating |

- `pickFile`: call `done({ name, bytes })` with a `Uint8Array` or `ArrayBuffer`, or `done(null)` to cancel. The file is stored in `/uploads` under `name`, without any directory part, and opened. Only one pick runs at a time.
- `download`: `path` is the file's path in MEMFS. `bytes()` returns a copy of its content, so a hook that declines doesn't copy a large document.
- `exit`: `saved` is false if writing to OPFS failed. The app has already closed its windows, so the page can navigate away.

Hooks run on the main thread. A browser only allows some things during a user gesture, for example opening a file input, so do those synchronously in the hook.

`apiVersion` is 1. The app logs a warning when it differs. Change it when a change here breaks hosts.
