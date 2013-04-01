/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DeserializeBenc_h
#define DeserializeBenc_h

struct SettingInfo;

namespace benc {

void *Deserialize(const char *data, size_t dataLen, const SettingInfo *meta);

};

#endif
