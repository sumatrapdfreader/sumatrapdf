import { join } from "path";

// List of languages we support, their iso codes and id as understood
// by Windows SDK (LANG_* and SUBLANG_*_*).
// See https://msdn.microsoft.com/en-us/library/dd318693.aspx for the full list.
// [code, name, msLangID, optional "RTL"]
const gLangs: string[][] = [
  ["af", "Afrikaans", "_LANGID(LANG_AFRIKAANS)"],
  ["am", "Armenian (Հայերեն)", "_LANGID(LANG_ARMENIAN)"],
  ["ar", "Arabic (الْعَرَبيّة)", "_LANGID(LANG_ARABIC)", "RTL"],
  ["az", "Azerbaijani (Azərbaycanca)", "_LANGID(LANG_AZERI)"],
  ["bg", "Bulgarian (Български)", "_LANGID(LANG_BULGARIAN)"],
  ["bn", "Bengali (বাংলা)", "_LANGID(LANG_BENGALI)"],
  ["co", "Corsican (Corsu)", "_LANGID(LANG_CORSICAN)"],
  ["br", "Portuguese - Brazil (Português)", "MAKELANGID(LANG_PORTUGUESE, SUBLANG_PORTUGUESE_BRAZILIAN)"],
  ["bs", "Bosnian (Bosanski)", "MAKELANGID(LANG_BOSNIAN, SUBLANG_BOSNIAN_BOSNIA_HERZEGOVINA_LATIN)"],
  ["by", "Belarusian (Беларуская)", "_LANGID(LANG_BELARUSIAN)"],
  ["ca", "Catalan (Català)", "_LANGID(LANG_CATALAN)"],
  ["ca-xv", "Catalan-Valencian (Català-Valencià)", "(LANGID)-1"],
  ["cn", "Chinese Simplified (简体中文)", "MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED)"],
  ["cy", "Welsh (Cymraeg)", "_LANGID(LANG_WELSH)"],
  ["cz", "Czech (Čeština)", "_LANGID(LANG_CZECH)"],
  ["de", "German (Deutsch)", "_LANGID(LANG_GERMAN)"],
  ["dk", "Danish (Dansk)", "_LANGID(LANG_DANISH)"],
  ["el", "Greek (Ελληνικά)", "_LANGID(LANG_GREEK)"],
  ["en", "English", "_LANGID(LANG_ENGLISH)"],
  ["es", "Spanish (Español)", "_LANGID(LANG_SPANISH)"],
  ["et", "Estonian (Eesti)", "_LANGID(LANG_ESTONIAN)"],
  ["eu", "Basque (Euskara)", "_LANGID(LANG_BASQUE)"],
  ["fa", "Persian (فارسی)", "_LANGID(LANG_FARSI)", "RTL"],
  ["fi", "Finnish (Suomi)", "_LANGID(LANG_FINNISH)"],
  ["fr", "French (Français)", "_LANGID(LANG_FRENCH)"],
  ["fo", "Faroese (Føroyskt)", "_LANGID(LANG_FAEROESE)"],
  ["fy-nl", "Frisian (Frysk)", "_LANGID(LANG_FRISIAN)"],
  ["ga", "Irish (Gaeilge)", "_LANGID(LANG_IRISH)"],
  ["gl", "Galician (Galego)", "_LANGID(LANG_GALICIAN)"],
  ["he", "Hebrew (עברית)", "_LANGID(LANG_HEBREW)", "RTL"],
  ["hi", "Hindi (हिंदी)", "_LANGID(LANG_HINDI)"],
  ["hr", "Croatian (Hrvatski)", "_LANGID(LANG_CROATIAN)"],
  ["hu", "Hungarian (Magyar)", "_LANGID(LANG_HUNGARIAN)"],
  ["id", "Indonesian (Bahasa Indonesia)", "_LANGID(LANG_INDONESIAN)"],
  ["it", "Italian (Italiano)", "_LANGID(LANG_ITALIAN)"],
  ["ja", "Japanese (日本語)", "_LANGID(LANG_JAPANESE)"],
  ["jv", "Javanese (ꦧꦱꦗꦮ)", "(LANGID)-1"],
  ["ka", "Georgian (ქართული)", "_LANGID(LANG_GEORGIAN)"],
  ["kr", "Korean (한국어)", "_LANGID(LANG_KOREAN)"],
  ["ku", "Kurdish (كوردی)", "MAKELANGID(LANG_CENTRAL_KURDISH, SUBLANG_CENTRAL_KURDISH_CENTRAL_KURDISH_IRAQ)", "RTL"],
  ["kw", "Cornish (Kernewek)", "(LANGID)-1"],
  ["lt", "Lithuanian (Lietuvių)", "_LANGID(LANG_LITHUANIAN)"],
  ["lv", "Latvian (latviešu valoda)", "_LANGID(LANG_LATVIAN)"],
  ["mk", "Macedonian (македонски)", "_LANGID(LANG_MACEDONIAN)"],
  ["ml", "Malayalam (മലയാളം)", "_LANGID(LANG_MALAYALAM)"],
  ["mm", "Burmese (ဗမာ စာ)", "(LANGID)-1"],
  ["my", "Malaysian (Bahasa Melayu)", "_LANGID(LANG_MALAY)"],
  ["ne", "Nepali (नेपाली)", "_LANGID(LANG_NEPALI)"],
  ["nl", "Dutch (Nederlands)", "_LANGID(LANG_DUTCH)"],
  ["nn", "Norwegian Neo-Norwegian (Norsk nynorsk)", "MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_NYNORSK)"],
  ["no", "Norwegian (Norsk)", "MAKELANGID(LANG_NORWEGIAN, SUBLANG_NORWEGIAN_BOKMAL)"],
  ["pa", "Punjabi (ਪੰਜਾਬੀ)", "_LANGID(LANG_PUNJABI)"],
  ["pl", "Polish (Polski)", "_LANGID(LANG_POLISH)"],
  ["pt", "Portuguese - Portugal (Português)", "_LANGID(LANG_PORTUGUESE)"],
  ["ro", "Romanian (Română)", "_LANGID(LANG_ROMANIAN)"],
  ["ru", "Russian (Русский)", "_LANGID(LANG_RUSSIAN)"],
  ["sat", "Santali (ᱥᱟᱱᱛᱟᱲᱤ)", "(LANGID)-1"],
  ["si", "Sinhala (සිංහල)", "_LANGID(LANG_SINHALESE)"],
  ["sk", "Slovak (Slovenčina)", "_LANGID(LANG_SLOVAK)"],
  ["sl", "Slovenian (Slovenščina)", "_LANGID(LANG_SLOVENIAN)"],
  ["sn", "Shona (Shona)", "(LANGID)-1"],
  ["sp-rs", "Serbian (Latin)", "MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_LATIN)"],
  ["sq", "Albanian (Shqip)", "_LANGID(LANG_ALBANIAN)"],
  ["sr-rs", "Serbian (Cyrillic)", "MAKELANGID(LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC)"],
  ["sv", "Swedish (Svenska)", "_LANGID(LANG_SWEDISH)"],
  ["ta", "Tamil (தமிழ்)", "_LANGID(LANG_TAMIL)"],
  ["th", "Thai (ภาษาไทย)", "_LANGID(LANG_THAI)"],
  ["tl", "Tagalog (Tagalog)", "_LANGID(LANG_FILIPINO)"],
  ["tr", "Turkish (Türkçe)", "_LANGID(LANG_TURKISH)"],
  ["tw", "Chinese Traditional (繁體中文)", "MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL)"],
  ["uk", "Ukrainian (Українська)", "_LANGID(LANG_UKRAINIAN)"],
  ["uz", "Uzbek (O'zbek)", "_LANGID(LANG_UZBEK)"],
  ["vn", "Vietnamese (Việt Nam)", "_LANGID(LANG_VIETNAMESE)"],
];

