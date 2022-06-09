/*
 * Copyright © 2021  Behdad Esfahbod
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 */

#include "hb.hh"
#include "hb-map.hh"
#include "hb-set.hh"
#include <string>

static const std::string invalid{"invalid"};
static const hb_map_t invalid_map{};
static const hb_set_t invalid_set{};
static const hb_vector_t<unsigned> invalid_vector{};

int
main (int argc, char **argv)
{

  /* Test copy constructor. */
  {
    hb_map_t v1;
    v1.set (1, 2);
    hb_map_t v2 {v1};
    assert (v1.get_population () == 1);
    assert (v2.get_population () == 1);
    assert (v1[1] == 2);
    assert (v2[1] == 2);
  }

  /* Test copy assignment. */
  {
    hb_map_t v1;
    v1.set (1, 2);
    hb_map_t v2 = v1;
    assert (v1.get_population () == 1);
    assert (v2.get_population () == 1);
    assert (v1[1] == 2);
    assert (v2[1] == 2);
  }

  /* Test move constructor. */
  {
    hb_map_t s {};
    s.set (1, 2);
    hb_map_t v (std::move (s));
    assert (s.get_population () == 0);
    assert (v.get_population () == 1);
  }

  /* Test move assignment. */
  {
    hb_map_t s {};
    s.set (1, 2);
    hb_map_t v;
    v = std::move (s);
    assert (s.get_population () == 0);
    assert (v.get_population () == 1);
  }

  /* Test initializing from iterable. */
  {
    hb_map_t s;

    s.set (1, 2);
    s.set (3, 4);

    hb_vector_t<hb_pair_t<hb_codepoint_t, hb_codepoint_t>> v (s);
    hb_map_t v0 (v);
    hb_map_t v1 (s);
    hb_map_t v2 (std::move (s));

    assert (s.get_population () == 0);
    assert (v0.get_population () == 2);
    assert (v1.get_population () == 2);
    assert (v2.get_population () == 2);
  }

  /* Test call fini() twice. */
  {
    hb_map_t s;
    for (int i = 0; i < 16; i++)
      s.set(i, i+1);
    s.fini();
  }

  /* Test initializing from iterator. */
  {
    hb_map_t s;

    s.set (1, 2);
    s.set (3, 4);

    hb_map_t v (hb_iter (s));

    assert (v.get_population () == 2);
  }

  /* Test initializing from initializer list and swapping. */
  {
    using pair_t = hb_pair_t<hb_codepoint_t, hb_codepoint_t>;
    hb_map_t v1 {pair_t{1,2}, pair_t{4,5}};
    hb_map_t v2 {pair_t{3,4}};
    hb_swap (v1, v2);
    assert (v1.get_population () == 1);
    assert (v2.get_population () == 2);
  }

  /* Test class key / value types. */
  {
    hb_hashmap_t<hb_bytes_t, int, std::nullptr_t, int, nullptr, 0> m1;
    hb_hashmap_t<int, hb_bytes_t, int, std::nullptr_t, 0, nullptr> m2;
    hb_hashmap_t<hb_bytes_t, hb_bytes_t, std::nullptr_t, std::nullptr_t, nullptr, nullptr> m3;
    assert (m1.get_population () == 0);
    assert (m2.get_population () == 0);
    assert (m3.get_population () == 0);
  }

  {
    hb_hashmap_t<int, int, int, int, 0, 0> m0;
    hb_hashmap_t<std::string, int, const std::string*, int, &invalid, 0> m1;
    hb_hashmap_t<int, std::string, int, const std::string*, 0, &invalid> m2;
    hb_hashmap_t<std::string, std::string, const std::string*, const std::string*, &invalid, &invalid> m3;

    std::string s;
    for (unsigned i = 1; i < 1000; i++)
    {
      s += "x";
      m0.set (i, i);
      m1.set (s, i);
      m2.set (i, s);
      m3.set (s, s);
    }
  }

  /* Test hashing maps. */
  {
    using pair = hb_pair_t<hb_codepoint_t, hb_codepoint_t>;

    hb_hashmap_t<hb_map_t, hb_map_t, const hb_map_t *, const hb_map_t *, &invalid_map, &invalid_map> m1;
    hb_hashmap_t<hb_map_t, hb_map_t, std::nullptr_t, std::nullptr_t, nullptr, nullptr> m2;

    m1.set (hb_map_t (), hb_map_t {});
    m2.set (hb_map_t (), hb_map_t {});

    m1.set (hb_map_t (), hb_map_t {pair (1u, 2u)});
    m2.set (hb_map_t (), hb_map_t {pair (1u, 2u)});

    m1.set (hb_map_t {pair (1u, 2u)}, hb_map_t {pair (2u, 3u)});
    m2.set (hb_map_t {pair (1u, 2u)}, hb_map_t {pair (2u, 3u)});

    /* Cannot override empty map. */
    assert (m1.get (hb_map_t ()) == hb_map_t ());
    assert (m2.get (hb_map_t ()) == hb_map_t ());

   assert (m1.get (hb_map_t {pair (1u, 2u)}) == hb_map_t {pair (2u, 3u)});
   assert (m2.get (hb_map_t {pair (1u, 2u)}) == hb_map_t {pair (2u, 3u)});
  }

  /* Test hashing sets. */
  {
    hb_hashmap_t<hb_set_t, hb_set_t, const hb_set_t *, const hb_set_t *, &invalid_set, &invalid_set> m1;
    hb_hashmap_t<hb_set_t, hb_set_t, std::nullptr_t, std::nullptr_t, nullptr, nullptr> m2;

    m1.set (hb_set_t (), hb_set_t ());
    m2.set (hb_set_t (), hb_set_t ());

    m1.set (hb_set_t (), hb_set_t {1});
    m2.set (hb_set_t (), hb_set_t {1});

    m1.set (hb_set_t {1, 1000}, hb_set_t {2});
    m2.set (hb_set_t {1, 1000}, hb_set_t {2});

    /* Cannot override empty set. */
    assert (m1.get (hb_set_t ()) == hb_set_t ());
    assert (m2.get (hb_set_t ()) == hb_set_t ());

    assert (m1.get (hb_set_t {1000, 1}) == hb_set_t {2});
    assert (m2.get (hb_set_t {1000, 1}) == hb_set_t {2});
  }

  /* Test hashing vectors. */
  {
    using vector_t = hb_vector_t<unsigned>;

    hb_hashmap_t<vector_t, vector_t, const vector_t *, const vector_t *, &invalid_vector, &invalid_vector> m1;
    hb_hashmap_t<vector_t, vector_t, std::nullptr_t, std::nullptr_t, nullptr, nullptr> m2;

    m1.set (vector_t (), vector_t ());
    m2.set (vector_t (), vector_t ());

    m1.set (vector_t (), vector_t {1});
    m2.set (vector_t (), vector_t {1});

    m1.set (vector_t {1}, vector_t {2});
    m2.set (vector_t {1}, vector_t {2});

    /* Cannot override empty vector. */
    assert (m1.get (vector_t ()) == vector_t ());
    assert (m2.get (vector_t ()) == vector_t ());

    assert (m1.get (vector_t {1}) == vector_t {2});
    assert (m2.get (vector_t {1}) == vector_t {2});
  }

  return 0;
}
