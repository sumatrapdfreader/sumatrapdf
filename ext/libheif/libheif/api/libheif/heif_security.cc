/*
 * HEIF codec.
 * Copyright (c) 2017-2025 Dirk Farin <dirk.farin@gmail.com>
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

#include "heif_security.h"
#include "api_structs.h"

#include "context.h"



const heif_security_limits* heif_get_global_security_limits()
{
  return &global_security_limits;
}


const heif_security_limits* heif_get_disabled_security_limits()
{
  return &disabled_security_limits;
}


heif_security_limits* heif_context_get_security_limits(const heif_context* ctx)
{
  if (!ctx) {
    return nullptr;
  }

  return ctx->context->get_security_limits();
}


heif_error heif_context_set_security_limits(heif_context* ctx, const heif_security_limits* limits)
{
  if (ctx==nullptr || limits==nullptr) {
    return heif_error_null_pointer_argument;
  }

  ctx->context->set_security_limits(limits);

  return heif_error_ok;
}


// DEPRECATED

void heif_context_set_maximum_image_size_limit(heif_context* ctx, int maximum_width)
{
  ctx->context->get_security_limits()->max_image_size_pixels = static_cast<uint64_t>(maximum_width) * maximum_width;
}
