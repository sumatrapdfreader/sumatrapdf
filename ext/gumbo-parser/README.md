Gumbo - A pure-C HTML5 parser.
============

[![Build Status](https://travis-ci.org/google/gumbo-parser.svg?branch=master)](https://travis-ci.org/google/gumbo-parser) [![Build status](https://ci.appveyor.com/api/projects/status/k5xxn4bxf62ao2cp?svg=true)](https://ci.appveyor.com/project/nostrademons/gumbo-parser)

Gumbo is an implementation of the [HTML5 parsing algorithm][] implemented
as a pure C99 library with no outside dependencies.  It's designed to serve
as a building block for other tools and libraries such as linters,
validators, templating languages, and refactoring and analysis tools.

Goals & features:

* Fully conformant with the [HTML5 spec][].
* Robust and resilient to bad input.
* Simple API that can be easily wrapped by other languages.
* Support for source locations and pointers back to the original text.
* Support for fragment parsing.
* Relatively lightweight, with no outside dependencies.
* Passes all [html5lib tests][], including the template tag.
* Tested on over 2.5 billion pages from Google's index.

Non-goals:

* Execution speed.  Gumbo gains some of this by virtue of being written in
  C, but it is not an important consideration for the intended use-case, and
  was not a major design factor.
* Support for encodings other than UTF-8.  For the most part, client code
  can convert the input stream to UTF-8 text using another library before
  processing.
* Mutability.  Gumbo is intentionally designed to turn an HTML document into a
  parse tree, and free that parse tree all at once.  It's not designed to
  persistently store nodes or subtrees outside of the parse tree, or to perform
  arbitrary DOM mutations within your program.  If you need this functionality,
  we recommend translating the Gumbo parse tree into a mutable DOM
  representation more suited for the particular needs of your program before
  operating on it.
* C89 support.  Most major compilers support C99 by now; the major exception
  (Microsoft Visual Studio) should be able to compile this in C++ mode with
  relatively few changes.  (Bug reports welcome.)
* ~~Security.  Gumbo was initially designed for a product that worked with
  trusted input files only.  We're working to harden this and make sure that it
  behaves as expected even on malicious input, but for now, Gumbo should only be
  run on trusted input or within a sandbox.~~ Gumbo underwent a number of
  security fixes and passed Google's security review as of version 0.9.1.

Wishlist (aka "We couldn't get these into the original release, but are
hoping to add them soon"):

* Full-featured error reporting.
* Additional performance improvements.
* DOM wrapper library/libraries (possibly within other language bindings)
* Query libraries, to extract information from parse trees using CSS or XPATH.

Installation
============

To build and install the library, issue the standard UNIX incantation from
the root of the distribution:

```bash
$ ./autogen.sh
$ ./configure
$ make
$ sudo make install
```

Gumbo comes with full pkg-config support, so you can use the pkg-config to
print the flags needed to link your program against it:

```bash
$ pkg-config --cflags gumbo         # print compiler flags
$ pkg-config --libs gumbo           # print linker flags
$ pkg-config --cflags --libs gumbo  # print both
```

For example:

```bash
$ gcc my_program.c `pkg-config --cflags --libs gumbo`
```

See the pkg-config man page for more info.

There are a number of sample programs in the examples/ directory.  They're
built automatically by 'make', but can also be made individually with
`make <programname>` (eg. `make clean_text`).

To run the unit tests, you'll need to have [googletest][] downloaded and
unzipped.  The googletest maintainers recommend against using
`make install`; instead, symlink the root googletest directory to 'gtest'
inside gumbo's root directory, and then `make check`:

```bash
$ unzip gtest-1.6.0.zip
$ cd gumbo-*
$ ln -s ../gtest-1.6.0 gtest
$ make check
```

Gumbo's `make check` has code to automatically configure & build gtest and
then link in the library.

Debian and Fedora users can install libgtest with:

```bash
$ apt-get install libgtest-dev  # Debian/Ubuntu
$ yum install gtest-devel       # CentOS/Fedora
```

Note for Ubuntu users: libgtest-dev package only install source files.
You have to make libraries yourself using cmake:

    $ sudo apt-get install cmake
    $ cd /usr/src/gtest
    $ sudo cmake CMakeLists.txt
    $ sudo make
    $ sudo cp *.a /usr/lib

The configure script will detect the presence of the library and use that
instead.

Note that you need to have super user privileges to execute these commands.
On most distros, you can prefix the commands above with `sudo` to execute
them as the super user.

Debian installs usually don't have `sudo` installed (Ubuntu however does.)
Switch users first with `su -`, then run `apt-get`.

Basic Usage
===========

Within your program, you need to include "gumbo.h" and then issue a call to
`gumbo_parse`:

```C
#include "gumbo.h"

int main() {
  GumboOutput* output = gumbo_parse("<h1>Hello, World!</h1>");
  // Do stuff with output->root
  gumbo_destroy_output(&kGumboDefaultOptions, output);
}
```

See the API documentation and sample programs for more details.

A note on API/ABI compatibility
===============================

We'll make a best effort to preserve API compatibility between releases.
The initial release is a 0.9 (beta) release to solicit comments from early
adopters, but if no major problems are found with the API, a 1.0 release
will follow shortly, and the API of that should be considered stable.  If
changes are necessary, we follow [semantic versioning][].

We make no such guarantees about the ABI, and it's very likely that
subsequent versions may require a recompile of client code.  For this
reason, we recommend NOT using Gumbo data structures throughout a program,
and instead limiting them to a translation layer that picks out whatever
data is needed from the parse tree and then converts that to persistent
data structures more appropriate for the application.  The API is
structured to encourage this use, with a single delete function for the
whole parse tree, and is not designed with mutation in mind.

Python usage
============

To install the python bindings, make sure that the
C library is installed first, and then `sudo python setup.py install` from
the root of the distro.  This installs a 'gumbo' module; `pydoc gumbo`
should tell you about it.

Recommended best-practice for Python usage is to use one of the adapters to
an existing API (personally, I prefer BeautifulSoup) and write your program
in terms of those.  The raw CTypes bindings should be considered building
blocks for higher-level libraries and rarely referenced directly.

External Bindings and other wrappers
====================================

The following language bindings or other tools/wrappers are maintained by
various contributors in other repositories:

* C++: [gumbo-query] by lazytiger
* Ruby:
  * [ruby-gumbo] by Nicolas Martyanoff
  * [nokogumbo] by Sam Ruby
* Node.js: [node-gumbo-parser] by Karl Westin
* D: [gumbo-d] by Christopher Bertels
* Lua: [lua-gumbo] by Craig Barnes
* Objective-C:
  * [ObjectiveGumbo] by Programming Thomas
  * [OCGumbo] by TracyYih
* C#: [GumboBindings] by Vladimir Zotov
* PHP: [GumboPHP] by Paul Preece
* Perl: [HTML::Gumbo] by Ruslan Zakirov
* Julia: [Gumbo.jl] by James Porter
* C/Libxml: [gumbo-libxml] by Jonathan Tang

[gumbo-query]: https://github.com/lazytiger/gumbo-query
[ruby-gumbo]: https://github.com/nevir/ruby-gumbo
[nokogumbo]: https://github.com/rubys/nokogumbo
[node-gumbo-parser]: https://github.com/karlwestin/node-gumbo-parser
[gumbo-d]: https://github.com/bakkdoor/gumbo-d
[lua-gumbo]: https://github.com/craigbarnes/lua-gumbo
[OCGumbo]: https://github.com/tracy-e/OCGumbo
[ObjectiveGumbo]: https://github.com/programmingthomas/ObjectiveGumbo
[GumboBindings]: https://github.com/rgripper/GumboBindings
[GumboPHP]: https://github.com/BipSync/gumbo
[Gumbo.jl]: https://github.com/porterjamesj/Gumbo.jl
[gumbo-libxml]: https://github.com/nostrademons/gumbo-libxml

[HTML5 parsing algorithm]: http://www.whatwg.org/specs/web-apps/current-work/multipage/#auto-toc-12
[HTML5 spec]: http://www.whatwg.org/specs/web-apps/current-work/multipage/
[html5lib tests]: https://github.com/html5lib/html5lib-tests
[googletest]: https://code.google.com/p/googletest/
[semantic versioning]: http://semver.org/
[HTML::Gumbo]: https://metacpan.org/pod/HTML::Gumbo
