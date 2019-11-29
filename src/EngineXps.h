/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace XpsEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);
BaseEngine* CreateFromStream(IStream* stream);

} // namespace XpsEngine
