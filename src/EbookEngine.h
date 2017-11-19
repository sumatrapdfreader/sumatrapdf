/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace EpubEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);
BaseEngine* CreateFromStream(IStream* stream);

} // namespace EpubEngine

namespace Fb2Engine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);
BaseEngine* CreateFromStream(IStream* stream);

} // namespace Fb2Engine

namespace MobiEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);
BaseEngine* CreateFromStream(IStream* stream);

} // namespace MobiEngine

namespace PdbEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);

} // namespace PdbEngine

namespace ChmEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);

} // namespace ChmEngine

namespace HtmlEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);

} // namespace HtmlEngine

namespace TxtEngine {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false);
BaseEngine* CreateFromFile(const WCHAR* fileName);

} // namespace TxtEngine

void SetDefaultEbookFont(const WCHAR* name, float size);
