/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "GumboHelpers.h"

bool GumboTagNameIs(const GumboNode* node, Str name) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        return false;
    }
    Str tag;
    if (node->v.element.tag != GUMBO_TAG_UNKNOWN) {
        tag = Str(gumbo_normalized_tagname(node->v.element.tag));
    } else {
        Str orig = AsStr(ByteSlice((const u8*)node->v.element.original_tag.data, node->v.element.original_tag.length));
        int off = 0;
        if (!str::IsEmpty(orig) && orig.s[0] == '<') {
            off = 1;
        }
        int end = off;
        while (end < orig.len && orig.s[end] != '>' && orig.s[end] != '/' && orig.s[end] != ' ' &&
               orig.s[end] != '\t' && orig.s[end] != '\n' && orig.s[end] != '\r') {
            end++;
        }
        tag = Str(orig.s + off, end - off);
    }
    return str::EqI(tag, name);
}

const GumboNode* GumboFindChildByTag(const GumboNode* node, Str name) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (GumboTagNameIs(child, name)) {
            return child;
        }
    }
    return nullptr;
}

const GumboNode* GumboFindDescendantByTag(const GumboNode* node, Str name) {
    // iterative pre-order DFS so a deeply nested document can't overflow the
    // stack (gumbo builds the tree iteratively, but recursing over it doesn't)
    Vec<const GumboNode*> toVisit;
    toVisit.Append(node);
    while (toVisit.size() > 0) {
        const GumboNode* n = toVisit.Pop();
        if (!n) {
            continue;
        }
        const GumboVector* children = nullptr;
        if (n->type == GUMBO_NODE_ELEMENT) {
            if (GumboTagNameIs(n, name)) {
                return n;
            }
            children = &n->v.element.children;
        } else if (n->type == GUMBO_NODE_DOCUMENT) {
            children = &n->v.document.children;
        }
        if (children) {
            // push in reverse so children are visited in document order
            for (unsigned int i = children->length; i > 0; i--) {
                toVisit.Append((const GumboNode*)children->data[i - 1]);
            }
        }
    }
    return nullptr;
}

TempStr GumboTextContentTemp(const GumboNode* node) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    StrBuilder sb;
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_WHITESPACE || child->type == GUMBO_NODE_CDATA) {
            sb.Append(Str(child->v.text.text));
        }
    }
    if (sb.IsEmpty()) {
        return {};
    }
    return str::DupTemp(sb.LendData());
}

static void* GumboMallocWrapper(void*, size_t size) {
    return malloc(size);
}
static void GumboFreeWrapper(void*, void* ptr) {
    free(ptr);
}

GumboOptions GumboMakeOptions() {
    GumboOptions opts{};
    opts.allocator = GumboMallocWrapper;
    opts.deallocator = GumboFreeWrapper;
    opts.userdata = nullptr;
    opts.tab_stop = 8;
    opts.stop_on_first_error = false;
    opts.max_errors = -1;
    opts.fragment_context = GUMBO_TAG_LAST;
    opts.fragment_namespace = GUMBO_NAMESPACE_HTML;
    return opts;
}