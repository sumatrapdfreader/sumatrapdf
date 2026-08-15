# Coding Style

This is a basic overview of the MuPDF coding style used in the C library.

## Indentation

There are two hard rules:

1. Indent with hard tabs.
2. Do not vertically align anything beyond the left edge!

If you want a different indentation level, configure your editor to use
whichever tab width you prefer. If you follow the two rules above, everything
will work out perfectly.

The rest you should be able to infer from the source around you:

- Curly braces on their own line.
- Put a space between `if`, `for`, `while`, and their expression.

## Names

Functions should be named according to one of the following schemes:

- verb_noun
- verb_noun_with_noun
- noun_attribute
- set_noun_attribute
- noun_from_noun -- convert from one type to another (avoid noun_to_noun)

Prefixes are mandatory for exported functions, macros, enums, globals and
types.

- fz for common code
- pdf, xps, etc., for interpreter specific code

Prefixes are optional (but encouraged) for private functions and types.

Avoid using 'get' as this is a meaningless and redundant filler word.

These words are reserved for reference counting schemes:

- new, create, find, load, open, keep -- return objects that you are responsible for freeing.
- drop -- relinquish ownership of the object passed in.

When searching for an object or value, the name used depends on whether
returning the value is passing ownership:

- lookup -- return a value or borrowed pointer
- find -- return an object that the caller is responsible for freeing

## Types

Various different integer types are used throughout MuPDF.

In general:

- int is assumed to be 32bit at least.
- short is assumed to be exactly 16 bits.
- char is assumed to be exactly 8 bits.
- array sizes, string lengths, and allocations are measured using size_t.
- size_t is 32bit in 32bit builds, and 64bit on all 64bit builds.
- buffers of data use unsigned chars (or uint8_t).
- Offsets within files/streams are represented using int64_t.

In addition, we use floats (and avoid doubles when possible), assumed to be
IEEE compliant.

## Reference counting

Reference counting uses special words in functions to make it easy to remember
and follow the rules.

Words that take ownership: new, find, load, open, keep.

Words that release ownership: drop.

If an object is returned by a function with one of the special words that take
ownership, you are responsible for freeing it by calling "drop" or "free", or
"close" before you return. You may pass ownership of an owned object by return
it only if you name the function using one of the special words.

Any objects returned by functions that do not have any of these special words,
are borrowed and have a limited life time. Do not hold on to them past the
duration of the current function, or stow them away inside structs. If you need
to keep the object for longer than that, you have to either "keep" it or make
your own copy.
