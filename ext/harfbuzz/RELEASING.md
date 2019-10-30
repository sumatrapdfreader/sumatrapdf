HarfBuzz release walk-through checklist:

1. Open gitk and review changes since last release.

   * `git diff $(git describe | sed 's/-.*//').. src/*.h` prints all public API
     changes.

     Document them in NEWS.  All API and API semantic changes should be clearly
     marked as API additions, API changes, or API deletions.  Document
     deprecations.  Ensure all new API / deprecations are in listed correctly in
     docs/harfbuzz-sections.txt

     If there's a backward-incompatible API change (including deletions for API
     used anywhere), that's a release blocker.  Do NOT release.

2. Based on severity of changes, decide whether it's a minor or micro release
   number bump,

3. Search for REPLACEME on the repository and replace it with the chosen version
   for the release.

4. Make sure you have correct date and new version at the top of NEWS file,

5. Bump version in configure.ac line 3,

6. Do "make distcheck", if it passes, you get a tarball.
   Otherwise, fix things and commit them separately before making release,
   Note: Check src/hb-version.h and make sure the new version number is
   there.  Sometimes, it does not get updated.  If that's the case,
   "touch configure.ac" and rebuild.  TODO: debug.

7. "make release-files".  Enter your GPG password.  This creates a sha256 hash
   and signs it.

8. Now that you have release files, commit NEWS, configure.ac, and src/hb-version.h,
   as well as any REPLACEME changes you made.  The commit message is simply the
   release number.  Eg. "1.4.7"

9. Tag the release and sign it: Eg. "git tag -s 1.4.7 -m 1.4.7".  Enter your
   GPG password again.

10. Build win32 bundle.

   a. Put contents of [this](https://drive.google.com/open?id=0B3_fQkxDZZXXbWltRGd5bjVrUDQ) on your `~/.local/i686-w64-mingw32`,

   b. Run `../mingw32.sh --with-uniscribe` script to configure harfbuzz with mingw
   in a subdirector (eg. winbuild/),

   c. make

   d. Back in the parent directory, run `./UPDATE.sh`(available below) to build win32
      bundle.

11. Copy all artefacts to users.freedesktop.org and move them into
    `/srv/www.freedesktop.org/www/software/harfbuzz/release` There should be four
    files.  Eg.:
 ```
-rw-r--r--  1 behdad eng 1592693 Jul 18 11:25 harfbuzz-1.4.7.tar.bz2
-rw-r--r--  1 behdad eng      89 Jul 18 11:34 harfbuzz-1.4.7.tar.bz2.sha256
-rw-r--r--  1 behdad eng     339 Jul 18 11:34 harfbuzz-1.4.7.tar.bz2.sha256.asc
-rw-r--r--  1 behdad eng 2895619 Jul 18 11:34 harfbuzz-1.4.7-win32.zip
```

12. While doing that, quickly double-check the size of the .tar.bz2 and .zip
    files against their previous releases to make sure nothing bad happened.
    They should be in the ballpark, perhaps slightly larger.  Sometimes they
    do shrink, that's not by itself a stopper.

13. Push the commit and tag out: "git push --follow-tags".  Make sure it's
    pushed both to freedesktop repo and github.

14. Go to GitHub release page [here](https://github.com/harfbuzz/harfbuzz/releases),
    edit the tag, upload artefacts and NEWS entry and save.


## UPDATE.sh
```bash
#!/bin/bash

v=$1

if test "x$v" = x; then
	echo "usage: UPDATE.sh micro-version"
	exit 1
fi

dir_prefix=harfbuzz-1.4.
dir_suffix=-win32
dir=$dir_prefix$v$dir_suffix
dir_old=$dir_prefix$((v-1))$dir_suffix
if test -d "$dir"; then
	echo "New dir $dir exists; not overwriting"
	exit 1
fi
if ! test -d "$dir_old"; then
	echo "Old dir $dir_old does NOT exist; aborting"
	exit 1
fi
set -ex
cp -a "$dir_old" "$dir.tmp"
rm -f "$dir.tmp"/GDX32.dll
rm -f "$dir.tmp"/usp10.dll
cp ../winbuild/src/.libs/libharfbuzz-0.dll{,.def} $dir.tmp/
cp ../winbuild/util/.libs/hb-{shape,view}.exe $dir.tmp/
i686-w64-mingw32-strip $dir.tmp/{hb-shape.exe,hb-view.exe,libharfbuzz-0.dll}
mv $dir.tmp $dir
zip -r $dir.zip $dir
echo Bundle $dir.zip ready
```
