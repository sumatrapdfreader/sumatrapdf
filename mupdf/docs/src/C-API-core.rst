.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


Core API
-----------------------------------------


Almost all functions in the :title:`MuPDF` library take a `fz_context` structure as their first argument. The context is used for many things; primarily it holds the exception stack for our `setjmp` based exception handling. It also holds various caches and auxiliary contexts for font rendering and color management.




The :title:`Fitz` Context
-----------------------------------------


.. note::

    If you wonder where the prefix "fz" and name :title:`Fitz` come from, :title:`MuPDF` originally started out as a prototype of a new rendering library architecture for :title:`Ghostscript`. It was to be a fusion of :title:`libart` and :title:`Ghostscript`. History turned out differently, and the project mutated into a standalone :title:`PDF` renderer now called :title:`MuPDF`. The "fz" prefix for the graphics library and core modules remains to this day.


Here is the code to create a :title:`Fitz` context. The first two arguments are used if you need to use a custom memory allocator, and the third argument is a hint to much memory the various caches should be allowed to grow. The limit is only a soft limit. We may exceed it, but will start clearing out stale data to try to stay below the limit when possible. Setting it to a lower value will prevent the caches from growing out of hand if you are tight on memory.



.. code-block:: C

    #include <mupdf/fitz.h>

    main()
    {
        fz_context *ctx;

        ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
        if (!ctx)
            die("Failed to create a new Fitz context!");

        ... do stuff ...

        fz_drop_context(ctx);
    }


Error Handling
-----------------------------------------

:title:`MuPDF` uses a `setjmp` based exception handling system. This is encapsulated by the use of three macros: `fz_try`, `fz_always`, and `fz_catch`. When an error is raised by `fz_throw`, or re-raised by `fz_rethrow`, execution will jump to the enclosing always/catch block.

All functions you call should be guarded by a `fz_try` block to catch the errors, or the program will call `exit()` on errors. You don't want that.

The `fz_always` block is optional. It is typically used to free memory or release resources unconditionally, in both the case when the execution of the try block succeeds, and when an error occurs.


.. code-block:: C

    fz_try(ctx) {
        // Do stuff that may throw an exception.
    }
    fz_always(ctx) {
        // This (optional) block is always executed.
    }
    fz_catch(ctx) {
        // This block is only executed when recovering from an exception.
    }


Since the `fz_try` macro is based on `setjmp`, the same conditions that apply to local variables in the presence of `setjmp` apply. Any locals written to inside the try block may be restored to their pre-try state when an error occurs. We provide a `fz_var()` macro to guard against this.

In the following example, if we don't guard `buf` with `fz_var`, then when an error occurs the `buf` local variable might have be reset to its pre-try value (`NULL`) and we would leak the memory.

.. code-block:: C

    char *buf = NULL;
    fz_var(buf);
    fz_try(ctx) {
        buf = fz_malloc(ctx, 100);
        // Do stuff with buf that may throw an exception.
    }
    fz_always(ctx) {
        fz_free(ctx, buf);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }

Carefully note that you should **never** return from within a `fz_try` or `fz_always block`! Doing so will unbalance the exception stack, and things will go catastrophically wrong. Instead, it is possible to break out of the `fz_try` and `fz_always` block by using a break statement if you want to exit the block early without throwing an exception.


Throwing a new exception can be done with `fz_throw`. Passing an exception along after having cleaned up in the `fz_catch` block can be done with `fz_rethrow`. `fz_throw` takes a `printf`-like formatting string.


.. code-block:: C

    enum {
        FZ_ERROR_MEMORY, // when malloc fails
        FZ_ERROR_SYNTAX, // recoverable syntax errors
        FZ_ERROR_GENERIC, // all other errors
    };
    void fz_throw(fz_context *ctx, int error_code, const char *fmt, ...);
    void fz_rethrow(fz_context *ctx);


Memory Allocation
-----------------------------------------

You should not need to do raw memory allocation using the :title:`Fitz` context, but if you do, here are the functions you need. These work just like the regular :title:`C` functions, but take a :title:`Fitz` context and throw an exception if the allocation fails. They will **not** return `NULL`; either they will succeed or they will throw an exception.


.. code-block:: C

    void *fz_malloc(fz_context *ctx, size_t size);
    void *fz_realloc(fz_context *ctx, void *old, size_t size);
    void *fz_calloc(fz_context *ctx, size_t count, size_t size);
    void fz_free(fz_context *ctx, void *ptr);

There are also some macros that allocate structures and arrays, together with a type cast to catch typing errors.

