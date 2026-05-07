/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"

#include "GumboHelpers.h"

bool GumboTagNameIs(const GumboNode* node, const char* name) {
    if (node->type != GUMBO_NODE_ELEMENT) {
        return false;
    }
    const char* tag;
    size_t tagLen;
    if (node->v.element.tag != GUMBO_TAG_UNKNOWN) {
        tag = gumbo_normalized_tagname(node->v.element.tag);
        tagLen = str::Len(tag);
    } else {
        const char* s = node->v.element.original_tag.data;
        const char* sentinel = s + node->v.element.original_tag.length;
        if (s < sentinel && s[0] == '<') {
            s++;
        }
        const char* end = s;
        while (end < sentinel && end[0] != '>' && end[0] != '/' && end[0] != ' ' && end[0] != '\t' && end[0] != '\n' &&
               end[0] != '\r') {
            end++;
        }
        tag = s;
        tagLen = (size_t)(end - s);
    }
    return str::EqNIx(tag, tagLen, name);
}

const GumboNode* GumboFindChildByTag(const GumboNode* node, const char* name) {
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

const GumboNode* GumboFindDescendantByTag(const GumboNode* node, const char* name) {
    if (!node) {
        return nullptr;
    }
    const GumboVector* children = nullptr;
    if (node->type == GUMBO_NODE_ELEMENT) {
        if (GumboTagNameIs(node, name)) {
            return node;
        }
        children = &node->v.element.children;
    } else if (node->type == GUMBO_NODE_DOCUMENT) {
        children = &node->v.document.children;
    }
    if (children) {
        for (unsigned int i = 0; i < children->length; i++) {
            const GumboNode* found = GumboFindDescendantByTag((const GumboNode*)children->data[i], name);
            if (found) {
                return found;
            }
        }
    }
    return nullptr;
}

TempStr GumboTextContentTemp(const GumboNode* node) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return nullptr;
    }
    StrBuilder sb;
    const GumboVector* children = &node->v.element.children;
    for (unsigned int i = 0; i < children->length; i++) {
        const GumboNode* child = (const GumboNode*)children->data[i];
        if (child->type == GUMBO_NODE_TEXT || child->type == GUMBO_NODE_WHITESPACE || child->type == GUMBO_NODE_CDATA) {
            sb.Append(child->v.text.text);
        }
    }
    if (sb.IsEmpty()) {
        return nullptr;
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