interface Lang {
  desc: string[];
  code: string;
  name: string;
  msLangID: string;
  isRtl: boolean;
}

// escape as octal number for C, as \nnn
function cOct(c: number): string {
  console.assert(c >= 0x80);
  let s = c.toString(8); // base 8 for octal
  while (s.length < 3) {
    s = "0" + s;
  }
  return `\\${s}`;
}

function cEscapeForCompact(txt: string): string {
  if (txt.length === 0) {
    return `"\\0"`;
  }
  // escape all quotes
  txt = txt.replaceAll(`"`, `\\"`);
  // and all non-7-bit characters of the UTF-8 encoded string
  const encoder = new TextEncoder();
  const bytes = encoder.encode(txt);
  let res = "";
  for (const c of bytes) {
    if (c < 0x80) {
      res += String.fromCharCode(c);
    } else {
      res += cOct(c);
    }
  }
  return `"${res}\\0"`;
}

function newLang(desc: string[]): Lang {
  console.assert(desc.length <= 4);
  const lang: Lang = {
    desc,
    code: desc[0],
    name: desc[1],
    msLangID: desc[2],
    isRtl: false,
  };
  if (desc.length > 3) {
    console.assert(desc[3] === "RTL");
    lang.isRtl = true;
  }
  return lang;
}

