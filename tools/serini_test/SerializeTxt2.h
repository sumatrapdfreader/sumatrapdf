/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef SerializeTxt2_h
#define SerializeTxt2_h

#include "../sertxt_test/SerializeTxt.h"

namespace sertxt {

enum SerializationFormat { Format_Ini, Format_Sqt, Format_Txt, Format_Txt_Sqt, Format_Txt2 };

// TODO: the metadata in SettingsSumatra can't be used for
// multiple serialization schemes simultaneously
void SetSerializeTxtFormat(SerializationFormat format);

};

#endif
