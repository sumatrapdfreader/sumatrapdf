.. Copyright (C) 2001-2023 Artifex Software, Inc.
.. All Rights Reserved.


.. include:: header.rst

.. meta::
   :description: MuPDF documentation
   :keywords: MuPDF, pdf, epub


Using :title:`MuPDF` with :title:`C`
==========================================

This intends to be an introductory guide into how to use :title:`MuPDF` with your :title:`C` code and as such assumes at least an intermediate knowledge of programming with :title:`C`.


Basic :title:`MuPDF` usage example
-------------------------------------

For an example of how to use :title:`MuPDF` in the most basic way, see `docs/examples/example.c`.


.. _docs_examples_example_c:

.. literalinclude:: ../examples/example.c
   :caption: docs/examples/example.c
   :language: c

To limit the complexity and give an easier introduction this code has no error handling at all, but any serious piece of code using :title:`MuPDF` should use :ref:`error handling strategies<using_mupdf_error_handling>`.



Common function arguments
-------------------------------------

Most functions in :title:`MuPDF's` interface take a context argument.

A context contains global state used by :title:`MuPDF` inside functions when parsing or rendering pages of the document. It contains for example:

- an exception stack (see error handling below),
- a memory allocator (allowing for custom allocators)
- a resource store (for caching of images, fonts, etc.)
- a set of locks and (un-)locking functions (for multi-threading)

Without the set of locks and accompanying functions the context and its proxies may only be used in a single-threaded application.



.. _using_mupdf_error_handling:


Error handling
-------------------------------------


:title:`MuPDF` uses a set of exception handling macros to simplify error return and cleanup. Conceptually, they work a lot like :title:`C++'s` try/catch system, but do not require any special compiler support.

The basic formulation is as follows:

.. code-block:: c

   fz_try(ctx)
   {
      // Try to perform a task. Never 'return', 'goto' or
      // 'longjmp' out of here. 'break' may be used to
      // safely exit (just) the try block scope.
   }
   fz_always(ctx)
   {
      // Any code here is always executed, regardless of
      // whether an exception was thrown within the try or
      // not. Never 'return', 'goto' or longjmp out from
      // here. 'break' may be used to safely exit (just) the
      // always block scope.
   }
   fz_catch(ctx)
   {
      // This code is called (after any always block) only
      // if something within the fz_try block (including any
      // functions it called) threw an exception. The code
      // here is expected to handle the exception (maybe
      // record/report the error, cleanup any stray state
      // etc) and can then either exit the block, or pass on
      // the exception to a higher level (enclosing) fz_try
      // block (using fz_throw, or fz_rethrow).
   }

The `fz_always` block is optional, and can safely be omitted.

The macro based nature of this system has 3 main limitations:

Never return from within try (or 'goto' or `longjmp` out of it). This upsets the internal housekeeping of the macros and will cause problems later on. The code will detect such things happening, but by then it is too late to give a helpful error report as to where the original infraction occurred.

The `fz_try(ctx) { ... } fz_always(ctx) { ... } fz_catch(ctx) { ... }` is not one atomic :title:`C` statement. That is to say, if you do:

.. code-block:: c

   if (condition)
      fz_try(ctx) { ... }
      fz_catch(ctx) { ... }

then you will not get what you want. Use the following instead:

.. code-block:: c

   if (condition) {
      fz_try(ctx) { ... }
      fz_catch(ctx) { ... }
   }

The macros are implemented using `setjmp` and `longjmp`, and so the standard :title:`C` restrictions on the use of those functions apply to `fz_try` / `fz_catch` too. In particular, any "truly local" variable that is set between the start of `fz_try` and something in `fz_try` throwing an exception may become undefined as part of the process of throwing that exception.

As a way of mitigating this problem, we provide a `fz_var()` macro that tells the compiler to ensure that that variable is not unset by the act of throwing the exception.

A model piece of code using these macros then might be:


.. code-block:: c

   house build_house(plans *p)
   {
      material m = NULL;
      walls w = NULL;
      roof r = NULL;
      house h = NULL;
      tiles t = make_tiles();

      fz_var(w);
      fz_var(r);
      fz_var(h);

      fz_try(ctx)
      {
         fz_try(ctx)
         {
            m = make_bricks();
         }
         fz_catch(ctx)
         {
            // No bricks available, make do with straw?
            m = make_straw();
         }
         w = make_walls(m, p);
         r = make_roof(m, t);
         // Note, NOT: return combine(w,r);
         h = combine(w, r);
      }
      fz_always(ctx)
      {
         drop_walls(w);
         drop_roof(r);
         drop_material(m);
         drop_tiles(t);
      }
      fz_catch(ctx)
      {
         fz_throw(ctx, "build_house failed");
      }
      return h;
   }

Things to note about this:

If `make_tiles` throws an exception, this will immediately be handled by some higher level exception handler. If it succeeds, `t` will be set before `fz_try` starts, so there is no need to `fz_var(t);`.

We try first off to make some bricks as our building material. If this fails, we fall back to straw. If this fails, we'll end up in the `fz_catch`, and the process will fail neatly.

We assume in this code that combine takes new reference to both the walls and the roof it uses, and therefore that `w` and `r` need to be cleaned up in all cases.

We assume the standard :title:`C` convention that it is safe to destroy `NULL` things.



Multi-threading
-------------------------------------


First off, study the basic usage example in :ref:`docs/examples/example.c<docs_examples_example_c>` and make sure you understand how it works as the data structures manipulated there will be referred to in this section too.

:title:`MuPDF` can usefully be built into a multi-threaded application without the library needing to know anything about threading at all. If the library opens a document in one thread, and then sits there as a 'server' requesting pages and rendering them for other threads that need them, then the library is only ever being called from this one thread.

Other threads can still be used to handle UI requests etc, but as far as :title:`MuPDF` is concerned it is only being used in a single threaded way. In this instance, there are no threading issues with :title:`MuPDF` at all, and it can safely be used without any locking, as described in the previous sections.

This section will attempt to explain how to use :title:`MuPDF` in the more complex case; where we genuinely want to call the :title:`MuPDF` library concurrently from multiple threads within a single application.

:title:`MuPDF` can be invoked with a user supplied set of locking functions. It uses these to take mutexes around operations that would conflict if performed concurrently in multiple threads. By leaving the exact implementation of locks to the caller :title:`MuPDF` remains threading library agnostic.

The following simple rules should be followed to ensure that multi-threaded operations run smoothly:

#. **"No simultaneous calls to MuPDF in different threads are allowed to use the same context."**

   Most of the time it is simplest to just use a different context for every thread; just create a new context at the same time as you create the thread. For more details see "Cloning the context" below.

#. **"No simultaneous calls to MuPDF in different threads are allowed to use the same document."**

   Only one thread can be accessing a document at a time, but once display lists are created from that document, multiple threads at a time can operate on them.

   The document can be used from several different threads as long as there are safeguards in place to prevent the usages being simultaneous.

#. **"No simultaneous calls to MuPDF in different threads are allowed to use the same device."**

   Calling a device simultaneously from different threads will cause it to get confused and may crash. Calling a device from several different threads is perfectly acceptable as long as there are safeguards in place to prevent the calls being simultaneous.

#. **"An fz_locks_context must be supplied at context creation time, unless MuPDF is to be used purely in a single thread at a time."**

   :title:`MuPDF` needs to protect against unsafe access to certain structures/resources/libraries from multiple threads. It does this by using the user supplied locking functions. This holds true even when using completely separate instances of :title:`MuPDF`.

#. **"All contexts in use must share the same fz_locks_context (or the underlying locks thereof)."**

   We strongly recommend that `fz_new_context` is called just once, and `fz_clone_context` is called to generate new contexts from that. This will automatically ensure that the same locking mechanism is used in all :title:`MuPDF` instances. For now, we do support multiple completely independent contexts being created using repeated calls to `fz_new_context`, but these MUST share the same `fz_locks_context` (or at least depend upon the same underlying locks). The facility to create different independent contexts may be removed in future.

**So, how does a multi-threaded example differ from a non-multithreaded one?**

Firstly, when we create the first context, we call `fz_new_context` as before, but the second argument should be a pointer to a set of locking functions.

The calling code should provide `FZ_LOCK_MAX` mutexes, which will be locked/unlocked by :title:`MuPDF` calling the lock/unlock function pointers in the supplied structure with the user pointer from the structure and the lock number, `i` (`0 <= i < FZ_LOCK_MAX`). These mutexes can safely be recursive or non-recursive as :title:`MuPDF` only calls in a non-recursive style.

