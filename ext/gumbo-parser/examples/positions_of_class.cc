// Copyright 2013 Google Inc. All Rights Reserved.
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
// Author: jdtang@google.com (Jonathan Tang)
//
// Print out the positions of all elements with a certain CSS class.

#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include "gumbo.h"

static std::string find_line(
    const std::string& original_text, const GumboAttribute& attr) {
  size_t attr_index = attr.original_value.data - original_text.data();
  size_t begin = original_text.rfind("\n", attr_index) + 1;
  size_t end = original_text.find("\n", attr_index);
  if (end != std::string::npos) {
    end--;
  } else {
    end = (size_t) original_text.length() - 1;
  }
  end = std::min(end, attr_index + 40);
  begin = std::max(begin, attr_index - 40);
  return original_text.substr(begin, end - begin);
}

static void search_for_class(
    GumboNode* node, const std::string& original_text, const char* cls_name) {
  if (node->type != GUMBO_NODE_ELEMENT) {
    return;
  }
  GumboAttribute* cls_attr;
  if ((cls_attr = gumbo_get_attribute(&node->v.element.attributes, "class")) &&
      strstr(cls_attr->value, cls_name) != NULL) {
    std::cout << cls_attr->value_start.line << ":"
              << cls_attr->value_start.column << " - "
              << find_line(original_text, *cls_attr) << std::endl;
  }

  GumboVector* children = &node->v.element.children;
  for (int i = 0; i < children->length; ++i) {
    search_for_class(
        static_cast<GumboNode*>(children->data[i]), original_text, cls_name);
  }
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cout << "Usage: positions_of_class <html filename> <CSS classname>.\n";
    exit(EXIT_FAILURE);
  }
  const char* filename = argv[1];
  const char* cls = argv[2];

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

  // If you used contents.c_str(), it'd be harder to match up original
  // positions, because c_str() creates a copy of the string and you can't do
  // pointer arithmetic betweent contents.data() and the original_* pointers.
  GumboOutput* output = gumbo_parse_with_options(
      &kGumboDefaultOptions, contents.data(), contents.length());
  search_for_class(output->root, contents, cls);
  gumbo_destroy_output(&kGumboDefaultOptions, output);
}
