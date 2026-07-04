/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "GumboHelpers.h"

static Str GumboElementTagName(const GumboNode* node) {
    ReportIf(!node || node->type != GUMBO_NODE_ELEMENT);
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    if (node->v.element.tag != GUMBO_TAG_UNKNOWN) {
        return Str(gumbo_normalized_tagname(node->v.element.tag));
    }
    Str orig = Str((char*)node->v.element.original_tag.data, (int)node->v.element.original_tag.length);
    int off = 0;
    if (len(orig) > 0 && orig.s[0] == '<') {
        off = 1;
    }
    int end = off;
    while (end < orig.len && orig.s[end] != '>' && orig.s[end] != '/' && orig.s[end] != ' ' && orig.s[end] != '\t' &&
           orig.s[end] != '\n' && orig.s[end] != '\r') {
        end++;
    }
    return Str(orig.s + off, end - off);
}

bool GumboTagNameIs(const GumboNode* node, Str name) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return false;
    }
    return str::EqI(GumboElementTagName(node), name);
}

bool GumboTagNameIsNS(const GumboNode* node, Str name, Str) {
    // Preserve the old parser's compatibility: namespace URI is ignored,
    // and a prefix in the source tag name is treated as optional.
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return false;
    }
    Str tag = GumboElementTagName(node);
    if (str::EqI(tag, name)) {
        return true;
    }
    Str after;
    if (!str::CutChar(tag, ':', nullptr, &after)) {
        return false;
    }
    return str::EqI(after, name);
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

static const GumboNode* GumboFindDescendantByTagImpl(const GumboNode* node, Str name, Str ns, bool matchNS) {
    // iterative pre-order DFS so a deeply nested document can't overflow the
    // stack (gumbo builds the tree iteratively, but recursing over it doesn't)
    Vec<const GumboNode*> toVisit;
    toVisit.Append(node);
    while (len(toVisit) > 0) {
        const GumboNode* n = toVisit.Pop();
        if (!n) {
            continue;
        }
        const GumboVector* children = nullptr;
        if (n->type == GUMBO_NODE_ELEMENT) {
            bool matches = matchNS ? GumboTagNameIsNS(n, name, ns) : GumboTagNameIs(n, name);
            if (matches) {
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

const GumboNode* GumboFindDescendantByTag(const GumboNode* node, Str name) {
    return GumboFindDescendantByTagImpl(node, name, {}, false);
}

const GumboNode* GumboFindDescendantByTagNS(const GumboNode* node, Str name, Str ns) {
    return GumboFindDescendantByTagImpl(node, name, ns, true);
}

TempStr GumboAttributeValueTemp(const GumboNode* node, const char* name) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    const GumboAttribute* attr = gumbo_get_attribute(&node->v.element.attributes, name);
    if (!attr) {
        return {};
    }
    return str::DupTemp(Str(attr->value));
}

TempStr GumboTextContentTemp(const GumboNode* node) {
    if (!node || node->type != GUMBO_NODE_ELEMENT) {
        return {};
    }
    str::Builder sb;
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
    return ToStrTemp(sb);
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

GumboOptions GumboMakeXmlFragmentOptions() {
    GumboOptions opts = GumboMakeOptions();
    // Gumbo only honors XML-style self-closing syntax for foreign content.
    // Parsing XML-ish metadata as an SVG-namespace fragment keeps <item />
    // and similar EPUB/ComicInfo nodes from swallowing their following siblings.
    opts.fragment_context = GUMBO_TAG_SVG;
    opts.fragment_namespace = GUMBO_NAMESPACE_SVG;
    return opts;
}
