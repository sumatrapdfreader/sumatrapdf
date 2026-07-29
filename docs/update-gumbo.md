# Updating Gumbo

Use `cmd/a-gumbo.ts` to update the amalgamated Gumbo copy used by the build.

1. Pick the upstream Gumbo repository URL and tag or commit hash.
2. Run:

   ```sh
   bun cmd/a-gumbo.ts <gumbo-repo-url> <tag-or-commit>
   ```

   Example:

   ```sh
   bun cmd/a-gumbo.ts https://github.com/ArtifexSoftware/thirdparty-gumbo-parser.git v0.10.1
   ```

3. The script checks out the requested revision under `deps/gumbo` and writes
   the validated amalgamation to `ext/a-gumbo/gumbo.h` and
   `ext/a-gumbo/gumbo.c`. It also writes `ext/a-gumbo/version.txt` with the
   source repo URL, requested revision, resolved commit SHA-1, and GitHub URLs
   when the source is hosted on GitHub.
4. Review the generated diff. The files are stripped of comments and local
   `#include "..."` directives are expanded in place, because Gumbo has include
   fragments such as `tag_enum.h` and `tag_gperf.h` that are only valid in their
   original contexts.
5. Check `ext/a-gumbo/version.txt` and make sure it records the intended
   upstream revision.
6. Regenerate the Visual Studio projects:

   ```sh
   bun cmd/premake.ts
   ```

7. Build:

   ```sh
   bun ./cmd/build.ts
   ```

The old `ext/gumbo-parser` checkout is intentionally left in the tree for now,
but the active Windows build uses the `a-gumbo` Premake project.
