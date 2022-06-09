# HarfBuzz release walk-through checklist:

- [ ] Open gitk and review changes since last release.

	- [ ] Print all public API changes:
        `git diff $(git describe | sed 's/-.*//').. src/*.h`

    - [ ]  Document them in NEWS.
        All API and API semantic changes should be clearly marked as API additions, API changes, or API deletions.

    - [ ] Document deprecations.
        Ensure all new API / deprecations are in listed correctly in docs/harfbuzz-sections.txt.
        If release added new API, add entry for new API index at the end of docs/harfbuzz-docs.xml.

     If there's a backward-incompatible API change (including deletions for API used anywhere), that's a release blocker.
     Do NOT release.

- [ ] Based on severity of changes, decide whether it's a minor or micro release number bump.

- [ ] Search for REPLACEME on the repository and replace it with the chosen version for the release.

- [ ] Make sure you have correct date and new version at the top of NEWS file.

- [ ] Bump version in line 3 of meson.build and configure.ac.

- [ ] Do a `meson test -Cbuild` so it both checks the tests and updates hb-version.h (use `git diff` to see if is really updated).

- [ ] Commit NEWS, meson.build, configure.ac, and src/hb-version.h, as well as any REPLACEME changes you made.
        The commit message is simply the release number, e. g. "1.4.7"

- [ ] Do a `meson dist -Cbuild` that runs the tests against the latest committed changes.
   If doesn't pass, something fishy is going on, reset the repo and start over.

- [ ] Tag the release and sign it: e.g. `git tag -s 1.4.7 -m 1.4.7`.
	  Enter your GPG password.

- [ ] Push the commit and tag out: `git push --follow-tags`.
