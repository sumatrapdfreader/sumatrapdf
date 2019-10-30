/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2012,2013  Google, Inc.
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
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_FACE_HH
#define HB_OT_FACE_HH

#include "hb.hh"

#include "hb-machinery.hh"


#define hb_ot_face_data(face) ((hb_ot_face_data_t *) face->shaper_data.ot.get_relaxed ())


/*
 * hb_ot_face_data_t
 */

/* Most of these tables are NOT needed for shaping.  But we need to hook them *somewhere*.
 * This is as good as any place. */
#define HB_OT_LAYOUT_TABLES \
    /* OpenType shaping. */ \
    HB_OT_LAYOUT_TABLE(OT, JSTF) \
    HB_OT_LAYOUT_TABLE(OT, BASE) \
    /* AAT shaping. */ \
    HB_OT_LAYOUT_TABLE(AAT, morx) \
    HB_OT_LAYOUT_TABLE(AAT, kerx) \
    HB_OT_LAYOUT_TABLE(AAT, ankr) \
    HB_OT_LAYOUT_TABLE(AAT, trak) \
    /* OpenType variations. */ \
    HB_OT_LAYOUT_TABLE(OT, fvar) \
    HB_OT_LAYOUT_TABLE(OT, avar) \
    HB_OT_LAYOUT_TABLE(OT, MVAR) \
    /* OpenType math. */ \
    HB_OT_LAYOUT_TABLE(OT, MATH) \
    /* OpenType fundamentals. */ \
    HB_OT_LAYOUT_ACCELERATOR(OT, GDEF) \
    HB_OT_LAYOUT_ACCELERATOR(OT, GSUB) \
    HB_OT_LAYOUT_ACCELERATOR(OT, GPOS) \
    HB_OT_LAYOUT_ACCELERATOR(OT, cmap) \
    HB_OT_LAYOUT_ACCELERATOR(OT, hmtx) \
    HB_OT_LAYOUT_ACCELERATOR(OT, vmtx) \
    HB_OT_LAYOUT_ACCELERATOR(OT, post) \
    HB_OT_LAYOUT_ACCELERATOR(OT, kern) \
    HB_OT_LAYOUT_ACCELERATOR(OT, glyf) \
    HB_OT_LAYOUT_ACCELERATOR(OT, CBDT) \
    /* */

/* Declare tables. */
#define HB_OT_LAYOUT_TABLE(Namespace, Type) namespace Namespace { struct Type; }
#define HB_OT_LAYOUT_ACCELERATOR(Namespace, Type) HB_OT_LAYOUT_TABLE (Namespace, Type##_accelerator_t)
HB_OT_LAYOUT_TABLES
#undef HB_OT_LAYOUT_ACCELERATOR
#undef HB_OT_LAYOUT_TABLE

struct hb_ot_face_data_t
{
  HB_INTERNAL void init0 (hb_face_t *face);
  HB_INTERNAL void fini (void);

#define HB_OT_LAYOUT_TABLE_ORDER(Namespace, Type) \
    HB_PASTE (ORDER_, HB_PASTE (Namespace, HB_PASTE (_, Type)))
  enum order_t
  {
    ORDER_ZERO,
#define HB_OT_LAYOUT_TABLE(Namespace, Type) HB_OT_LAYOUT_TABLE_ORDER (Namespace, Type),
#define HB_OT_LAYOUT_ACCELERATOR(Namespace, Type) HB_OT_LAYOUT_TABLE (Namespace, Type)
    HB_OT_LAYOUT_TABLES
#undef HB_OT_LAYOUT_ACCELERATOR
#undef HB_OT_LAYOUT_TABLE
  };

  hb_face_t *face; /* MUST be JUST before the lazy loaders. */
#define HB_OT_LAYOUT_TABLE(Namespace, Type) \
  hb_table_lazy_loader_t<Namespace::Type, HB_OT_LAYOUT_TABLE_ORDER (Namespace, Type)> Type;
#define HB_OT_LAYOUT_ACCELERATOR(Namespace, Type) \
  hb_face_lazy_loader_t<Namespace::Type##_accelerator_t, HB_OT_LAYOUT_TABLE_ORDER (Namespace, Type)> Type;
  HB_OT_LAYOUT_TABLES
#undef HB_OT_LAYOUT_ACCELERATOR
#undef HB_OT_LAYOUT_TABLE
};


HB_INTERNAL hb_ot_face_data_t *
_hb_ot_face_data_create (hb_face_t *face);

HB_INTERNAL void
_hb_ot_face_data_destroy (hb_ot_face_data_t *data);


#endif /* HB_OT_FACE_HH */