const compactCTmpl = `/*
 DO NOT EDIT MANUALLY !!!
 Generated with .\\doit.bat -trans-regen
*/

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace trans {

using SeqStrings = const char*; // str-port: generated packed string table base

constexpr int kLangsCount = {{langsCount}};

SeqStrings gLangCodes = {{langcodes}} "\\0"; // str-port: generated packed string table

SeqStrings gLangNames = {{langnames}} "\\0"; // str-port: generated packed string table

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
{{langids}}
};
#undef _LANGID

bool IsLangRtl(int idx)
{
  {{islangrtl}}
}

int gLangsCount = kLangsCount;

const LANGID *GetLangIds() { return &gLangIds[0]; }

} // namespace trans
`;

function genTranslationInfoCpp() {
  // sort: "en" first, then alphabetically by name
  gLangs.sort((x, y) => {
    if (x[0] === "en") return -1;
    if (y[0] === "en") return 1;
    return x[1] < y[1] ? -1 : x[1] > y[1] ? 1 : 0;
  });

  const langs = gLangs.map((desc) => newLang(desc));
  console.assert(langs[0].code === "en");

  console.log(`langs: ${langs.length}, gLangs: ${gLangs.length}`);

  const langcodes = langs.map((lang) => `  ${cEscapeForCompact(lang.code)}`).join(" \\\n");
  console.log(`langcodes: ${langcodes.length} bytes`);

  const langnames = langs.map((lang) => `  ${cEscapeForCompact(lang.name)}`).join(" \\\n");
  console.log(`langnames: ${langnames.length} bytes`);

  const langids = langs.map((lang) => `  ${lang.msLangID}`).join(",\n");
  console.log(`langids: ${langids.length} bytes`);

  const rtlInfo: string[] = [];
  for (let idx = 0; idx < langs.length; idx++) {
    const lang = langs[idx];
    if (!lang.isRtl) continue;
    console.log(`lang rtl: ${lang.code} ${lang.name}`);
    rtlInfo.push(`(${idx} == idx)`);
  }

  let islangrtl: string;
  if (rtlInfo.length === 0) {
    islangrtl = "return false;";
  } else {
    islangrtl = "return " + rtlInfo.join(" || ") + ";";
  }

  const langsCount = langs.length;

  let fileContent = compactCTmpl;
  fileContent = fileContent.replace("{{langsCount}}", String(langsCount));
  fileContent = fileContent.replace("{{langcodes}}", langcodes);
  fileContent = fileContent.replace("{{langnames}}", langnames);
  fileContent = fileContent.replace("{{langids}}", langids);
  fileContent = fileContent.replace("{{islangrtl}}", islangrtl);

  const path = join("src", "TranslationLangs.cpp");
  console.log(`fileContent: path: ${path}, file size: ${fileContent.length}`);
  Bun.write(path, fileContent);
}

genTranslationInfoCpp();
