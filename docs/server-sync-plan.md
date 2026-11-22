# SumatraPDF Web: matching local files by content

Temporary plan. When done, the design moves to AGENTS.md and this file goes.

## Today

Client (orig and ng, `src/shared/ServerFiles.cpp`, `SumatraPDFWebCommon.cpp`):

- settings: `OnlineApiKey`, `OnlineServer`
- memory: server names + sizes from `GET /api/files`
- match: same base name; same size = "Server", else "Server ≠"
- upload: `UploadJob` hashes the bytes, tries `POST /api/files?name=&sha1=`
  without a body (link), then with the body
- server-only files: stand-in `FileState` at `<appdata>/SumatraPDF-web/<name>`

Server (`../hack/webapps/sumatra-web/server/store_files.go`): content stored
once as `blobs/<sha1>`; `/api/files` lists `name, size, uploadedAt, sha1`.

## Design

```
GET /api/files ──> server list (name, size, sha1, uploadedAt) ──> disk cache
                        │
history ──> candidates (size equals a server size, no valid hash)
                        │
                 hash worker (background, per-share timeouts)
                        │
             FileState.ContentSha1 ──> match by sha1 ──> badges / menu
```

### Data

- `OnlineListedFile`: `name, size, sha1 (hex), uploadedAt`.
- `FileState.ContentSha1` (new, both generators): `<40 hex> <size> <mtime>`,
  mtime = `FILETIME` as u64. Valid only while the file's size and mtime match.
- In memory, per path: `Unknown`, `Pending`, `Hashed`, `SizeNotOnServer`,
  `Unreachable`.
- Disk cache of the last list: `<appdata>/SumatraPDF-web-files.txt`, first line
  `<origin> <user id from the key>`, then the `/api/files` JSON. Shown until a
  fresh list arrives; ignored for another account or server.

### Hashing

- Streaming SHA-1 in `base/Crypto` (CryptoAPI, CommonCrypto, own on Linux).
- Hash on upload (already computed by `UploadJob`, now returned) and on download
  (the server's sha1); stored in `ContentSha1`.
- After a list: history entries not known this session are queued. The worker
  stats each one; a size that no server file has needs no hash. A stored hash
  with matching size and mtime is reused. Otherwise it reads in 1 MiB chunks.
- One coordinator thread, no I/O of its own; one I/O thread per file. No
  progress for 5 s: cancel the I/O (`CancelSynchronousIo` on Windows), abandon
  the thread, mark the share (`X:`, `\\server\share`, first two posix path
  parts) unreachable for 10 minutes. Quit never waits for it.
- Results go to the UI thread: store `ContentSha1`, save settings, redraw.
- Wasm: no hashing; documents in `/uploads` already carry server names.

### Matching

| local | result |
|---|---|
| hash equals a server sha1 | Server (that server file; its name may differ) |
| same name, different hash | Server ≠ |
| hash unknown | name + size, as today |

- Server-only: server files no local file matches.
- Upload: same name with different content asks first (as today). Delete from
  server deletes the matched server name.
- The home page lists again when the list is older than 5 minutes.

## Phases (each committed, orig and ng)

1. Server sha1 in the list; `ContentSha1`; hash stored on upload / download.
2. Streaming SHA-1 and the background hash worker.
3. Match by hash: badges, server-only list, delete target.
4. Disk cache of the list; re-list after 5 minutes.
