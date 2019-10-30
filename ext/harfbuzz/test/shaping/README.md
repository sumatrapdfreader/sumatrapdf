Adding tests
============

You can test shaping of a unicode sequence against a font like this:
```sh
$ ./hb-unicode-encode 41 42 43 627 | ../../util/hb-shape font.ttf
```
assuming an in-tree build.  The 41 42 43 627 here is a sequence of
Unicode codepoints: U+0041,0042,0043,0627.  When you are happy with
the shape results, you can use the `record-test.sh` script to add
this to the test suite.  `record-test.sh` requires `pyftsubset` to
be installed.  You can get `pyftsubset` by installing
FontTools from <https://github.com/behdad/fonttools>.

To use `record-test.sh`, just put it right before the `hb-shape` invocation:
```sh
$ ./hb-unicode-encode 41 42 43 627 | ./record-test.sh ../../util/hb-shape font.ttf
```
what this does is:
  * Subset the font for the sequence of Unicode characters requested,
  * Compare the `hb-shape` output of the original font versus the subset
    font for the input sequence,
  * If the outputs differ, perhaps it is because the font does not have
    glyph names; it then compares the output of `hb-view` for both fonts.
  * If the outputs differ, recording fails.  Otherwise, it will move the
    subset font file into `data/in-house/fonts` and name it after its
    hash, and print out the test case input, which you can then redirect
    to an existing or new test file in `data/in-house/tests` using `-o=`,
    e.g.:
```sh
$ ./hb-unicode-encode 41 42 43 627 | ./record-test.sh -o=data/in-house/tests/test-name.test ../../util/hb-shape font.ttf
```

If you created a new test file, add it to `data/in-house/Makefile.sources`
so it is run.  Check that `make check` does indeed run it, and that the
test passes.  When everything looks good, `git add` the new font as well
as the new test file if you created any.  You can see what new files are
there by running `git status data/in-house`.  And commit!

*Note!*  Please only add tests using Open Source fonts, preferably under
OFL or similar license.