.. code-block:: C

    T *fz_malloc_struct(fz_context *ctx, T); // Allocate and zero the memory.
    T *fz_malloc_array(fz_context *ctx, size_t count, T); // Allocate uninitialized memory!
    T *fz_realloc_array(fz_context *ctx, T *old, size_t count, T);


In the rare case that you need an allocation that returns `NULL` on failure, there are variants for that too: `fz_malloc_no_throw`, etc.



Pool Allocator
-----------------------------------------

The pool allocator is used for allocating many small objects that live and die together. All objects allocated from the pool will be freed when the pool is freed.


.. code-block:: C

    typedef struct { opaque } fz_pool;

    fz_pool *fz_new_pool(fz_context *ctx);
    void *fz_pool_alloc(fz_context *ctx, fz_pool *pool, size_t size);
    char *fz_pool_strdup(fz_context *ctx, fz_pool *pool, const char *s);
    void fz_drop_pool(fz_context *ctx, fz_pool *pool);


Reference Counting
-----------------------------------------

Most objects in :title:`MuPDF` use reference counting to keep track of when they are no longer used and can be freed. We use the verbs "keep" and "drop" to increment and decrement the reference count. For simplicity, we also use the word "drop" for non-reference counted objects (so that in case we change our minds and decide to add reference counting to an object, the code that uses it need not change).



Hash Table
-----------------------------------------

We have a generic hash table structure with fixed length keys.

The keys and values are not reference counted by the hash table. Callers are responsible for manually taking care of reference counting when inserting and removing values from the table, should that be desired.

.. code-block:: C

    typedef struct { opaque } fz_hash_table;


`fz_hash_table *fz_new_hash_table(fz_context *ctx, int initial_size, int key_length, int lock, void (*drop_value)(fz_context *ctx, void *value));`
    The lock parameter should be zero, any other value will result in unpredictable behavior. The `drop_value` callback function to the constructor is only used to release values when the hash table is destroyed.

`void fz_drop_hash_table(fz_context *ctx, fz_hash_table *table);`
    Free the hash table and call the `drop_value` function on all the values in the table.

`void *fz_hash_find(fz_context *ctx, fz_hash_table *table, const void *key);`
    Find the value associated with the key. Returns `NULL` if not found.

`void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, const void *key, void *value);`
    Insert the value into the hash table. Inserting a duplicate entry will **not** overwrite the old value, it will return the old value instead. Return `NULL` if the value was inserted for the first time. Does not reference count the value!

`void fz_hash_remove(fz_context *ctx, fz_hash_table *table, const void *key);`
    Remove the associated value from the hash table. This will not reference count the value!

`void fz_hash_for_each(fz_context *ctx, fz_hash_table *table, void *state, void (*callback)(fz_context *ctx, void *state, void *key, int key_length, void *value);`
    Iterate and call a function for each key-value pair in the table.


Binary Tree
-----------------------------------------

The `fz_tree` structure is a self-balancing binary tree that maps text strings to values.

`typedef struct { opaque } fz_tree;`

`void *fz_tree_lookup(fz_context *ctx, fz_tree *node, const char *key);`
    Look up an entry in the tree. Returns `NULL` if not found.

`fz_tree *fz_tree_insert(fz_context *ctx, fz_tree *root, const char *key, void *value);`
    Insert a new entry into the tree. Do not insert duplicate entries. Returns the new root object.

`void fz_drop_tree(fz_context *ctx, fz_tree *node, void (*dropfunc)(fz_context *ctx, void *value));`
    Free the tree and all the values in it.

There is no constructor for this structure, since there is no containing root structure. Instead, the insert function returns the new root node. Use `NULL` for the initial empty tree.


.. code-block:: C

    fz_tree *tree = NULL;
    tree = fz_tree_insert(ctx, tree, "A", my_a_obj);
    tree = fz_tree_insert(ctx, tree, "B", my_b_obj);
    tree = fz_tree_insert(ctx, tree, "C", my_c_obj);
    assert(fz_tree_lookup(ctx, tree, "B") == my_b_obj);



:title:`XML` Parser
-----------------------------------------

We have a rudimentary :title:`XML` parser that handles well formed :title:`XML`. It does not do any namespace processing, and it does not validate the :title:`XML` syntax.

The parser supports `UTF-8`, `UTF-16`, `iso-8859-1`, `iso-8859-7`, `koi8`, `windows-1250`, `windows-1251`, and `windows-1252` encoded input.

If `preserve_white` is *false*, we will discard all *whitespace-only* text elements. This is useful for parsing non-text documents such as :title:`XPS` and :title:`SVG`. Preserving whitespace is useful for parsing :title:`XHTML`.


