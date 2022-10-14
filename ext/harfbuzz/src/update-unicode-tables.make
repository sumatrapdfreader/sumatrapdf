#!/usr/bin/env -S make -f

all: packtab \
	hb-ot-shaper-arabic-joining-list.hh \
	hb-ot-shaper-arabic-table.hh hb-unicode-emoji-table.hh \
	hb-ot-shaper-indic-table.cc hb-ot-tag-table.hh \
	hb-ucd-table.hh hb-ot-shaper-use-table.hh \
	hb-ot-shaper-vowel-constraints.cc

.PHONY: all clean packtab

hb-ot-shaper-arabic-joining-list.hh: gen-arabic-joining-list.py ArabicShaping.txt Scripts.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shaper-arabic-table.hh: gen-arabic-table.py ArabicShaping.txt UnicodeData.txt Blocks.txt
	./$^ > $@ || ($(RM) $@; false)
hb-unicode-emoji-table.hh: gen-emoji-table.py emoji-data.txt emoji-test.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shaper-indic-table.cc: gen-indic-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt Blocks.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-tag-table.hh: gen-tag-table.py languagetags language-subtag-registry
	./$^ > $@ || ($(RM) $@; false)
hb-ucd-table.hh: gen-ucd-table.py ucd.nounihan.grouped.zip hb-common.h
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shaper-use-table.hh: gen-use-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt ArabicShaping.txt DerivedCoreProperties.txt UnicodeData.txt Blocks.txt Scripts.txt ms-use/IndicSyllabicCategory-Additional.txt ms-use/IndicPositionalCategory-Additional.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shaper-vowel-constraints.cc: gen-vowel-constraints.py ms-use/IndicShapingInvalidCluster.txt Scripts.txt
	./$^ > $@ || ($(RM) $@; false)

packtab:
	/usr/bin/env python3 -c "import packTab" 2>/dev/null || /usr/bin/env python3 -m pip install git+https://github.com/harfbuzz/packtab

ArabicShaping.txt Blocks.txt DerivedCoreProperties.txt IndicPositionalCategory.txt IndicSyllabicCategory.txt Scripts.txt UnicodeData.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/$@
emoji-data.txt:
	curl -O https://www.unicode.org/Public/UCD/latest/ucd/emoji/emoji-data.txt
emoji-test.txt:
	curl -O https://www.unicode.org/Public/emoji/latest/emoji-test.txt
languagetags:
	curl -O https://learn.microsoft.com/en-us/typography/opentype/spec/languagetags
language-subtag-registry:
	curl -O https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry
ucd.nounihan.grouped.zip:
	curl -O https://unicode.org/Public/UCD/latest/ucdxml/ucd.nounihan.grouped.zip

clean:
	$(RM) \
		ArabicShaping.txt UnicodeData.txt Blocks.txt emoji-data.txt \
		IndicSyllabicCategory.txt IndicPositionalCategory.txt \
		languagetags language-subtag-registry ucd.nounihan.grouped.zip Scripts.txt
