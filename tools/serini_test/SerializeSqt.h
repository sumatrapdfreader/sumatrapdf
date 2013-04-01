/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SerializeSqt_h
#define SerializeSqt_h

struct SettingInfo;

namespace sqt {

void *Deserialize(const char *data, size_t dataLen, const SettingInfo *meta);
char *Serialize(const void *data, const SettingInfo *meta, size_t *sizeOut=NULL, const char *comment=NULL);
void FreeStruct(void *data, const SettingInfo *meta);

};

#endif
