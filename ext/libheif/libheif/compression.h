/*
 * HEIF codec.
 * Copyright (c) 2022 Dirk Farin <dirk.farin@gmail.com>
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
#ifndef LIBHEIF_COMPRESSION_H
#define LIBHEIF_COMPRESSION_H

#include <vector>
#include <cinttypes>
#include <cstddef>

#include <error.h>

#if HAVE_ZLIB
/**
 * Compress data using zlib method.
 * 
 * This is the RFC 1950 format.
 * 
 * @param input pointer to the data to be compressed
 * @param size the length of the input array in bytes
 * @return the corresponding compressed data
 */
std::vector<uint8_t> compress_zlib(const uint8_t* input, size_t size);

/**
 * Compress data using deflate method.
 * 
 * This is the RFC 1951 format.
 * 
 * @param input pointer to the data to be compressed
 * @param size the length of the input array in bytes
 * @return the corresponding compressed data
 */
std::vector<uint8_t> compress_deflate(const uint8_t* input, size_t size);

/**
 * Decompress zlib compressed data.
 *
 * This is assumed to be in RFC 1950 format, which is the normal zlib format.
 *
 * @param compressed_input the compressed data to be decompressed
 * @param output pointer to the resulting vector of decompressed data
 * @return success (Ok) or an error on failure (usually corrupt data)
 * 
 * @sa decompress_deflate
 * @sa compress_zlib
 */
Result<std::vector<uint8_t>> decompress_zlib(const std::vector<uint8_t>& compressed_input);

/**
 * Decompress "deflate" compressed data.
 *
 * This is assumed to be in RFC 1951 format, which is the deflate format.
 *
 * @param compressed_input the compressed data to be decompressed
 * @param output pointer to the resulting vector of decompressed data
 * @return success (Ok) or an error on failure (usually corrupt data)
 * 
 * @sa decompress_zlib
 * @sa compress_deflate
 */
Result<std::vector<uint8_t>> decompress_deflate(const std::vector<uint8_t>& compressed_input);

#endif

#if HAVE_BROTLI
/**
 * Decompress Brotli compressed data.
 *
 * Brotli is described at https://brotli.org/
 *
 * @param compressed_input the compressed data to be decompressed
 * @param output pointer to the resulting vector of decompressed data
 * @return success (Ok) or an error on failure (usually corrupt data)
 */
Result<std::vector<uint8_t>> decompress_brotli(const std::vector<uint8_t>& compressed_input);

std::vector<uint8_t> compress_brotli(const uint8_t* input, size_t size);
#endif

#endif //LIBHEIF_COMPRESSION_H
