/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SerializeTxt3_h
#define SerializeTxt3_h

struct SettingInfo;

namespace sertxt3 {

void *Deserialize(const char *data, size_t dataLen, SettingInfo *meta);
char *Serialize(const void *data, SettingInfo *meta, size_t *sizeOut=NULL, const char *comment=NULL);
void FreeStruct(void *data, SettingInfo *meta);

};

#endif
