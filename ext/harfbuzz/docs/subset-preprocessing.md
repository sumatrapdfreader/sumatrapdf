# Introduction

Subset preprocessing is a mechanism which can significantly speed up font subsetting operations.
It works by prepopulating datastructures from the source font which can be used in later subsetting
operations to more quickly produce the subset. Preprocessing is useful in cases where multiple subsets
will be cut from the same source font.

# Usage

```c++
hb_face_t* preprocessed = hb_subset_preprocess (source_face);

...

hb_face_t* subset = hb_subset_or_fail (preprocessed, subset_input);
```

# Additional Details

*  A subset produced from a preprocessed face should be identical to a subset produced from only the
   original face. The preprocessor does not change the functionality of the subsetter, just speeds
   things up.

*  The preprocessing operation may take longer than the time it takes to produce a subset from the
   source font. Thus the main performance gains are made when a preprocessed face is reused for
   multiple subsetting operations.

*  Currently the largest performance gains are seen when using a preprocessed face for CFF subsetting.

*  The preprocessed face may contain references to the memory backing the source face. If this memory
   is fully owned by a harfbuzz hb_blob_t* then it will automatically be kept alive for the lifetime
   of the preprocessed face. However, if this memory is not fully owned by a harfbuzz hb_blob_t* then
   it is necessary to ensure that the memory is kept alive for the lifetime of the preprocessed face.


# Performance Improvements

Here is the performance difference of producing a subset with a preprocessed face vs producing
a subset with the source face:

Benchmark | Delta Time (%)
----------|-----------------
BM_subset/subset_glyphs/Roboto-Regular.ttf/10_median|-56%
BM_subset/subset_glyphs/Roboto-Regular.ttf/64_median|-33%
BM_subset/subset_glyphs/Roboto-Regular.ttf/512_median|-28%
BM_subset/subset_glyphs/Roboto-Regular.ttf/1000_median|-11%
BM_subset/subset_glyphs/Roboto-Regular.ttf/nohinting/10_median|-56%
BM_subset/subset_glyphs/Roboto-Regular.ttf/nohinting/64_median|-33%
BM_subset/subset_glyphs/Roboto-Regular.ttf/nohinting/512_median|-21%
BM_subset/subset_glyphs/Roboto-Regular.ttf/nohinting/1000_median|-9%
BM_subset/subset_glyphs/Amiri-Regular.ttf/10_median|-67%
BM_subset/subset_glyphs/Amiri-Regular.ttf/64_median|-48%
BM_subset/subset_glyphs/Amiri-Regular.ttf/512_median|-21%
BM_subset/subset_glyphs/Amiri-Regular.ttf/4096_median|-9%
BM_subset/subset_glyphs/Amiri-Regular.ttf/nohinting/10_median|-66%
BM_subset/subset_glyphs/Amiri-Regular.ttf/nohinting/64_median|-50%
BM_subset/subset_glyphs/Amiri-Regular.ttf/nohinting/512_median|-8%
BM_subset/subset_glyphs/Amiri-Regular.ttf/nohinting/4096_median|-9%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/10_median|-85%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/64_median|-71%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/512_median|-3%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/1400_median|4%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/nohinting/10_median|-84%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/nohinting/64_median|-72%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/nohinting/512_median|0%
BM_subset/subset_glyphs/NotoNastaliqUrdu-Regular.ttf/nohinting/1400_median|0%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/10_median|-30%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/64_median|-24%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/512_median|-3%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/1000_median|-3%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/nohinting/10_median|-30%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/nohinting/64_median|-24%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/nohinting/512_median|-3%
BM_subset/subset_glyphs/NotoSansDevanagari-Regular.ttf/nohinting/1000_median|-5%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/10_median|-96%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/64_median|-90%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/512_median|-74%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/4096_median|-25%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/10000_median|-23%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/nohinting/10_median|-95%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/nohinting/64_median|-90%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/nohinting/512_median|-73%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/nohinting/4096_median|-24%
BM_subset/subset_glyphs/Mplus1p-Regular.ttf/nohinting/10000_median|-11%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/10_median|-84%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/64_median|-77%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/512_median|-70%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/4096_median|-80%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/10000_median|-86%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/nohinting/10_median|-84%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/nohinting/64_median|-78%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/nohinting/512_median|-71%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/nohinting/4096_median|-86%
BM_subset/subset_glyphs/SourceHanSans-Regular_subset.otf/nohinting/10000_median|-88%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/10_median|-59%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/64_median|-55%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/512_median|-67%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/2000_median|-68%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/nohinting/10_median|-60%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/nohinting/64_median|-58%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/nohinting/512_median|-72%
BM_subset/subset_glyphs/SourceSansPro-Regular.otf/nohinting/2000_median|-71%
BM_subset/subset_glyphs/AdobeVFPrototype.otf/10_median|-70%
BM_subset/subset_glyphs/AdobeVFPrototype.otf/64_median|-64%
BM_subset/subset_glyphs/AdobeVFPrototype.otf/300_median|-73%
BM_subset/subset_glyphs/AdobeVFPrototype.otf/nohinting/10_median|-71%
BM_subset/subset_glyphs/AdobeVFPrototype.otf/nohinting/64_median|-68%
BM_subset/subset_glyphs/AdobeVFPrototype.otf/nohinting/300_median|-72%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/10_median|-90%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/64_median|-82%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/512_median|-31%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/4096_median|-9%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/6000_median|-22%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/nohinting/10_median|-88%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/nohinting/64_median|-83%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/nohinting/512_median|-31%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/nohinting/4096_median|-16%
BM_subset/subset_glyphs/MPLUS1-Variable.ttf/nohinting/6000_median|-18%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/10_median|-44%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/64_median|-18%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/512_median|-2%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/900_median|-6%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/nohinting/10_median|-45%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/nohinting/64_median|-17%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/nohinting/512_median|-15%
BM_subset/subset_glyphs/RobotoFlex-Variable.ttf/nohinting/900_median|-3%
BM_subset/subset_codepoints/Roboto-Regular.ttf/10_median|-20%
BM_subset/subset_codepoints/Roboto-Regular.ttf/64_median|-16%
BM_subset/subset_codepoints/Roboto-Regular.ttf/512_median|-12%
BM_subset/subset_codepoints/Roboto-Regular.ttf/1000_median|-10%
BM_subset/subset_codepoints/Roboto-Regular.ttf/nohinting/10_median|-24%
BM_subset/subset_codepoints/Roboto-Regular.ttf/nohinting/64_median|-14%
BM_subset/subset_codepoints/Roboto-Regular.ttf/nohinting/512_median|-15%
BM_subset/subset_codepoints/Roboto-Regular.ttf/nohinting/1000_median|-9%
BM_subset/subset_codepoints/Amiri-Regular.ttf/10_median|-51%
BM_subset/subset_codepoints/Amiri-Regular.ttf/64_median|-37%
BM_subset/subset_codepoints/Amiri-Regular.ttf/512_median|-12%
BM_subset/subset_codepoints/Amiri-Regular.ttf/4096_median|-1%
BM_subset/subset_codepoints/Amiri-Regular.ttf/nohinting/10_median|-49%
BM_subset/subset_codepoints/Amiri-Regular.ttf/nohinting/64_median|-35%
BM_subset/subset_codepoints/Amiri-Regular.ttf/nohinting/512_median|-6%
BM_subset/subset_codepoints/Amiri-Regular.ttf/nohinting/4096_median|-1%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/10_median|-82%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/64_median|-9%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/512_median|0%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/1400_median|0%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/nohinting/10_median|-82%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/nohinting/64_median|-13%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/nohinting/512_median|-3%
BM_subset/subset_codepoints/NotoNastaliqUrdu-Regular.ttf/nohinting/1400_median|2%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/10_median|-40%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/64_median|-26%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/512_median|-5%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/1000_median|3%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/nohinting/10_median|-43%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/nohinting/64_median|-24%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/nohinting/512_median|-2%
BM_subset/subset_codepoints/NotoSansDevanagari-Regular.ttf/nohinting/1000_median|2%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/10_median|-83%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/64_median|-67%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/512_median|-39%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/4096_median|-20%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/10000_median|-25%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/nohinting/10_median|-83%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/nohinting/64_median|-65%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/nohinting/512_median|-42%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/nohinting/4096_median|-34%
BM_subset/subset_codepoints/Mplus1p-Regular.ttf/nohinting/10000_median|-21%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/10_median|-69%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/64_median|-69%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/512_median|-70%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/4096_median|-84%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/10000_median|-83%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/nohinting/10_median|-71%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/nohinting/64_median|-68%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/nohinting/512_median|-70%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/nohinting/4096_median|-86%
BM_subset/subset_codepoints/SourceHanSans-Regular_subset.otf/nohinting/10000_median|-88%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/10_median|-45%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/64_median|-48%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/512_median|-57%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/2000_median|-66%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/nohinting/10_median|-43%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/nohinting/64_median|-50%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/nohinting/512_median|-63%
BM_subset/subset_codepoints/SourceSansPro-Regular.otf/nohinting/2000_median|-72%
BM_subset/subset_codepoints/AdobeVFPrototype.otf/10_median|-69%
BM_subset/subset_codepoints/AdobeVFPrototype.otf/64_median|-66%
BM_subset/subset_codepoints/AdobeVFPrototype.otf/300_median|-74%
BM_subset/subset_codepoints/AdobeVFPrototype.otf/nohinting/10_median|-70%
BM_subset/subset_codepoints/AdobeVFPrototype.otf/nohinting/64_median|-71%
BM_subset/subset_codepoints/AdobeVFPrototype.otf/nohinting/300_median|-75%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/10_median|-66%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/64_median|-46%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/512_median|-15%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/4096_median|-5%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/6000_median|-16%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/nohinting/10_median|-66%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/nohinting/64_median|-45%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/nohinting/512_median|-14%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/nohinting/4096_median|-11%
BM_subset/subset_codepoints/MPLUS1-Variable.ttf/nohinting/6000_median|-27%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/10_median|-38%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/64_median|-9%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/512_median|-3%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/900_median|-16%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/nohinting/10_median|-39%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/nohinting/64_median|-12%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/nohinting/512_median|-4%
BM_subset/subset_codepoints/RobotoFlex-Variable.ttf/nohinting/900_median|-2%
BM_subset/instance/MPLUS1-Variable.ttf/10_median|-68%
BM_subset/instance/MPLUS1-Variable.ttf/64_median|-45%
BM_subset/instance/MPLUS1-Variable.ttf/512_median|-18%
BM_subset/instance/MPLUS1-Variable.ttf/4096_median|-2%
BM_subset/instance/MPLUS1-Variable.ttf/6000_median|4%
BM_subset/instance/MPLUS1-Variable.ttf/nohinting/10_median|-69%
BM_subset/instance/MPLUS1-Variable.ttf/nohinting/64_median|-46%
BM_subset/instance/MPLUS1-Variable.ttf/nohinting/512_median|-11%
BM_subset/instance/MPLUS1-Variable.ttf/nohinting/4096_median|4%
BM_subset/instance/MPLUS1-Variable.ttf/nohinting/6000_median|-5%
BM_subset/instance/RobotoFlex-Variable.ttf/10_median|-34%
BM_subset/instance/RobotoFlex-Variable.ttf/64_median|-12%
BM_subset/instance/RobotoFlex-Variable.ttf/512_median|6%
BM_subset/instance/RobotoFlex-Variable.ttf/900_median|-6%
BM_subset/instance/RobotoFlex-Variable.ttf/nohinting/10_median|-33%
BM_subset/instance/RobotoFlex-Variable.ttf/nohinting/64_median|-11%
BM_subset/instance/RobotoFlex-Variable.ttf/nohinting/512_median|3%
BM_subset/instance/RobotoFlex-Variable.ttf/nohinting/900_median|0%
