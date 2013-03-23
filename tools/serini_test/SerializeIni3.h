/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SerializeIni3_h
#define SerializeIni3_h

struct SettingInfo;

namespace serini3 {

void *Deserialize(const char *data, size_t dataLen, SettingInfo *meta);
char *Serialize(const void *data, SettingInfo *meta, size_t *sizeOut);
void FreeStruct(void *data, SettingInfo *meta);

};

#endif