.. code-block:: C

    typedef struct { opaque } fz_xml_doc;
    typedef struct { opaque } fz_xml;

    fz_xml_doc *fz_parse_xml(fz_context *ctx, fz_buffer *buf, int preserve_white);
    void fz_drop_xml(fz_context *ctx, fz_xml_doc *xml);
    fz_xml *fz_xml_root(fz_xml_doc *xml);

    fz_xml *fz_xml_prev(fz_xml *item);
    fz_xml *fz_xml_next(fz_xml *item);
    fz_xml *fz_xml_up(fz_xml *item);
    fz_xml *fz_xml_down(fz_xml *item);


`int fz_xml_is_tag(fz_xml *item, const char *name);`
    Returns *true* if the element is a tag with the given name.

`char *fz_xml_tag(fz_xml *item);`
    Returns the tag name if the element is a tag, otherwise `NULL`.

`char *fz_xml_att(fz_xml *item, const char *att);`
    Returns the value of the tag element's attribute, or `NULL` if not a tag or missing.

`char *fz_xml_text(fz_xml *item);`
    Returns the `UTF-8` text of the text element, or `NULL` if not a text element.

`fz_xml *fz_xml_find(fz_xml *item, const char *tag);`
    Find the next element with the given tag name. Returns the element itself if it matches, or the first sibling if it doesn't. Returns `NULL` if there is no sibling with that tag name.

`fz_xml *fz_xml_find_next(fz_xml *item, const char *tag);`
    Find the next sibling element with the given tag name, or `NULL` if none.

`fz_xml *fz_xml_find_down(fz_xml *item, const char *tag);`
    Find the first child element with the given tag name, or `NULL` if none.



String Functions
-----------------------------------------

All text strings in :title:`MuPDF` use the `UTF-8` encoding. The following functions encode and decode `UTF-8` characters, and return the number of bytes used by the `UTF-8` character (at most `FZ_UTFMAX`).

.. code-block:: C

    enum { FZ_UTFMAX=4 };
    int fz_chartorune(int *rune, const char *str);
    int fz_runetochar(char *str, int rune);


Since many of the :title:`C` string functions are locale dependent, we also provide our own locale independent versions of these functions. We also have a couple of semi-standard functions like `strsep` and `strlcpy` that we can't rely on the system providing. These should be pretty self explanatory:

.. code-block:: C

    char *fz_strdup(fz_context *ctx, const char *s);
    float fz_strtof(const char *s, char **es);
    char *fz_strsep(char **stringp, const char *delim);
    size_t fz_strlcpy(char *dst, const char *src, size_t n);
    size_t fz_strlcat(char *dst, const char *src, size_t n);
    void *fz_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);
    int fz_strcasecmp(const char *a, const char *b);


There are also a couple of functions to process filenames and :title:`URLs`:

`char *fz_cleanname(char *path);`
    Rewrite path in-place to the shortest string that names the same path. Eliminates multiple and trailing slashes, and interprets "." and "..".

`void fz_dirname(char *dir, const char *path, size_t dir_size);`
    Extract the directory component from a path.

`char *fz_urldecode(char *url);`
    Decode :title:`URL` escapes in-place.


String Formatting
-----------------------------------------

Our `printf` family handles the common `printf` formatting characters, with a few minor differences. We also support several non-standard formatting characters. The same `printf` syntax is used in the `printf` functions in the :title:`I/O` module as well.


.. code-block:: C

    size_t fz_vsnprintf(char *buffer, size_t space, const char *fmt, va_list args);
    size_t fz_snprintf(char *buffer, size_t space, const char *fmt, ...);
    char *fz_asprintf(fz_context *ctx, const char *fmt, ...);

`%%`, `%c`, `%e`, `%f`, `%p`, `%x`, `%d`, `%u`, `%s`
    These behave as usual, but only take padding (+,0,space), width, and precision arguments.

`%g float`
    Prints the `float` in the shortest possible format that won't lose precision, except `NaN` to `0`, `+Inf` to `FLT_MAX`, `-Inf` to `-FLT_MAX`.

`%M fz_matrix*`
    Prints all 6 coefficients in the matrix as `%g` separated by spaces.

`%R fz_rect*`
    Prints all `x0`, `y0`, `x1`, `y1` in the rectangle as `%g` separated by spaces.

`%P fz_point*`
    Prints `x`, `y` in the point as `%g` separated by spaces.

`%C int`
    Formats character as `UTF-8`. Useful to print unicode text.

`%q char*`
    Formats string using double quotes and C escapes.

`%( char*`
    Formats string using parenthesis quotes and :title:`Postscript` escapes.

