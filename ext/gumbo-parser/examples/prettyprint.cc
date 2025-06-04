// Copyright 2015 Kevin B. Hendricks, Stratford, Ontario,  All Rights Reserved.
// loosely based on a greatly simplified version of BeautifulSoup4 decode() routine
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Kevin Hendricks
//
// Prettyprint back to html / xhtml

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <string>

#include "gumbo.h"

static std::string nonbreaking_inline  = "|a|abbr|acronym|b|bdo|big|cite|code|dfn|em|font|i|img|kbd|nobr|s|small|span|strike|strong|sub|sup|tt|";
static std::string empty_tags          = "|area|base|basefont|bgsound|br|command|col|embed|event-source|frame|hr|image|img|input|keygen|link|menuitem|meta|param|source|spacer|track|wbr|";
static std::string preserve_whitespace = "|pre|textarea|script|style|";
static std::string special_handling    = "|html|body|";
static std::string no_entity_sub       = "|script|style|";
static std::string treat_like_inline   = "|p|";

static inline void rtrim(std::string &s) 
{
  s.erase(s.find_last_not_of(" \n\r\t")+1);
}


static inline void ltrim(std::string &s)
{
  s.erase(0,s.find_first_not_of(" \n\r\t"));
}


static void replace_all(std::string &s, const char * s1, const char * s2)
{
  std::string t1(s1);
  size_t len = t1.length();
  size_t pos = s.find(t1);
  while (pos != std::string::npos) {
    s.replace(pos, len, s2);
    pos = s.find(t1, pos + len);
  }
}


static std::string substitute_xml_entities_into_text(const std::string &text)
{
  std::string result = text;
  // replacing & must come first 
  replace_all(result, "&", "&amp;");
  replace_all(result, "<", "&lt;");
  replace_all(result, ">", "&gt;");
  return result;
}


static std::string substitute_xml_entities_into_attributes(char quote, const std::string &text)
{
  std::string result = substitute_xml_entities_into_text(text);
  if (quote == '"') {
    replace_all(result,"\"","&quot;");
  }    
  else if (quote == '\'') {
    replace_all(result,"'","&apos;");
  }
 return result;
}


static std::string handle_unknown_tag(GumboStringPiece *text)
{
  std::string tagname = "";
  if (text->data == NULL) {
    return tagname;
  }
  // work with copy GumboStringPiece to prevent asserts 
  // if try to read same unknown tag name more than once
  GumboStringPiece gsp = *text;
  gumbo_tag_from_original_text(&gsp);
  tagname = std::string(gsp.data, gsp.length);
  return tagname; 
}


static std::string get_tag_name(GumboNode *node)
{
  std::string tagname;
  // work around lack of proper name for document node
  if (node->type == GUMBO_NODE_DOCUMENT) {
    tagname = "document";
  } else {
    tagname = gumbo_normalized_tagname(node->v.element.tag);
  }
  if (tagname.empty()) {
    tagname = handle_unknown_tag(&node->v.element.original_tag);
  }
  return tagname;
}


static std::string build_doctype(GumboNode *node)
{
  std::string results = "";
  if (node->v.document.has_doctype) {
    results.append("<!DOCTYPE ");
    results.append(node->v.document.name);
    std::string pi(node->v.document.public_identifier);
    if ((node->v.document.public_identifier != NULL) && !pi.empty() ) {
        results.append(" PUBLIC \"");
        results.append(node->v.document.public_identifier);
        results.append("\" \"");
        results.append(node->v.document.system_identifier);
        results.append("\"");
    }
    results.append(">\n");
  }
  return results;
}


static std::string build_attributes(GumboAttribute * at, bool no_entities)
{
  std::string atts = "";
  atts.append(" ");
  atts.append(at->name);

  // how do we want to handle attributes with empty values
  // <input type="checkbox" checked />  or <input type="checkbox" checked="" /> 

  if ( (!std::string(at->value).empty())   || 
       (at->original_value.data[0] == '"') || 
       (at->original_value.data[0] == '\'') ) {

    // determine original quote character used if it exists
    char quote = at->original_value.data[0];
    std::string qs = "";
    if (quote == '\'') qs = std::string("'");
    if (quote == '"') qs = std::string("\"");

    atts.append("=");

    atts.append(qs);

    if (no_entities) {
      atts.append(at->value);
    } else {
      atts.append(substitute_xml_entities_into_attributes(quote, std::string(at->value)));
    }

    atts.append(qs);
  }
  return atts;
}


// forward declaration

static std::string prettyprint(GumboNode*, int lvl, const std::string indent_chars);


// prettyprint children of a node
// may be invoked recursively

