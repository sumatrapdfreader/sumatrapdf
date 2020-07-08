#!/usr/bin/env python
import sys
from setuptools import setup
from setuptools.command.sdist import sdist

_name_of_lib = 'libgumbo.so'
if sys.platform.startswith('darwin'):
    _name_of_lib = 'libgumbo.dylib'
elif sys.platform.startswith('win'):
    _name_of_lib = 'gumbo.dll'

class CustomSdistCommand(sdist):
    """Customized Sdist command, to copy libgumbo.so into the Python directory
    so that it can be installed with `pip install`."""
    def run(self):
        try:
            import shutil
            shutil.copyfile('.libs/' + _name_of_lib,
                'python/gumbo/' + _name_of_lib)
            sdist.run(self)
        except IOError as e:
            print(e)


README = '''Gumbo - A pure-C HTML5 parser.
==============================

Gumbo is an implementation of the `HTML5 parsing algorithm <http://www.whatwg.org/specs/web-apps/current-work/multipage/#auto-toc-12>`_ implemented
as a pure C99 library with no outside dependencies.  It's designed to serve
as a building block for other tools and libraries such as linters,
validators, templating languages, and refactoring and analysis tools.  This
package contains the library itself, Python ctypes bindings for the library, and
adapters for html5lib and BeautifulSoup (3.2) that give it the same API as those
libaries.

Goals & features:
-----------------

- Robust and resilient to bad input.

- Simple API that can be easily wrapped by other languages.

- Support for source locations and pointers back to the original text.

- Relatively lightweight, with no outside dependencies.

- Passes all `html5lib-0.95 tests <https://github.com/html5lib/html5lib-tests>`_.

- Tested on over 2.5 billion pages from Google's index.

Non-goals:
----------

- Execution speed.  Gumbo gains some of this by virtue of being written in
  C, but it is not an important consideration for the intended use-case, and
  was not a major design factor.

- Support for encodings other than UTF-8.  For the most part, client code
  can convert the input stream to UTF-8 text using another library before
  processing.

- Security.  Gumbo was initially designed for a product that worked with
  trusted input files only.  We're working to harden this and make sure that it
  behaves as expected even on malicious input, but for now, Gumbo should only be
  run on trusted input or within a sandbox.

- C89 support.  Most major compilers support C99 by now; the major exception
  (Microsoft Visual Studio) should be able to compile this in C++ mode with
  relatively few changes.  (Bug reports welcome.)

Wishlist (aka "We couldn't get these into the original release, but are
hoping to add them soon"):

- Support for recent HTML5 spec changes to support the template tag.

- Support for fragment parsing.

- Full-featured error reporting.

- Bindings in other languages.

Installation
------------

```pip install gumbo``` should do it.  If you have a local copy, ```python
setup.py install``` from the root directory.

The `html5lib <https://pypi.python.org/pypi/html5lib/0.999>`_ and
`BeautifulSoup <https://pypi.python.org/pypi/BeautifulSoup/3.2.1>`_ adapters
require that their respective libraries be installed separately to work.

Basic Usage
-----------

For the ctypes bindings:

.. code-block:: python

    import gumbo
    
    with gumbo.parse(text) as output:
        root = output.contents.root.contents
        # root is a Node object representing the root of the parse tree
        # tree-walk over it as necessary.

For the BeautifulSoup bindings:

.. code-block:: python

    import gumbo

    soup = gumbo.soup_parse(text)
    # soup is a BeautifulSoup object representing the parse tree.

For the html5lib bindings:

.. code-block:: python

    from gumbo import html5lib

    doc = html5lib.parse(text[, treebuilder='lxml'])

Recommended best-practice for Python usage is to use one of the adapters to
an existing API (personally, I prefer BeautifulSoup) and write your program
in terms of those.  The raw CTypes bindings should be considered building
blocks for higher-level libraries and rarely referenced directly.

See the source code, Pydoc, and implementation of soup_adapter and
html5lib_adapter for more information.

A note on API/ABI compatibility
-------------------------------

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

Most of this is transparent to Python usage, as the Python adapters are all
built with this in mind.  However, since ctypes requires ABI compatibility, it
does mean you'll have to re-deploy the gumboc library and C extension when
upgrading to a new version.
'''

CLASSIFIERS = [
    'Development Status :: 4 - Beta',
    'Intended Audience :: Developers',
    'License :: OSI Approved :: Apache Software License',
    'Operating System :: Unix',
    'Operating System :: POSIX :: Linux',
    'Programming Language :: C',
    'Programming Language :: Python',
    'Programming Language :: Python :: 2',
    'Programming Language :: Python :: 2.7',
    'Programming Language :: Python :: 3',
    'Programming Language :: Python :: 3.4',
    'Topic :: Software Development :: Libraries :: Python Modules',
    'Topic :: Text Processing :: Markup :: HTML'
]

setup(name='gumbo',
      version='0.10.1',
      description='Python bindings for Gumbo HTML parser',
      long_description=README,
      url='http://github.com/google/gumbo-parser',
      keywords='gumbo html html5 parser google html5lib beautifulsoup',
      author='Jonathan Tang',
      author_email='jonathan.d.tang@gmail.com',
      license='Apache 2.0',
      classifiers=CLASSIFIERS,
      packages=['gumbo'],
      package_dir={'': 'python'},
      package_data={'gumbo': [_name_of_lib]},
      cmdclass={ 'sdist': CustomSdistCommand },
      zip_safe=False)
