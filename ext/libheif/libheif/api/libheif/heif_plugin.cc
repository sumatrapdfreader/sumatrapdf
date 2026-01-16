/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#include "heif.h"
#include "heif_plugin.h"  // needed to avoid 'unresolved symbols' on Visual Studio compiler

heif_error heif_error_ok = {heif_error_Ok, heif_suberror_Unspecified, "Success"};

heif_error heif_error_unsupported_parameter = {heif_error_Usage_error,
                                                      heif_suberror_Unsupported_parameter,
                                                      "Unsupported encoder parameter"};

heif_error heif_error_invalid_parameter_value = {heif_error_Usage_error,
                                                        heif_suberror_Invalid_parameter_value,
                                                        "Invalid parameter value"};