static std::string prettyprint_contents(GumboNode* node, int lvl, const std::string indent_chars) {

  std::string contents        = "";
  std::string tagname         = get_tag_name(node);
  std::string key             = "|" + tagname + "|";
  bool no_entity_substitution = no_entity_sub.find(key) != std::string::npos;
  bool keep_whitespace        = preserve_whitespace.find(key) != std::string::npos;
  bool is_inline              = nonbreaking_inline.find(key) != std::string::npos;
  bool pp_okay                = !is_inline && !keep_whitespace;

  GumboVector* children = &node->v.element.children;

  for (unsigned int i = 0; i < children->length; ++i) {
    GumboNode* child = static_cast<GumboNode*> (children->data[i]);

    if (child->type == GUMBO_NODE_TEXT) {

      std::string val;

      if (no_entity_substitution) {
        val = std::string(child->v.text.text);
      } else {
        val = substitute_xml_entities_into_text(std::string(child->v.text.text));
      }

      if (pp_okay) rtrim(val);

      if (pp_okay && (contents.length() == 0)) {
        // add required indentation
        char c = indent_chars.at(0);
        int n  = indent_chars.length();
        contents.append(std::string((lvl-1)*n,c));
      }

      contents.append(val);


    } else if ((child->type == GUMBO_NODE_ELEMENT) || (child->type == GUMBO_NODE_TEMPLATE)) {

      std::string val = prettyprint(child, lvl, indent_chars);

      // remove any indentation if this child is inline and not first child
      std::string childname = get_tag_name(child);
      std::string childkey = "|" + childname + "|";
      if ((nonbreaking_inline.find(childkey) != std::string::npos) && (contents.length() > 0)) {
        ltrim(val);
      }

      contents.append(val);

    } else if (child->type == GUMBO_NODE_WHITESPACE) {

      if (keep_whitespace || is_inline) {
        contents.append(std::string(child->v.text.text));
      }

    } else if (child->type != GUMBO_NODE_COMMENT) {

      // Does this actually exist: (child->type == GUMBO_NODE_CDATA)
      fprintf(stderr, "unknown element of type: %d\n", child->type); 

    }

  }

  return contents;
}


// prettyprint a GumboNode back to html/xhtml
// may be invoked recursively

static std::string prettyprint(GumboNode* node, int lvl, const std::string indent_chars) {

  // special case the document node
  if (node->type == GUMBO_NODE_DOCUMENT) {
    std::string results = build_doctype(node);
    results.append(prettyprint_contents(node,lvl+1,indent_chars));
    return results;
  }

  std::string close              = "";
  std::string closeTag           = "";
  std::string atts               = "";
  std::string tagname            = get_tag_name(node);
  std::string key                = "|" + tagname + "|";
  bool need_special_handling     =  special_handling.find(key) != std::string::npos;
  bool is_empty_tag              = empty_tags.find(key) != std::string::npos;
  bool no_entity_substitution    = no_entity_sub.find(key) != std::string::npos;
  bool keep_whitespace           = preserve_whitespace.find(key) != std::string::npos;
  bool is_inline                 = nonbreaking_inline.find(key) != std::string::npos;
  bool inline_like               = treat_like_inline.find(key) != std::string::npos;
  bool pp_okay                   = !is_inline && !keep_whitespace;
  char c                         = indent_chars.at(0);
  int  n                         = indent_chars.length(); 

  // build attr string
  const GumboVector * attribs = &node->v.element.attributes;
  for (int i=0; i< attribs->length; ++i) {
    GumboAttribute* at = static_cast<GumboAttribute*>(attribs->data[i]);
    atts.append(build_attributes(at, no_entity_substitution));
  }

  // determine closing tag type
  if (is_empty_tag) {
      close = "/";
  } else {
      closeTag = "</" + tagname + ">";
  }

  std::string indent_space = std::string((lvl-1)*n,c);

  // prettyprint your contents 
  std::string contents = prettyprint_contents(node, lvl+1, indent_chars);

  if (need_special_handling) {
    rtrim(contents);
    contents.append("\n");
  }

  char last_char = ' ';
  if (!contents.empty()) {
    last_char = contents.at(contents.length()-1);
  } 

  // build results
  std::string results;
  if (pp_okay) {
    results.append(indent_space);
  }
  results.append("<"+tagname+atts+close+">");
  if (pp_okay && !inline_like) {
    results.append("\n");
  }
  if (inline_like) {
    ltrim(contents);
  }
  results.append(contents);
  if (pp_okay && !contents.empty() && (last_char != '\n') && (!inline_like)) {
    results.append("\n");
  }
  if (pp_okay && !inline_like && !closeTag.empty()) {
    results.append(indent_space);
  }
  results.append(closeTag);
  if (pp_okay && !closeTag.empty()) {
    results.append("\n");
  }

  return results;
}


int main(int argc, char** argv) {
  if (argc != 2) {
      std::cout << "prettyprint <html filename>\n";
      exit(EXIT_FAILURE);
  }
  const char* filename = argv[1];

  std::ifstream in(filename, std::ios::in | std::ios::binary);
  if (!in) {
    std::cout << "File " << filename << " not found!\n";
    exit(EXIT_FAILURE);
  }

  std::string contents;
  in.seekg(0, std::ios::end);
  contents.resize(in.tellg());
  in.seekg(0, std::ios::beg);
  in.read(&contents[0], contents.size());
  in.close();
 
  GumboOptions options = kGumboDefaultOptions;

  GumboOutput* output = gumbo_parse_with_options(&options, contents.data(), contents.length());
  std::string indent_chars = "  ";
  std::cout << prettyprint(output->document, 0, indent_chars) << std::endl;
  gumbo_destroy_output(&kGumboDefaultOptions, output);
}
