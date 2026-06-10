/*
 * HEIF codec.
 * Copyright (c) 2026 Brad Hards <bradh@frogmouth.net>
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libheif/heif_omaf.h"
#include "api_structs.h"


heif_omaf_image_projection heif_image_handle_get_omaf_image_projection(const heif_image_handle* handle)
{
  return handle->image->get_omaf_image_projection();
}

void heif_image_handle_set_omaf_image_projection(heif_image_handle* handle,
                                                 heif_omaf_image_projection image_projection)
{
  handle->image->set_omaf_image_projection(image_projection);
}


heif_omaf_image_projection heif_image_get_omaf_image_projection(const heif_image* image)
{
  return image->image->get_omaf_image_projection();
}

void heif_image_set_omaf_image_projection(heif_image* image,
                                          heif_omaf_image_projection image_projection)
{
  image->image->set_omaf_image_projection(image_projection);
}