To make subsequent contexts, the user should **not** call `fz_new_context` again (as this will fail to share important resources such as the store and glyph cache), but should rather call `fz_clone_context`. Each of these cloned contexts can be freed by `fz_free_context` as usual. They will share the important data structures (like store, glyph cache etc.) with the original context, but will have their own exception stacks.

To open a document, call `fz_open_document` as usual, passing a context and a filename. It is important to realise that only one thread at a time can be accessing the documents itself.

This means that only one thread at a time can perform operations such as fetching a page, or rendering that page to a display list. Once a display list has been obtained however, it can be rendered from any other thread (or even from several threads simultaneously, giving banded rendering).

This means that an implementer has 2 basic choices when constructing an application to use :title:`MuPDF` in multi-threaded mode. Either they can construct it so that a single nominated thread opens the document and then acts as a 'server' creating display lists for other threads to render, or they can add their own mutex around calls to :title:`MuPDF` that use the document. The former is likely to be far more efficient in the long run.

For an example of how to do multi-threading see below which has a main thread and one rendering thread per page.


.. _docs_examples_multi_threaded_c:

.. literalinclude:: ../examples/multi-threaded.c
   :caption: docs/examples/multi-threaded.c
   :language: c




Cloning the context
----------------------------

As described above, every context contains an exception stack which is manipulated during the course of nested `fz_try` / `fz_catches`. For obvious reasons the same exception stack cannot be used from more than one thread at a time.

If, however, we simply created a new context (using `fz_new_context`) for every thread, we would end up with separate stores/glyph caches etc., which is not (generally) what is desired. :title:`MuPDF` therefore provides a mechanism for "cloning" a context. This creates a new context that shares everything with the given context, except for the exception stack.

A commonly used general scheme is therefore to create a 'base' context at program start up, and to clone this repeatedly to get new contexts that can be used on new threads.



Coding Style
----------------


Names
~~~~~~~~~~~~~~

Functions should be named according to one of the following schemes:

- **verb_noun**.
- **verb_noun_with_noun**.
- **noun_attribute**.
- **set_noun_attribute**.
- **noun_from_noun** - convert from one type to another (avoid **noun_to_noun**).


Prefixes are mandatory for exported functions, macros, enums, globals and types:

- `fz` for common code.
- `pdf`, `xps`, etc., for interpreter specific code.

Prefixes are optional (but encouraged) for private functions and types.

Avoid using 'get' as this is a meaningless and redundant filler word.

These words are reserved for reference counting schemes:

- **new**, **create**, **find**, **load**, **open**, **keep** - return objects that you are responsible for freeing.
- **drop** - relinquish ownership of the object passed in.

When searching for an object or value, the name used depends on whether returning the value is passing ownership:

- **lookup** - return a value or borrowed pointer.
- **find** - return an object that the caller is responsible for freeing.


Types
~~~~~~~~~~

Various different integer types are used throughout :title:`MuPDF`.

In general:

- `int` is assumed to be 32bit at least.
- `short` is assumed to be exactly 16 bits.
- `char` is assumed to be exactly 8 bits.
- Array sizes, string lengths, and allocations are measured using `size_t`. `size_t` is 32bit in 32bit builds, and 64bit on all 64bit builds.
- Buffers of data use unsigned chars (or `uint8_t`).
- Offsets within files/streams are represented using `int64_t`.


In addition, we use floats (and avoid doubles when possible), assumed to be `IEEE compliant`_.

Reference counting
~~~~~~~~~~~~~~~~~~~~


Reference counting uses special words in functions to make it easy to remember and follow the rules.

Words that take ownership: ``new``, ``find``, ``load``, ``open``, ``keep``.

Words that release ownership: ``drop``.

If an object is returned by a function with one of the special words that take ownership, you are responsible for freeing it by calling ``drop`` or ``free``, or ``close`` before you return. You may pass ownership of an owned object by return it only if you name the function using one of the special words.

Any objects returned by functions that do not have any of these special words, are borrowed and have a limited life time. Do not hold on to them past the duration of the current function, or stow them away inside structs. If you need to keep the object for longer than that, you have to either ``keep`` it or make your own copy.




.. include:: footer.rst



.. External links

.. _IEEE compliant: https://en.wikipedia.org/wiki/IEEE_754
