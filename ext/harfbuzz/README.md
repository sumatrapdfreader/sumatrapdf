[![Linux CI Status](https://github.com/harfbuzz/harfbuzz/workflows/linux-ci/badge.svg)](https://github.com/harfbuzz/harfbuzz/workflows/linux-ci/badge.svg)
[![CircleCI Build Status](https://circleci.com/gh/harfbuzz/harfbuzz/tree/main.svg?style=svg)](https://circleci.com/gh/harfbuzz/harfbuzz/tree/main)
[![OSS-Fuzz Status](https://oss-fuzz-build-logs.storage.googleapis.com/badges/harfbuzz.svg)](https://oss-fuzz-build-logs.storage.googleapis.com/index.html)
[![Coverity Scan Build Status](https://scan.coverity.com/projects/15166/badge.svg)](https://scan.coverity.com/projects/harfbuzz)
[![Codacy Badge](https://app.codacy.com/project/badge/Grade/89c872f5ce1c42af802602bfcd15d90a)](https://www.codacy.com/gh/harfbuzz/harfbuzz/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=harfbuzz/harfbuzz&amp;utm_campaign=Badge_Grade)
[![Codecov Code Coverage](https://codecov.io/gh/harfbuzz/harfbuzz/branch/main/graph/badge.svg)](https://codecov.io/gh/harfbuzz/harfbuzz)
[![Packaging status](https://repology.org/badge/tiny-repos/harfbuzz.svg)](https://repology.org/project/harfbuzz/versions)

# HarfBuzz

HarfBuzz is a text shaping engine. It primarily supports [OpenType][1], but also
[Apple Advanced Typography][2]. HarfBuzz is used in Android, Chrome,
ChromeOS, Firefox, GNOME, GTK+, KDE, LibreOffice, OpenJDK, PlayStation, Qt,
XeTeX, and other places.

For bug reports, mailing list, and other information please visit:

  http://harfbuzz.org/

For license information, see [COPYING](COPYING).

## Documentation

For user manual as well as API documentation, check: https://harfbuzz.github.io

## Download

For tarball releases of HarfBuzz, look [here][3]. At the same place you
will also find Win32/Win64 binary bundles that include libharfbuzz DLL,
hb-view.exe, hb-shape.exe, and all dependencies.

The canonical source tree is available on [github][4].

The API that comes with `hb.h` will not change incompatibly. Other, peripheral,
headers are more likely to go through minor modifications, but again, we do our
best to never change API in an incompatible way. We will never break the ABI.

If you are not sure whether Pango or HarfBuzz is right for you, read [Pango vs
HarfBuzz][5].

## Development

For build information, see [BUILD.md](BUILD.md).

For custom configurations, see [CONFIG.md](CONFIG.md).

For testing and profiling, see [TESTING.md](TESTING.md).

To get a better idea of where HarfBuzz stands in the text rendering stack you
may want to read [State of Text Rendering][6], though, that document is many
years old. Here are a few presentation slides about HarfBuzz at the
Internationalization and Unicode Conference over the years:

*   November 2014, [Unicode, OpenType, and HarfBuzz: Closing the Circle][7],
*   October 2012, [HarfBuzz, The Free and Open Text Shaping Engine][8],
*   October 2009, [HarfBuzz: the Free and Open Shaping Engine][9].

Both development and user support discussion around HarfBuzz happens on the
[github][4].

To report bugs or submit patches please use [github][4] issues and
pull-requests.

For a comparison of old vs new HarfBuzz memory consumption see [this][10].

<!--See past and upcoming [HarfBuzz Hackfests](https://freedesktop.org/wiki/Software/HarfBuzz/Hackfests/)!-->

## Name

HarfBuzz (حرف‌باز) is my Persian translation of “[OpenType][1]”,
transliterated using the Latin script. It sports a second meaning, but that
ain’t translatable.

> Background: Originally there was this font format called TrueType. People and
> companies started calling their type engines all things ending in Type:
> FreeType, CoolType, ClearType, etc. And then came OpenType, which is the
> successor of TrueType. So, for my OpenType implementation, I decided to stick
> with the concept but use the Persian translation. Which is fitting given that
> Persian is written in the Arabic script, and OpenType is an extension of
> TrueType that adds support for complex script rendering, and HarfBuzz is an
> implementation of OpenType complex text shaping.

<details>
  <summary>Packaging status of HarfBuzz</summary>

[![Packaging status](https://repology.org/badge/vertical-allrepos/harfbuzz.svg?header=harfbuzz)](https://repology.org/project/harfbuzz/versions)

</details>

[1]: https://docs.microsoft.com/en-us/typography/opentype/spec/
[2]: https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6AATIntro.html
[3]: https://github.com/harfbuzz/harfbuzz/releases
[4]: https://github.com/harfbuzz/harfbuzz
[5]: http://mces.blogspot.com/2009/11/pango-vs-harfbuzz.html
[6]: http://behdad.org/text/
[7]: https://goo.gl/FSIQuC
[8]: https://goo.gl/2wSRu
[9]: http://behdad.org/download/Presentations/slippy/harfbuzz_slides.pdf
[10]: https://goo.gl/woyty
