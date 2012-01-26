// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTF_STRING_CONVERSIONS_H_
#define BASE_UTF_STRING_CONVERSIONS_H_
#pragma once

#include <string>

#include "base/base_export.h"
#include "base/string16.h"
#include "base/string_piece.h"

// These convert between UTF-8, -16, and -32 strings. They are potentially slow,
// so avoid unnecessary conversions. The low-level versions return a boolean
// indicating whether the conversion was 100% valid. In this case, it will still
// do the best it can and put the result in the output buffer. The versions that
// return strings ignore this error and just return the best conversion
// possible.
BASE_EXPORT bool WideToUTF8(const wchar_t* src, size_t src_len,
                            std::string* output);
BASE_EXPORT std::string WideToUTF8(const std::wstring& wide);
BASE_EXPORT bool UTF8ToWide(const char* src, size_t src_len,
                            std::wstring* output);
BASE_EXPORT std::wstring UTF8ToWide(const base::StringPiece& utf8);

BASE_EXPORT bool WideToUTF16(const wchar_t* src, size_t src_len,
                             string16* output);
BASE_EXPORT string16 WideToUTF16(const std::wstring& wide);
BASE_EXPORT bool UTF16ToWide(const char16* src, size_t src_len,
                             std::wstring* output);
BASE_EXPORT std::wstring UTF16ToWide(const string16& utf16);

BASE_EXPORT bool UTF8ToUTF16(const char* src, size_t src_len, string16* output);
BASE_EXPORT string16 UTF8ToUTF16(const base::StringPiece& utf8);
BASE_EXPORT bool UTF16ToUTF8(const char16* src, size_t src_len,
                             std::string* output);
BASE_EXPORT std::string UTF16ToUTF8(const string16& utf16);

// We are trying to get rid of wstring as much as possible, but it's too big
// a mess to do it all at once.  These conversions should be used when we
// really should just be passing a string16 around, but we haven't finished
// porting whatever module uses wstring and the conversion is being used as a
// stopcock.  This makes it easy to grep for the ones that should be removed.
#if defined(OS_WIN)
# define WideToUTF16Hack
# define UTF16ToWideHack
#else
# define WideToUTF16Hack WideToUTF16
# define UTF16ToWideHack UTF16ToWide
#endif

// These convert an ASCII string, typically a hardcoded constant, to a
// UTF16/Wide string.
BASE_EXPORT std::wstring ASCIIToWide(const base::StringPiece& ascii);
BASE_EXPORT string16 ASCIIToUTF16(const base::StringPiece& ascii);

#endif  // BASE_UTF_STRING_CONVERSIONS_H_
