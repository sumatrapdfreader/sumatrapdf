#!/usr/bin/env -S make -f

all: packtab \
	hb-ot-shape-complex-arabic-joining-list.hh \
	hb-ot-shape-complex-arabic-table.hh hb-unicode-emoji-table.hh \
	hb-ot-shape-complex-indic-table.cc hb-ot-tag-table.hh \
	hb-ucd-table.hh hb-ot-shape-complex-use-table.cc \
	hb-ot-shape-complex-vowel-constraints.cc

.PHONY: all clean packtab

hb-ot-shape-complex-arabic-joining-list.hh: gen-arabic-joining-list.py ArabicShaping.txt Scripts.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shape-complex-arabic-table.hh: gen-arabic-table.py ArabicShaping.txt UnicodeData.txt Blocks.txt
	./$^ > $@ || ($(RM) $@; false)
hb-unicode-emoji-table.hh: gen-emoji-table.py emoji-data.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shape-complex-indic-table.cc: gen-indic-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt Blocks.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-tag-table.hh: gen-tag-table.py languagetags language-subtag-registry
	./$^ > $@ || ($(RM) $@; false)
hb-ucd-table.hh: gen-ucd-table.py ucd.nounihan.grouped.zip hb-common.h
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shape-complex-use-table.cc: gen-use-table.py IndicSyllabicCategory.txt IndicPositionalCategory.txt UnicodeData.txt ArabicShaping.txt Blocks.txt ms-use/IndicSyllabicCategory-Additional.txt ms-use/IndicPositionalCategory-Additional.txt
	./$^ > $@ || ($(RM) $@; false)
hb-ot-shape-complex-vowel-constraints.cc: gen-vowel-constraints.py ms-use/IndicShapingInvalidCluster.txt Scripts.txt
	./$^ > $@ || ($(RM) $@; false)

packtab:
	/usr/bin/env python3 -c "import packTab" 2>/dev/null || /usr/bin/env python3 -m pip install git+https://github.com/harfbuzz/packtab

ArabicShaping.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/ArabicShaping.txt
UnicodeData.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/UnicodeData.txt
Blocks.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/Blocks.txt
emoji-data.txt:
	curl -O https://www.unicode.org/Public/UCD/latest/ucd/emoji/emoji-data.txt
IndicSyllabicCategory.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/IndicSyllabicCategory.txt
IndicPositionalCategory.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/IndicPositionalCategory.txt
languagetags:
	curl -O https://docs.microsoft.com/en-us/typography/opentype/spec/languagetags
language-subtag-registry:
	curl -O https://www.iana.org/assignments/language-subtag-registry/language-subtag-registry
ucd.nounihan.grouped.zip:
	curl -O https://unicode.org/Public/UCD/latest/ucdxml/ucd.nounihan.grouped.zip
Scripts.txt:
	curl -O https://unicode.org/Public/UCD/latest/ucd/Scripts.txt

clean:
	$(RM) \
		ArabicShaping.txt UnicodeData.txt Blocks.txt emoji-data.txt \
		IndicSyllabicCategory.txt IndicPositionalCategory.txt \
		languagetags language-subtag-registry ucd.nounihan.grouped.zip Scripts.txt
