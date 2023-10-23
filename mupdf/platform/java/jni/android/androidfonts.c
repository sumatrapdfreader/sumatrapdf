// Copyright (C) 2004-2021 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

/* Android font functions */

static fz_font *load_noto(fz_context *ctx, const char *a, const char *b, const char *c, int idx)
{
	char buf[500];
	fz_font *font = NULL;
	fz_try(ctx)
	{
		fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.ttf", a, b, c);
		if (!fz_file_exists(ctx, buf))
			fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.otf", a, b, c);
		if (!fz_file_exists(ctx, buf))
			fz_snprintf(buf, sizeof buf, "/system/fonts/%s%s%s.ttc", a, b, c);
		if (fz_file_exists(ctx, buf))
			font = fz_new_font_from_file(ctx, NULL, buf, idx, 0);
	}
	fz_catch(ctx)
		return NULL;
	return font;
}

static fz_font *load_noto_cjk(fz_context *ctx, int lang)
{
	fz_font *font = load_noto(ctx, "NotoSerif", "CJK", "-Regular", lang);
	if (!font) font = load_noto(ctx, "NotoSans", "CJK", "-Regular", lang);
	if (!font) font = load_noto(ctx, "DroidSans", "Fallback", "", 0);
	return font;
}

static fz_font *load_noto_arabic(fz_context *ctx)
{
	fz_font *font = load_noto(ctx, "Noto", "Naskh", "-Regular", 0);
	if (!font) font = load_noto(ctx, "Noto", "NaskhArabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "Droid", "Naskh", "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSerif", "Arabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSans", "Arabic", "-Regular", 0);
	if (!font) font = load_noto(ctx, "DroidSans", "Arabic", "-Regular", 0);
	return font;
}

static fz_font *load_noto_try(fz_context *ctx, const char *stem)
{
	fz_font *font = load_noto(ctx, "NotoSerif", stem, "-Regular", 0);
	if (!font) font = load_noto(ctx, "NotoSans", stem, "-Regular", 0);
	if (!font) font = load_noto(ctx, "DroidSans", stem, "-Regular", 0);
	return font;
}

enum { JP, KR, SC, TC };

fz_font *load_droid_fallback_font(fz_context *ctx, int script, int language, int serif, int bold, int italic)
{
	switch (script)
	{
	default:
	case UCDN_SCRIPT_COMMON:
	case UCDN_SCRIPT_INHERITED:
	case UCDN_SCRIPT_UNKNOWN:
		return NULL;

	case UCDN_SCRIPT_HANGUL: return load_noto_cjk(ctx, KR);
	case UCDN_SCRIPT_HIRAGANA: return load_noto_cjk(ctx, JP);
	case UCDN_SCRIPT_KATAKANA: return load_noto_cjk(ctx, JP);
	case UCDN_SCRIPT_BOPOMOFO: return load_noto_cjk(ctx, TC);
	case UCDN_SCRIPT_HAN:
		switch (language)
		{
		case FZ_LANG_ja: return load_noto_cjk(ctx, JP);
		case FZ_LANG_ko: return load_noto_cjk(ctx, KR);
		case FZ_LANG_zh_Hans: return load_noto_cjk(ctx, SC);
		default:
		case FZ_LANG_zh_Hant: return load_noto_cjk(ctx, TC);
		}

	case UCDN_SCRIPT_LATIN:
	case UCDN_SCRIPT_GREEK:
	case UCDN_SCRIPT_CYRILLIC:
		return load_noto_try(ctx, "");
	case UCDN_SCRIPT_ARABIC:
		return load_noto_arabic(ctx);

	case UCDN_SCRIPT_ARMENIAN: return load_noto_try(ctx, "Armenian");
	case UCDN_SCRIPT_HEBREW: return load_noto_try(ctx, "Hebrew");
	case UCDN_SCRIPT_SYRIAC: return load_noto_try(ctx, "Syriac");
	case UCDN_SCRIPT_THAANA: return load_noto_try(ctx, "Thaana");
	case UCDN_SCRIPT_DEVANAGARI: return load_noto_try(ctx, "Devanagari");
	case UCDN_SCRIPT_BENGALI: return load_noto_try(ctx, "Bengali");
	case UCDN_SCRIPT_GURMUKHI: return load_noto_try(ctx, "Gurmukhi");
	case UCDN_SCRIPT_GUJARATI: return load_noto_try(ctx, "Gujarati");
	case UCDN_SCRIPT_ORIYA: return load_noto_try(ctx, "Oriya");
	case UCDN_SCRIPT_TAMIL: return load_noto_try(ctx, "Tamil");
	case UCDN_SCRIPT_TELUGU: return load_noto_try(ctx, "Telugu");
	case UCDN_SCRIPT_KANNADA: return load_noto_try(ctx, "Kannada");
	case UCDN_SCRIPT_MALAYALAM: return load_noto_try(ctx, "Malayalam");
	case UCDN_SCRIPT_SINHALA: return load_noto_try(ctx, "Sinhala");
	case UCDN_SCRIPT_THAI: return load_noto_try(ctx, "Thai");
	case UCDN_SCRIPT_LAO: return load_noto_try(ctx, "Lao");
	case UCDN_SCRIPT_TIBETAN: return load_noto_try(ctx, "Tibetan");
	case UCDN_SCRIPT_MYANMAR: return load_noto_try(ctx, "Myanmar");
	case UCDN_SCRIPT_GEORGIAN: return load_noto_try(ctx, "Georgian");
	case UCDN_SCRIPT_ETHIOPIC: return load_noto_try(ctx, "Ethiopic");
	case UCDN_SCRIPT_CHEROKEE: return load_noto_try(ctx, "Cherokee");
	case UCDN_SCRIPT_CANADIAN_ABORIGINAL: return load_noto_try(ctx, "CanadianAboriginal");
	case UCDN_SCRIPT_OGHAM: return load_noto_try(ctx, "Ogham");
	case UCDN_SCRIPT_RUNIC: return load_noto_try(ctx, "Runic");
	case UCDN_SCRIPT_KHMER: return load_noto_try(ctx, "Khmer");
	case UCDN_SCRIPT_MONGOLIAN: return load_noto_try(ctx, "Mongolian");
	case UCDN_SCRIPT_YI: return load_noto_try(ctx, "Yi");
	case UCDN_SCRIPT_OLD_ITALIC: return load_noto_try(ctx, "OldItalic");
	case UCDN_SCRIPT_GOTHIC: return load_noto_try(ctx, "Gothic");
	case UCDN_SCRIPT_DESERET: return load_noto_try(ctx, "Deseret");
	case UCDN_SCRIPT_TAGALOG: return load_noto_try(ctx, "Tagalog");
	case UCDN_SCRIPT_HANUNOO: return load_noto_try(ctx, "Hanunoo");
	case UCDN_SCRIPT_BUHID: return load_noto_try(ctx, "Buhid");
	case UCDN_SCRIPT_TAGBANWA: return load_noto_try(ctx, "Tagbanwa");
	case UCDN_SCRIPT_LIMBU: return load_noto_try(ctx, "Limbu");
	case UCDN_SCRIPT_TAI_LE: return load_noto_try(ctx, "TaiLe");
	case UCDN_SCRIPT_LINEAR_B: return load_noto_try(ctx, "LinearB");
	case UCDN_SCRIPT_UGARITIC: return load_noto_try(ctx, "Ugaritic");
	case UCDN_SCRIPT_SHAVIAN: return load_noto_try(ctx, "Shavian");
	case UCDN_SCRIPT_OSMANYA: return load_noto_try(ctx, "Osmanya");
	case UCDN_SCRIPT_CYPRIOT: return load_noto_try(ctx, "Cypriot");
	case UCDN_SCRIPT_BUGINESE: return load_noto_try(ctx, "Buginese");
	case UCDN_SCRIPT_COPTIC: return load_noto_try(ctx, "Coptic");
	case UCDN_SCRIPT_NEW_TAI_LUE: return load_noto_try(ctx, "NewTaiLue");
	case UCDN_SCRIPT_GLAGOLITIC: return load_noto_try(ctx, "Glagolitic");
	case UCDN_SCRIPT_TIFINAGH: return load_noto_try(ctx, "Tifinagh");
	case UCDN_SCRIPT_SYLOTI_NAGRI: return load_noto_try(ctx, "SylotiNagri");
	case UCDN_SCRIPT_OLD_PERSIAN: return load_noto_try(ctx, "OldPersian");
	case UCDN_SCRIPT_KHAROSHTHI: return load_noto_try(ctx, "Kharoshthi");
	case UCDN_SCRIPT_BALINESE: return load_noto_try(ctx, "Balinese");
	case UCDN_SCRIPT_CUNEIFORM: return load_noto_try(ctx, "Cuneiform");
	case UCDN_SCRIPT_PHOENICIAN: return load_noto_try(ctx, "Phoenician");
	case UCDN_SCRIPT_PHAGS_PA: return load_noto_try(ctx, "PhagsPa");
	case UCDN_SCRIPT_NKO: return load_noto_try(ctx, "NKo");
	case UCDN_SCRIPT_SUNDANESE: return load_noto_try(ctx, "Sundanese");
	case UCDN_SCRIPT_LEPCHA: return load_noto_try(ctx, "Lepcha");
	case UCDN_SCRIPT_OL_CHIKI: return load_noto_try(ctx, "OlChiki");
	case UCDN_SCRIPT_VAI: return load_noto_try(ctx, "Vai");
	case UCDN_SCRIPT_SAURASHTRA: return load_noto_try(ctx, "Saurashtra");
	case UCDN_SCRIPT_KAYAH_LI: return load_noto_try(ctx, "KayahLi");
	case UCDN_SCRIPT_REJANG: return load_noto_try(ctx, "Rejang");
	case UCDN_SCRIPT_LYCIAN: return load_noto_try(ctx, "Lycian");
	case UCDN_SCRIPT_CARIAN: return load_noto_try(ctx, "Carian");
	case UCDN_SCRIPT_LYDIAN: return load_noto_try(ctx, "Lydian");
	case UCDN_SCRIPT_CHAM: return load_noto_try(ctx, "Cham");
	case UCDN_SCRIPT_TAI_THAM: return load_noto_try(ctx, "TaiTham");
	case UCDN_SCRIPT_TAI_VIET: return load_noto_try(ctx, "TaiViet");
	case UCDN_SCRIPT_AVESTAN: return load_noto_try(ctx, "Avestan");
	case UCDN_SCRIPT_EGYPTIAN_HIEROGLYPHS: return load_noto_try(ctx, "EgyptianHieroglyphs");
	case UCDN_SCRIPT_SAMARITAN: return load_noto_try(ctx, "Samaritan");
	case UCDN_SCRIPT_LISU: return load_noto_try(ctx, "Lisu");
	case UCDN_SCRIPT_BAMUM: return load_noto_try(ctx, "Bamum");
	case UCDN_SCRIPT_JAVANESE: return load_noto_try(ctx, "Javanese");
	case UCDN_SCRIPT_MEETEI_MAYEK: return load_noto_try(ctx, "MeeteiMayek");
	case UCDN_SCRIPT_IMPERIAL_ARAMAIC: return load_noto_try(ctx, "ImperialAramaic");
	case UCDN_SCRIPT_OLD_SOUTH_ARABIAN: return load_noto_try(ctx, "OldSouthArabian");
	case UCDN_SCRIPT_INSCRIPTIONAL_PARTHIAN: return load_noto_try(ctx, "InscriptionalParthian");
	case UCDN_SCRIPT_INSCRIPTIONAL_PAHLAVI: return load_noto_try(ctx, "InscriptionalPahlavi");
	case UCDN_SCRIPT_OLD_TURKIC: return load_noto_try(ctx, "OldTurkic");
	case UCDN_SCRIPT_KAITHI: return load_noto_try(ctx, "Kaithi");
	case UCDN_SCRIPT_BATAK: return load_noto_try(ctx, "Batak");
	case UCDN_SCRIPT_BRAHMI: return load_noto_try(ctx, "Brahmi");
	case UCDN_SCRIPT_MANDAIC: return load_noto_try(ctx, "Mandaic");
	case UCDN_SCRIPT_CHAKMA: return load_noto_try(ctx, "Chakma");
	case UCDN_SCRIPT_MIAO: return load_noto_try(ctx, "Miao");
	case UCDN_SCRIPT_MEROITIC_CURSIVE: return load_noto_try(ctx, "Meroitic");
	case UCDN_SCRIPT_MEROITIC_HIEROGLYPHS: return load_noto_try(ctx, "Meroitic");
	case UCDN_SCRIPT_SHARADA: return load_noto_try(ctx, "Sharada");
	case UCDN_SCRIPT_SORA_SOMPENG: return load_noto_try(ctx, "SoraSompeng");
	case UCDN_SCRIPT_TAKRI: return load_noto_try(ctx, "Takri");
	case UCDN_SCRIPT_BASSA_VAH: return load_noto_try(ctx, "BassaVah");
	case UCDN_SCRIPT_CAUCASIAN_ALBANIAN: return load_noto_try(ctx, "CaucasianAlbanian");
	case UCDN_SCRIPT_DUPLOYAN: return load_noto_try(ctx, "Duployan");
	case UCDN_SCRIPT_ELBASAN: return load_noto_try(ctx, "Elbasan");
	case UCDN_SCRIPT_GRANTHA: return load_noto_try(ctx, "Grantha");
	case UCDN_SCRIPT_KHOJKI: return load_noto_try(ctx, "Khojki");
	case UCDN_SCRIPT_KHUDAWADI: return load_noto_try(ctx, "Khudawadi");
	case UCDN_SCRIPT_LINEAR_A: return load_noto_try(ctx, "LinearA");
	case UCDN_SCRIPT_MAHAJANI: return load_noto_try(ctx, "Mahajani");
	case UCDN_SCRIPT_MANICHAEAN: return load_noto_try(ctx, "Manichaean");
	case UCDN_SCRIPT_MENDE_KIKAKUI: return load_noto_try(ctx, "MendeKikakui");
	case UCDN_SCRIPT_MODI: return load_noto_try(ctx, "Modi");
	case UCDN_SCRIPT_MRO: return load_noto_try(ctx, "Mro");
	case UCDN_SCRIPT_NABATAEAN: return load_noto_try(ctx, "Nabataean");
	case UCDN_SCRIPT_OLD_NORTH_ARABIAN: return load_noto_try(ctx, "OldNorthArabian");
	case UCDN_SCRIPT_OLD_PERMIC: return load_noto_try(ctx, "OldPermic");
	case UCDN_SCRIPT_PAHAWH_HMONG: return load_noto_try(ctx, "PahawhHmong");
	case UCDN_SCRIPT_PALMYRENE: return load_noto_try(ctx, "Palmyrene");
	case UCDN_SCRIPT_PAU_CIN_HAU: return load_noto_try(ctx, "PauCinHau");
	case UCDN_SCRIPT_PSALTER_PAHLAVI: return load_noto_try(ctx, "PsalterPahlavi");
	case UCDN_SCRIPT_SIDDHAM: return load_noto_try(ctx, "Siddham");
	case UCDN_SCRIPT_TIRHUTA: return load_noto_try(ctx, "Tirhuta");
	case UCDN_SCRIPT_WARANG_CITI: return load_noto_try(ctx, "WarangCiti");
	case UCDN_SCRIPT_AHOM: return load_noto_try(ctx, "Ahom");
	case UCDN_SCRIPT_ANATOLIAN_HIEROGLYPHS: return load_noto_try(ctx, "AnatolianHieroglyphs");
	case UCDN_SCRIPT_HATRAN: return load_noto_try(ctx, "Hatran");
	case UCDN_SCRIPT_MULTANI: return load_noto_try(ctx, "Multani");
	case UCDN_SCRIPT_OLD_HUNGARIAN: return load_noto_try(ctx, "OldHungarian");
	case UCDN_SCRIPT_SIGNWRITING: return load_noto_try(ctx, "Signwriting");
	case UCDN_SCRIPT_ADLAM: return load_noto_try(ctx, "Adlam");
	case UCDN_SCRIPT_BHAIKSUKI: return load_noto_try(ctx, "Bhaiksuki");
	case UCDN_SCRIPT_MARCHEN: return load_noto_try(ctx, "Marchen");
	case UCDN_SCRIPT_NEWA: return load_noto_try(ctx, "Newa");
	case UCDN_SCRIPT_OSAGE: return load_noto_try(ctx, "Osage");
	case UCDN_SCRIPT_TANGUT: return load_noto_try(ctx, "Tangut");
	case UCDN_SCRIPT_MASARAM_GONDI: return load_noto_try(ctx, "MasaramGondi");
	case UCDN_SCRIPT_NUSHU: return load_noto_try(ctx, "Nushu");
	case UCDN_SCRIPT_SOYOMBO: return load_noto_try(ctx, "Soyombo");
	case UCDN_SCRIPT_ZANABAZAR_SQUARE: return load_noto_try(ctx, "ZanabazarSquare");
	case UCDN_SCRIPT_DOGRA: return load_noto_try(ctx, "Dogra");
	case UCDN_SCRIPT_GUNJALA_GONDI: return load_noto_try(ctx, "GunjalaGondi");
	case UCDN_SCRIPT_HANIFI_ROHINGYA: return load_noto_try(ctx, "HanifiRohingya");
	case UCDN_SCRIPT_MAKASAR: return load_noto_try(ctx, "Makasar");
	case UCDN_SCRIPT_MEDEFAIDRIN: return load_noto_try(ctx, "Medefaidrin");
	case UCDN_SCRIPT_OLD_SOGDIAN: return load_noto_try(ctx, "OldSogdian");
	case UCDN_SCRIPT_SOGDIAN: return load_noto_try(ctx, "Sogdian");
	case UCDN_SCRIPT_ELYMAIC: return load_noto_try(ctx, "Elymaic");
	case UCDN_SCRIPT_NANDINAGARI: return load_noto_try(ctx, "Nandinagari");
	case UCDN_SCRIPT_NYIAKENG_PUACHUE_HMONG: return load_noto_try(ctx, "NyiakengPuachueHmong");
	case UCDN_SCRIPT_WANCHO: return load_noto_try(ctx, "Wancho");
	case UCDN_SCRIPT_CHORASMIAN: return load_noto_try(ctx, "Chorasmian");
	case UCDN_SCRIPT_DIVES_AKURU: return load_noto_try(ctx, "DivesAkuru");
	case UCDN_SCRIPT_KHITAN_SMALL_SCRIPT: return load_noto_try(ctx, "KhitanSmallScript");
	case UCDN_SCRIPT_YEZIDI: return load_noto_try(ctx, "Yezidi");
	case UCDN_SCRIPT_VITHKUQI: return load_noto_try(ctx, "Vithkuqi");
	case UCDN_SCRIPT_OLD_UYGHUR: return load_noto_try(ctx, "OldUyghur");
	case UCDN_SCRIPT_CYPRO_MINOAN: return load_noto_try(ctx, "CyproMinoan");
	case UCDN_SCRIPT_TANGSA: return load_noto_try(ctx, "Tangsa");
	case UCDN_SCRIPT_TOTO: return load_noto_try(ctx, "Toto");
	}
	return NULL;
}

fz_font *load_droid_cjk_font(fz_context *ctx, const char *name, int ros, int serif)
{
	switch (ros)
	{
	case FZ_ADOBE_CNS: return load_noto_cjk(ctx, TC);
	case FZ_ADOBE_GB: return load_noto_cjk(ctx, SC);
	case FZ_ADOBE_JAPAN: return load_noto_cjk(ctx, JP);
	case FZ_ADOBE_KOREA: return load_noto_cjk(ctx, KR);
	}
	return NULL;
}

fz_font *load_droid_font(fz_context *ctx, const char *name, int bold, int italic, int needs_exact_metrics)
{
	return NULL;
}
