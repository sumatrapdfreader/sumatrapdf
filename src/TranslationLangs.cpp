/*
 DO NOT EDIT MANUALLY !!!
 Generated with .\doit.bat -trans-regen
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace trans {

constexpr int kLangsCount = 73;

const char *gLangCodes =   "en\0" \
  "af\0" \
  "sq\0" \
  "ar\0" \
  "am\0" \
  "az\0" \
  "eu\0" \
  "by\0" \
  "bn\0" \
  "bs\0" \
  "bg\0" \
  "mm\0" \
  "ca\0" \
  "ca-xv\0" \
  "cn\0" \
  "tw\0" \
  "kw\0" \
  "co\0" \
  "hr\0" \
  "cz\0" \
  "dk\0" \
  "nl\0" \
  "et\0" \
  "fo\0" \
  "fi\0" \
  "fr\0" \
  "fy-nl\0" \
  "gl\0" \
  "ka\0" \
  "de\0" \
  "el\0" \
  "he\0" \
  "hi\0" \
  "hu\0" \
  "id\0" \
  "ga\0" \
  "it\0" \
  "ja\0" \
  "jv\0" \
  "kr\0" \
  "ku\0" \
  "lv\0" \
  "lt\0" \
  "mk\0" \
  "ml\0" \
  "my\0" \
  "ne\0" \
  "no\0" \
  "nn\0" \
  "fa\0" \
  "pl\0" \
  "br\0" \
  "pt\0" \
  "pa\0" \
  "ro\0" \
  "ru\0" \
  "sat\0" \
  "sr-rs\0" \
  "sp-rs\0" \
  "sn\0" \
  "si\0" \
  "sk\0" \
  "sl\0" \
  "es\0" \
  "sv\0" \
  "tl\0" \
  "ta\0" \
  "th\0" \
  "tr\0" \
  "uk\0" \
  "uz\0" \
  "vn\0" \
  "cy\0" "\0";

const char *gLangNames =   "English\0" \
  "Afrikaans\0" \
  "Albanian (Shqip)\0" \
  "Arabic (\330\247\331\204\331\222\330\271\331\216\330\261\331\216\330\250\331\212\331\221\330\251)\0" \
  "Armenian (\325\200\325\241\325\265\325\245\326\200\325\245\325\266)\0" \
  "Azerbaijani (Az\311\231rbaycanca)\0" \
  "Basque (Euskara)\0" \
  "Belarusian (\320\221\320\265\320\273\320\260\321\200\321\203\321\201\320\272\320\260\321\217)\0" \
  "Bengali (\340\246\254\340\246\276\340\246\202\340\246\262\340\246\276)\0" \
  "Bosnian (Bosanski)\0" \
  "Bulgarian (\320\221\321\212\320\273\320\263\320\260\321\200\321\201\320\272\320\270)\0" \
  "Burmese (\341\200\227\341\200\231\341\200\254 \341\200\205\341\200\254)\0" \
  "Catalan (Catal\303\240)\0" \
  "Catalan-Valencian (Catal\303\240-Valenci\303\240)\0" \
  "Chinese Simplified (\347\256\200\344\275\223\344\270\255\346\226\207)\0" \
  "Chinese Traditional (\347\271\201\351\253\224\344\270\255\346\226\207)\0" \
  "Cornish (Kernewek)\0" \
  "Corsican (Corsu)\0" \
  "Croatian (Hrvatski)\0" \
  "Czech (\304\214e\305\241tina)\0" \
  "Danish (Dansk)\0" \
  "Dutch (Nederlands)\0" \
  "Estonian (Eesti)\0" \
  "Faroese (F\303\270royskt)\0" \
  "Finnish (Suomi)\0" \
  "French (Fran\303\247ais)\0" \
  "Frisian (Frysk)\0" \
  "Galician (Galego)\0" \
  "Georgian (\341\203\245\341\203\220\341\203\240\341\203\227\341\203\243\341\203\232\341\203\230)\0" \
  "German (Deutsch)\0" \
  "Greek (\316\225\316\273\316\273\316\267\316\275\316\271\316\272\316\254)\0" \
  "Hebrew (\327\242\327\221\327\250\327\231\327\252)\0" \
  "Hindi (\340\244\271\340\244\277\340\244\202\340\244\246\340\245\200)\0" \
  "Hungarian (Magyar)\0" \
  "Indonesian (Bahasa Indonesia)\0" \
  "Irish (Gaeilge)\0" \
  "Italian (Italiano)\0" \
  "Japanese (\346\227\245\346\234\254\350\252\236)\0" \
  "Javanese (\352\246\247\352\246\261\352\246\227\352\246\256)\0" \
  "Korean (\355\225\234\352\265\255\354\226\264)\0" \
  "Kurdish (\331\203\331\210\330\261\330\257\333\214)\0" \
  "Latvian (latvie\305\241u valoda)\0" \
  "Lithuanian (Lietuvi\305\263)\0" \
  "Macedonian (\320\274\320\260\320\272\320\265\320\264\320\276\320\275\321\201\320\272\320\270)\0" \
  "Malayalam (\340\264\256\340\264\262\340\264\257\340\264\276\340\264\263\340\264\202)\0" \
  "Malaysian (Bahasa Melayu)\0" \
  "Nepali (\340\244\250\340\245\207\340\244\252\340\244\276\340\244\262\340\245\200)\0" \
  "Norwegian (Norsk)\0" \
  "Norwegian Neo-Norwegian (Norsk nynorsk)\0" \
  "Persian (\331\201\330\247\330\261\330\263\333\214)\0" \
  "Polish (Polski)\0" \
  "Portuguese - Brazil (Portugu\303\252s)\0" \
  "Portuguese - Portugal (Portugu\303\252s)\0" \
  "Punjabi (\340\250\252\340\251\260\340\250\234\340\250\276\340\250\254\340\251\200)\0" \
  "Romanian (Rom\303\242n\304\203)\0" \
  "Russian (\320\240\321\203\321\201\321\201\320\272\320\270\320\271)\0" \
  "Santali (\341\261\245\341\261\237\341\261\261\341\261\233\341\261\237\341\261\262\341\261\244)\0" \
  "Serbian (Cyrillic)\0" \
  "Serbian (Latin)\0" \
  "Shona (Shona)\0" \
  "Sinhala (\340\267\203\340\267\222\340\266\202\340\267\204\340\266\275)\0" \
  "Slovak (Sloven\304\215ina)\0" \
  "Slovenian (Sloven\305\241\304\215ina)\0" \
  "Spanish (Espa\303\261ol)\0" \
  "Swedish (Svenska)\0" \
  "Tagalog (Tagalog)\0" \
  "Tamil (\340\256\244\340\256\256\340\256\277\340\256\264\340\257\215)\0" \
  "Thai (\340\270\240\340\270\262\340\270\251\340\270\262\340\271\204\340\270\227\340\270\242)\0" \
  "Turkish (T\303\274rk\303\247e)\0" \
  "Ukrainian (\320\243\320\272\321\200\320\260\321\227\320\275\321\201\321\214\320\272\320\260)\0" \
  "Uzbek (O'zbek)\0" \
  "Vietnamese (Vi\341\273\207t Nam)\0" \
  "Welsh (Cymraeg)\0" "\0";

// from https://msdn.microsoft.com/en-us/library/windows/desktop/dd318693(v=vs.85).aspx
// those definition are not present in 7.0A SDK my VS 2010 uses
#ifndef LANG_CENTRAL_KURDISH
#define LANG_CENTRAL_KURDISH 0x92
#endif

#ifndef SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ
#define SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ 0x01
#endif

#define _LANGID(lang) MAKELANGID(lang, SUBLANG_NEUTRAL)
const LANGID gLangIds[kLangsCount] = {
  _LANGID(LANG_ENGLISH),
  _LANGID(LANG_AFRIKAANS),
  _LANGID(LANG_ALBANIAN),
  _LANGID(LANG_ARABIC),
  _LANGID(LANG_ARMENIAN),
  _LANGID(LANG_AZERI),
  _LANGID(LANG_BASQUE),
  _LANGID(LANG_BELARUSIAN),
  _LANGID(LANG_BENGALI),
  MAKELANGID(LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN),
  _LANGID(LANG_BULGARIAN),
  (LANGID)-1,
  _LANGID(LANG_CATALAN),
  (LANGID)-1,
  MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
  MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL),
  (LANGID)-1,
  _LANGID(LANG_CORSICAN),
  _LANGID(LANG_CROATIAN),
  _LANGID(LANG_CZECH),
  _LANGID(LANG_DANISH),
  _LANGID(LANG_DUTCH),
  _LANGID(LANG_ESTONIAN),
  _LANGID(LANG_FAEROESE),
  _LANGID(LANG_FINNISH),
  _LANGID(LANG_FRENCH),
  _LANGID(LANG_FRISIAN),
  _LANGID(LANG_GALICIAN),
  _LANGID(LANG_GEORGIAN),
  _LANGID(LANG_GERMAN),
  _LANGID(LANG_GREEK),
  _LANGID(LANG_HEBREW),
  _LANGID(LANG_HINDI),
  _LANGID(LANG_HUNGARIAN),
  _LANGID(LANG_INDONESIAN),
  _LANGID(LANG_IRISH),
  _LANGID(LANG_ITALIAN),
  _LANGID(LANG_JAPANESE),
  (LANGID)-1,
  _LANGID(LANG_KOREAN),
  MAKELANGID(LANG_CENTRAL_KURDISH, SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ),
  _LANGID(LANG_LATVIAN),
  _LANGID(LANG_LITHUANIAN),
  _LANGID(LANG_MACEDONIAN),
  _LANGID(LANG_MALAYALAM),
  _LANGID(LANG_MALAY),
  _LANGID(LANG_NEPALI),
  MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL),
  MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK),
  _LANGID(LANG_FARSI),
  _LANGID(LANG_POLISH),
  MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN),
  _LANGID(LANG_PORTUGUESE),
  _LANGID(LANG_PUNJABI),
  _LANGID(LANG_ROMANIAN),
  _LANGID(LANG_RUSSIAN),
  (LANGID)-1,
  MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC),
  MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN),
  (LANGID)-1,
  _LANGID(LANG_SINHALESE),
  _LANGID(LANG_SLOVAK),
  _LANGID(LANG_SLOVENIAN),
  _LANGID(LANG_SPANISH),
  _LANGID(LANG_SWEDISH),
  _LANGID(LANG_FILIPINO),
  _LANGID(LANG_TAMIL),
  _LANGID(LANG_THAI),
  _LANGID(LANG_TURKISH),
  _LANGID(LANG_UKRAINIAN),
  _LANGID(LANG_UZBEK),
  _LANGID(LANG_VIETNAMESE),
  _LANGID(LANG_WELSH)
};
#undef _LANGID

bool IsLangRtl(int idx)
{
  return (3 == idx) || (31 == idx) || (40 == idx) || (49 == idx);
}

int gLangsCount = kLangsCount;

const LANGID *GetLangIds() { return &gLangIds[0]; }

} // namespace trans
