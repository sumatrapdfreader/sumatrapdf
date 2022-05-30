/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Joachim Bauch <bauch@struktur.de>
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

#include <sstream>

#include "box.h"
#include "bitstream.h"
#include "logging.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  auto reader = std::make_shared<heif::StreamReader_memory>(data, size, false);

  heif::BitstreamRange range(reader, size);
  for (;;) {
    std::shared_ptr<heif::Box> box;
    heif::Error error = heif::Box::read(range, &box);
    if (error != heif::Error::Ok || range.error()) {
      break;
    }

    box->get_type();
    box->get_type_string();
    heif::Indent indent;
    box->dump(indent);
  }

  return 0;
}