`%n char*`
    Formats string using prefix `/` and :title:`PDF` name hex-escapes.



Math Functions
-----------------------------------------

We obviously need to deal with lots of points, rectangles, and transformations in :title:`MuPDF`.

Points are fairly self evident. The `fz_make_point` utility function is for use with :title:`Visual Studio` that doesn't yet support the :title:`C99` struct initializer syntax.

.. code-block:: C

    typedef struct {
        float x, y;
    } fz_point;

    fz_point fz_make_point(float x, float y);

Rectangles are represented by two pairs of coordinates. The `x0`, `y0` pair have the smallest values, and in the normal coordinate space used by :title:`MuPDF` that is the upper left corner. The `x1`, `y1` pair have the largest values, typically the lower right corner.

In order to represent an infinite unbounded area, we use an `x0` that is larger than the `x1`.


.. code-block:: C

    typedef struct {
        float x0, y0;
        float x1, y1;
    } fz_rect;

    const fz_rect fz_infinite_rect = { 1, 1, -1, -1 };
    const fz_rect fz_empty_rect = { 0, 0, 0, 0 };
    const fz_rect fz_unit_rect = { 0, 0, 1, 1 };

    fz_rect fz_make_rect(float x0, float y0, float x1, float y1);

Our matrix structure is a row-major 3x3 matrix with the last column always `[ 0 0 1 ]`. This is represented as a struct with six fields, in the same order as in :title:`PDF` and :title:`Postscript`. The identity matrix is a global constant, for easy access.



.. code-block::

    / a b 0 \
    | c d 0 |
    \ e f 1 /

.. code-block:: C

    typedef struct {
        float a, b, c, d, e, f;
    } fz_matrix;

    const fz_matrix fz_identity = { 1, 0, 0, 1, 0, 0 };

    fz_matrix fz_make_matrix(float a, float b, float c, float d, float e, float f);


Sometimes we need to represent a non-axis aligned rectangular-ish area, such as the area covered by some rotated text. For this we use a quad representation, using a points for each of the upper/lower/left/right corners as seen from the reading direction of the text represented.


.. code-block:: C

    typedef struct {
        fz_point ul, ur, ll, lr;
    } fz_quad;


**List of math functions**

These are simple mathematical operations that can not throw errors, so do not need a context argument.

`float fz_abs(float f);`
    Abs for float.

`float fz_min(float a, float b);`
    Min for float.

`float fz_max(float a, float b);`
    Max for float.

`float fz_clamp(float f, float min, float max);`
    Clamp for float.

`int fz_absi(int i);`
    Abs for integer.

`int fz_mini(int a, int b);`
    Min for integer.

`int fz_maxi(int a, int b);`
    Max for integer.

`int fz_clampi(int i, int min, int max);`
    Clamp for integer.

`int fz_is_empty_rect(fz_rect r);`
    Returns whether the supplied `fz_rect` is empty.

`int fz_is_infinite_rect(fz_rect r);`
    Returns whether the supplied `fz_rect` is infinite.

`fz_matrix fz_concat(fz_matrix left, fz_matrix right);`
    Concat two matrices and returns a new matrix.

`fz_matrix fz_scale(float sx, float sy);`
    Scale.

`fz_matrix fz_shear(float sx, float sy);`
    Shear.

`fz_matrix fz_rotate(float degrees);`
    Rotate.

`fz_matrix fz_translate(float tx, float ty);`
    Translate.

`fz_matrix fz_invert_matrix(fz_matrix matrix);`
    Invert a matrix.

`fz_point fz_transform_point(fz_point point, fz_matrix m);`
    Transform a point according to the given matrix.

`fz_point fz_transform_vector(fz_point vector, fz_matrix m);`
    Transform a vector according to the given matrix (ignores translation).

`fz_rect fz_transform_rect(fz_rect rect, fz_matrix m);`
    Transform a `fz_rect` according to the given matrix.

`fz_quad fz_transform_quad(fz_quad q, fz_matrix m);`
    Transform a `fz_quad` according to the given matrix.

`int fz_is_point_inside_rect(fz_point p, fz_rect r);`
    Returns whether the point is inside the supplied `fz_rect`.

`int fz_is_point_inside_quad(fz_point p, fz_quad q);`
    Returns whether the point is inside the supplied `fz_quad`.

`fz_matrix fz_transform_page(fz_rect mediabox, float resolution, float rotate);`
    Create a transform matrix to draw a page at a given resolution and rotation. The scaling factors are adjusted so that the page covers a whole number of pixels. Resolution is given in dots per inch. Rotation is expressed in degrees (`0`, `90`, `180`, and `270` are valid values).
