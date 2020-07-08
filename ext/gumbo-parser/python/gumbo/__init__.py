"""Gumbo HTML parser.

These are the Python bindings for Gumbo.  All public API classes and functions
are exported from this module.  They include:

- CTypes representations of all structs and enums defined in gumbo.h.  The
  naming convention is to take the C name and strip off the "Gumbo" prefix.

- A low-level wrapper around the gumbo_parse function, returning the classes
  exposed above.  Usage:

  import gumbo
  with gumboc.parse(text, **options) as output:
    do_stuff_with_doctype(output.document)
    do_stuff_with_parse_tree(output.root)

- Higher-level bindings that mimic the API provided by html5lib.  Usage:

  from gumbo import html5lib

  This requires that html5lib be installed (it uses their treebuilders), and is
  intended as a drop-in replacement.

- Similarly, higher-level bindings that mimic BeautifulSoup and return
  BeautifulSoup objects.  For this, use:

  import gumbo
  soup = gumbo.soup_parse(text, **options)

  It will give you back a soup object like BeautifulSoup.BeautifulSoup(text).
"""

from gumbo.gumboc import *

try:
  from gumbo import html5lib_adapter as html5lib
except ImportError:
  # html5lib not installed
  pass

try:
  from gumbo.soup_adapter import parse as soup_parse
except ImportError:
  # BeautifulSoup not installed
  pass
