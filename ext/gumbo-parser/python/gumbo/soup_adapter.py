# Copyright 2012 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Adapter between Gumbo and BeautifulSoup.

This parses an HTML document and gives back a BeautifulSoup object, which you
can then manipulate like a normal BeautifulSoup parse tree.
"""

__author__ = 'jdtang@google.com (Jonathan Tang)'

import BeautifulSoup

import gumboc


def _utf8(text):
  return text.decode('utf-8', 'replace')


def _add_source_info(obj, original_text, start_pos, end_pos):
  obj.original = str(original_text)
  obj.line = start_pos.line
  obj.col = start_pos.column
  obj.offset = start_pos.offset
  if end_pos:
    obj.end_line = end_pos.line
    obj.end_col = end_pos.column
    obj.end_offset = end_pos.offset


def _convert_attrs(attrs):
  # TODO(jdtang): Ideally attributes would pass along their positions as well,
  # but I can't extend the built in str objects with new attributes.  Maybe work
  # around this with a subclass in some way...
  return [(_utf8(attr.name), _utf8(attr.value)) for attr in attrs]


def _add_document(soup, element):
  # Currently ignored, since there's no real place for this in the BeautifulSoup
  # API.
  pass


def _add_element(soup, element):
  # TODO(jdtang): Expose next/previous in gumbo so they can be passed along to
  # BeautifulSoup.
  tag = BeautifulSoup.Tag(
      soup, _utf8(element.tag_name), _convert_attrs(element.attributes))
  for child in element.children:
    tag.append(_add_node(soup, child))
  _add_source_info(
      tag, element.original_tag, element.start_pos, element.end_pos)
  tag.original_end_tag = str(element.original_end_tag)
  return tag


def _add_text(cls):
  def add_text_internal(soup, element):
    text = cls(_utf8(element.text))
    _add_source_info(text, element.original_text, element.start_pos, None)
    return text
  return add_text_internal


_HANDLERS = [
    _add_document,
    _add_element,
    _add_text(BeautifulSoup.NavigableString),
    _add_text(BeautifulSoup.CData),
    _add_text(BeautifulSoup.Comment),
    _add_text(BeautifulSoup.NavigableString),
    _add_element,
    ]


def _add_node(soup, node):
  return _HANDLERS[node.type.value](soup, node.contents)


def _add_next_prev_pointers(soup):
  def _traverse(node):
    # .findAll requires the .next pointer, which is what we're trying to add
    # when we call this, and so we manually supply a generator to yield the
    # nodes in DOM order.
    yield node
    try:
      for child in node.contents:
        for descendant in _traverse(child):
          yield descendant
    except AttributeError:
      # Not an element.
      return

  nodes = sorted(_traverse(soup), key=lambda node: node.offset)
  if nodes:
    nodes[0].previous = None
    nodes[-1].next = None
  for i, node in enumerate(nodes[1:-1], 1):
    nodes[i-1].next = node
    node.previous = nodes[i-1]


def parse(text, **kwargs):
  with gumboc.parse(text, **kwargs) as output:
    soup = BeautifulSoup.BeautifulSoup()
    soup.append(_add_node(soup, output.contents.root.contents))
    _add_next_prev_pointers(soup)
    return soup
