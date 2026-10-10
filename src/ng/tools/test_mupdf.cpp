// Smoke test for the mupdf static library: opens a document and prints how
// many pages it has. With -js-throw / -js-oom it also checks that an error
// while form JavaScript runs doesn't escape on the JS engine's context.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

// Not empty: the linker would fold empty ones into mupdf's default lock
// functions, and fz_clone_context() refuses a context that has those.
static int gLockDepth = 0;

static void Lock(void*, int) {
    gLockDepth++;
}

static void Unlock(void*, int) {
    gLockDepth--;
}

static void ThrowingConsoleWrite(void* user, const char*) {
    fz_throw((fz_context*)user, FZ_ERROR_GENERIC, "console write failed");
}

static pdf_js_console gThrowingConsole = {nullptr, nullptr, nullptr, nullptr, ThrowingConsoleWrite};

// size of the one allocation -js-oom fails: the copy of a kOomValueLen
// character event.value, which nothing else in mupdf happens to ask for
constexpr size_t kOomValueLen = 5000;
constexpr size_t kOomAllocSize = kOomValueLen + 1;

static void* OomMalloc(void*, size_t size) {
    if (size == kOomAllocSize) {
        return nullptr;
    }
    return malloc(size);
}

static void* OomRealloc(void*, void* p, size_t size) {
    return realloc(p, size);
}

static void OomFree(void*, void* p) {
    free(p);
}

enum class JsFault {
    ConsoleThrows,
    ValueCopyOom,
};

// Loads page 1 on a cloned context, the way a render thread does: the JS
// engine keeps the context it was enabled with, which has no fz_try here.
static int JsFaultTest(const char* path, JsFault fault) {
    // cloning needs lock callbacks; this test has one thread
    fz_locks_context locks = {nullptr, Lock, Unlock};
    fz_alloc_context oomAlloc = {nullptr, OomMalloc, OomRealloc, OomFree};
    fz_alloc_context* alloc = fault == JsFault::ValueCopyOom ? &oomAlloc : nullptr;
    fz_context* ctx = fz_new_context(alloc, &locks, FZ_STORE_DEFAULT);
    if (!ctx) {
        fprintf(stderr, "fz_new_context() failed\n");
        return 1;
    }
    fz_document* doc = nullptr;
    fz_var(doc);
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, path);
        pdf_document* pdf = pdf_document_from_fz_document(ctx, doc);
        pdf_enable_js(ctx, pdf);
        if (fault == JsFault::ConsoleThrows) {
            pdf_js_set_console(ctx, pdf, &gThrowingConsole, ctx);
        }
    }
    fz_catch(ctx) {
        fprintf(stderr, "%s: %s\n", path, fz_caught_message(ctx));
        return 1;
    }

    fz_context* clone = fz_clone_context(ctx);
    if (!clone) {
        fprintf(stderr, "fz_clone_context() failed\n");
        return 1;
    }
    int res = 0;
    fz_try(clone) {
        fz_drop_page(clone, fz_load_page(clone, doc, 0));
    }
    fz_catch(clone) {
        fprintf(stderr, "%s: %s\n", path, fz_caught_message(clone));
        // running out of memory may fail the page, but on the caller's context
        res = fault == JsFault::ValueCopyOom ? 0 : 1;
    }
    fz_drop_context(clone);
    fz_drop_document(ctx, doc);
    fz_drop_context(ctx);
    if (res == 0) {
        printf("%s: JavaScript error contained\n", path);
    }
    return res;
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "-js-throw") == 0) {
        return JsFaultTest(argv[2], JsFault::ConsoleThrows);
    }
    if (argc == 3 && strcmp(argv[1], "-js-oom") == 0) {
        return JsFaultTest(argv[2], JsFault::ValueCopyOom);
    }
    if (argc != 2) {
        fprintf(stderr, "usage: test_mupdf [-js-throw | -js-oom] <file>\n");
        return 2;
    }
    fz_context* ctx = fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT);
    if (!ctx) {
        fprintf(stderr, "fz_new_context() failed\n");
        return 1;
    }
    fz_document* doc = nullptr;
    int nPages = 0;
    fz_var(doc);
    fz_try(ctx) {
        fz_register_document_handlers(ctx);
        doc = fz_open_document(ctx, argv[1]);
        nPages = fz_count_pages(ctx, doc);
    }
    fz_always(ctx) {
        fz_drop_document(ctx, doc);
    }
    fz_catch(ctx) {
        fprintf(stderr, "%s: %s\n", argv[1], fz_caught_message(ctx));
        fz_drop_context(ctx);
        return 1;
    }
    printf("%s: %d pages\n", argv[1], nPages);
    fz_drop_context(ctx);
    return 0;
}
