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
"""Tests for the Gumbo's BeautifulSoup Python adapter."""

__author__ = 'jdtang@google.com (Jonathan Tang)'

import unittest

import soup_adapter


class SoupAdapterTest(unittest.TestCase):

  def testSimpleParse(self):
    soup = soup_adapter.parse(
        """
        <ul>
          <li class=odd><a href="one.html">One</a>
          <li class="even"><a href="two.html">Two</a>
          <li class='odd'><a href="three.html">Three</a>
          <li class="even"><a href="four.html">Four</a>
        </ul>
        """)

    head = soup.head
    self.assertEquals(soup, head.parent.parent)
    self.assertEquals(u'head', head.name)
    self.assertEquals(0, len(head))

    body = soup.body
    self.assertEquals(head, body.previousSibling)
    self.assertEquals(2, len(body))  # <ul> + trailing whitespace
    self.assertEquals(u'ul', body.contents[0].name)
    self.assertEquals(body, head.next)
    self.assertEquals(head, body.previous)

    list_items = body.findAll('li')
    self.assertEquals(4, len(list_items))

    evens = body('li', 'even')
    self.assertEquals(2, len(evens))

    a2 = body.find('a', href='two.html')
    self.assertEquals(u'a', a2.name)
    self.assertEquals(u'Two', a2.contents[0])
    self.assertEquals(a2, evens[0].next)
    self.assertEquals(evens[0], a2.previous)

    li2 = a2.parent
    self.assertEquals(u'li', li2.name)
    self.assertEquals(u'even', li2['class'])
    self.assertEquals(list_items[1], li2)
    self.assertEquals(evens[0], li2)

if __name__ == '__main__':
  unittest.main()
