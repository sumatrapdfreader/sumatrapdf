/*
 * Copyright © 2018 Adobe Inc.
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
 * Adobe Author(s): Michiharu Ariza
 */

#include "hb.hh"

#ifndef HB_NO_OT_FONT_CFF

#include "hb-ot-cff2-table.hh"
#include "hb-cff2-interp-cs.hh"

using namespace CFF;

struct cff2_extents_param_t
{
  void init ()
  {
    path_open = false;
    min_x.set_int (INT_MAX);
    min_y.set_int (INT_MAX);
    max_x.set_int (INT_MIN);
    max_y.set_int (INT_MIN);
  }

  void   start_path ()       { path_open = true; }
  void     end_path ()       { path_open = false; }
  bool is_path_open () const { return path_open; }

  void update_bounds (const point_t &pt)
  {
    if (pt.x < min_x) min_x = pt.x;
    if (pt.x > max_x) max_x = pt.x;
    if (pt.y < min_y) min_y = pt.y;
    if (pt.y > max_y) max_y = pt.y;
  }

  bool  path_open;
  number_t min_x;
  number_t min_y;
  number_t max_x;
  number_t max_y;
};

struct cff2_path_procs_extents_t : path_procs_t<cff2_path_procs_extents_t, cff2_cs_interp_env_t, cff2_extents_param_t>
{
  static void moveto (cff2_cs_interp_env_t &env, cff2_extents_param_t& param, const point_t &pt)
  {
    param.end_path ();
    env.moveto (pt);
  }

  static void line (cff2_cs_interp_env_t &env, cff2_extents_param_t& param, const point_t &pt1)
  {
    if (!param.is_path_open ())
    {
      param.start_path ();
      param.update_bounds (env.get_pt ());
    }
    env.moveto (pt1);
    param.update_bounds (env.get_pt ());
  }

  static void curve (cff2_cs_interp_env_t &env, cff2_extents_param_t& param, const point_t &pt1, const point_t &pt2, const point_t &pt3)
  {
    if (!param.is_path_open ())
    {
      param.start_path ();
      param.update_bounds (env.get_pt ());
    }
    /* include control points */
    param.update_bounds (pt1);
    param.update_bounds (pt2);
    env.moveto (pt3);
    param.update_bounds (env.get_pt ());
  }
};

struct cff2_cs_opset_extents_t : cff2_cs_opset_t<cff2_cs_opset_extents_t, cff2_extents_param_t, cff2_path_procs_extents_t> {};

bool OT::cff2::accelerator_t::get_extents (hb_font_t *font,
					   hb_codepoint_t glyph,
					   hb_glyph_extents_t *extents) const
{
#ifdef HB_NO_OT_FONT_CFF
  /* XXX Remove check when this code moves to .hh file. */
  return true;
#endif

  if (unlikely (!is_valid () || (glyph >= num_glyphs))) return false;

  unsigned int fd = fdSelect->get_fd (glyph);
  cff2_cs_interpreter_t<cff2_cs_opset_extents_t, cff2_extents_param_t> interp;
  const byte_str_t str = (*charStrings)[glyph];
  interp.env.init (str, *this, fd, font->coords, font->num_coords);
  cff2_extents_param_t  param;
  param.init ();
  if (unlikely (!interp.interpret (param))) return false;

  if (param.min_x >= param.max_x)
  {
    extents->width = 0;
    extents->x_bearing = 0;
  }
  else
  {
    extents->x_bearing = font->em_scalef_x (param.min_x.to_real ());
    extents->width = font->em_scalef_x (param.max_x.to_real () - param.min_x.to_real ());
  }
  if (param.min_y >= param.max_y)
  {
    extents->height = 0;
    extents->y_bearing = 0;
  }
  else
  {
    extents->y_bearing = font->em_scalef_y (param.max_y.to_real ());
    extents->height = font->em_scalef_y (param.min_y.to_real () - param.max_y.to_real ());
  }

  return true;
}


#endif
