/*
 * Copyright © 2020  Google, Inc.
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
 * Google Author(s): Garret Rieger
 */

#include "hb.hh"
#include "hb-priority-queue.hh"

static void
test_insert ()
{
  hb_priority_queue_t queue;
  assert (queue.is_empty ());

  queue.insert (10, 0);
  assert (!queue.is_empty ());
  assert (queue.minimum () == hb_pair (10, 0));

  queue.insert (20, 1);
  assert (queue.minimum () == hb_pair (10, 0));

  queue.insert (5, 2);
  assert (queue.minimum () == hb_pair (5, 2));

  queue.insert (15, 3);
  assert (queue.minimum () == hb_pair (5, 2));

  queue.insert (1, 4);
  assert (queue.minimum () == hb_pair (1, 4));
}

static void
test_extract ()
{
  hb_priority_queue_t queue;
  queue.insert (0, 0);
  queue.insert (60, 6);
  queue.insert (30, 3);
  queue.insert (40 ,4);
  queue.insert (20, 2);
  queue.insert (50, 5);
  queue.insert (70, 7);
  queue.insert (10, 1);

  for (int i = 0; i < 8; i++)
  {
    assert (!queue.is_empty ());
    assert (queue.minimum () == hb_pair (i * 10, i));
    assert (queue.pop_minimum () == hb_pair (i * 10, i));
  }

  assert (queue.is_empty ());
}

int
main (int argc, char **argv)
{
  test_insert ();
  test_extract ();
}
