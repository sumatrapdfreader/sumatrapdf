// Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
// License: Apache-2.0 (matches the rest of gumbo-parser)
//
// SumatraPDF-local additions to gumbo-parser. Kept in a separate file so the
// vendored sources stay unpatched.
//
// gumbo's built-in destroy_node() (parser.c) recurses once per nesting level.
// A pathologically deep tree -- e.g. thousands of unclosed/nested tags in a
// malformed MOBI/EPUB/CHM -- overflows the stack while freeing. These
// replacements free the tree with an explicit work-list, so stack usage stays
// constant regardless of document depth. The work-list only ever holds the
// unprocessed frontier, which for a deep chain is tiny.

#include "gumbo.h"
#include "parser.h"
#include "attribute.h"
#include "error.h"
#include "util.h"
#include "vector.h"

void gumbo_destroy_node_iter(GumboOptions* options, GumboNode* node) {
  // Need a dummy GumboParser because the allocator comes along with the
  // options object (same trick as gumbo_destroy_node).
  GumboParser parser;
  parser._options = options;

  GumboVector stack;
  gumbo_vector_init(&parser, 10, &stack);
  gumbo_vector_add(&parser, node, &stack);
  while (stack.length > 0) {
    GumboNode* n = (GumboNode*) gumbo_vector_pop(&parser, &stack);
    switch (n->type) {
      case GUMBO_NODE_DOCUMENT: {
        GumboDocument* doc = &n->v.document;
        for (unsigned int i = 0; i < doc->children.length; ++i) {
          gumbo_vector_add(&parser, doc->children.data[i], &stack);
        }
        gumbo_parser_deallocate(&parser, (void*) doc->children.data);
        gumbo_parser_deallocate(&parser, (void*) doc->name);
        gumbo_parser_deallocate(&parser, (void*) doc->public_identifier);
        gumbo_parser_deallocate(&parser, (void*) doc->system_identifier);
      } break;
      case GUMBO_NODE_TEMPLATE:
      case GUMBO_NODE_ELEMENT:
        for (unsigned int i = 0; i < n->v.element.attributes.length; ++i) {
          gumbo_destroy_attribute(&parser, n->v.element.attributes.data[i]);
        }
        gumbo_parser_deallocate(&parser, n->v.element.attributes.data);
        for (unsigned int i = 0; i < n->v.element.children.length; ++i) {
          gumbo_vector_add(&parser, n->v.element.children.data[i], &stack);
        }
        gumbo_parser_deallocate(&parser, n->v.element.children.data);
        break;
      case GUMBO_NODE_TEXT:
      case GUMBO_NODE_CDATA:
      case GUMBO_NODE_COMMENT:
      case GUMBO_NODE_WHITESPACE:
        gumbo_parser_deallocate(&parser, (void*) n->v.text.text);
        break;
    }
    gumbo_parser_deallocate(&parser, n);
  }
  gumbo_vector_destroy(&parser, &stack);
}

void gumbo_destroy_output_iter(const GumboOptions* options, GumboOutput* output) {
  GumboParser parser;
  parser._options = options;
  gumbo_destroy_node_iter((GumboOptions*) options, output->document);
  for (unsigned int i = 0; i < output->errors.length; ++i) {
    gumbo_error_destroy(&parser, output->errors.data[i]);
  }
  gumbo_vector_destroy(&parser, &output->errors);
  gumbo_parser_deallocate(&parser, output);
}
