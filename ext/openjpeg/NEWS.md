# OpenJPEG NEWS

More details in the [CHANGELOG](https://github.com/uclouvain/openjpeg/blob/master/CHANGELOG.md)

## OpenJPEG 2.5.3 (Dec 2024)

No API/ABI break compared to v2.5.2

### New Features

* Use TLM \(Tile Length Marker\) segments to optimize decoding [\#1538](https://github.com/uclouvain/openjpeg/pull/1538)
* Add AVX2 and AVX512 optimization [\#1552](https://github.com/uclouvain/openjpeg/pull/1552)
* Support setting enumcs for CMYK and EYCC color space [\#1529](https://github.com/uclouvain/openjpeg/pull/1529)

### Bug fixes

* Do not turn on 'TPsot==TNsot detection fix' when TNsot==1, and add a OPJ_DPARAMETERS_DISABLE_TPSOT_FIX flag to disable it [\#1560](https://github.com/uclouvain/openjpeg/pull/1560)
* opj\_j2k\_setup\_encoder\(\): set numgbits = 1 for Cinema2K [\#1559](https://github.com/uclouvain/openjpeg/pull/1559)
* fix: when EPH markers are specified, they are required. [\#1547](https://github.com/uclouvain/openjpeg/pull/1547)
* sycc422\_to\_rgb\(\): fix out-of-bounds read accesses when 2 \* width\_component\_1\_or\_2 + 1 == with\_component\_0 [\#1566](https://github.com/uclouvain/openjpeg/pull/1566)
* Avoid heap-buffer-overflow read on corrupted image in non-strict mode [\#1536](https://github.com/uclouvain/openjpeg/pull/1536)
* opj\_j2k\_read\_sod\(\): validate opj\_stream\_read\_data\(\) return to avoid potential later heap-buffer-overflow in in opj_t1_decode_cblk when disabling strict mode [\#1534](https://github.com/uclouvain/openjpeg/pull/1534)
* fix integer Overflow at j2k.c:9614 [\#1530](https://github.com/uclouvain/openjpeg/pull/1530)
* Memory leak fixes in error code path of opj\_compress [\#1567](https://github.com/uclouvain/openjpeg/issues/1567)
* opj\_j2k\_decode\_tiles\(\): avoid use of uninitialized l\_current\_tile\_no variable [\#1528](https://github.com/uclouvain/openjpeg/pull/1528)
* Do not allow header length to be zero in non-zero length packet [\#1526](https://github.com/uclouvain/openjpeg/pull/1526)
* Fix building on OpenBSD big endian hosts [\#1520](https://github.com/uclouvain/openjpeg/pull/1520)

### Changes in third party components

* thirdparty/libz: update to zlib-1.3.1 [\#1542](https://github.com/uclouvain/openjpeg/pull/1542)
* thirdparty/libpng: update to libpng-1.6.43 [\#1541](https://github.com/uclouvain/openjpeg/pull/1541)
* thirdparty/libtiff: update to libtiff 4.6.0 [\#1540](https://github.com/uclouvain/openjpeg/pull/1540)

## OpenJPEG 2.5.2 (Feb 2024)

No API/ABI break compared to v2.5.1

* Make sure openjpeg.h includes opj_config.h [\#1514](https://github.com/uclouvain/openjpeg/issues/1514)

## OpenJPEG 2.5.1 (Feb 2024)

No API/ABI break compared to v2.5.0

* CMake: drop support for cmake < 3.5
* Several bugfixes, including [\#1509](https://github.com/uclouvain/openjpeg/pull/1509) for CVE-2021-3575
* Significant speed-up rate allocation by rate/distoratio ratio [\#1440](https://github.com/uclouvain/openjpeg/pull/1440)

## OpenJPEG 2.5.0 (May 2022)

No API/ABI break compared to v2.4.0, but additional symbols for subset of components decoding (hence the MINOR version bump).

* Encoder: add support for generation of TLM markers [\#1359](https://github.com/uclouvain/openjpeg/pull/1359)
* Decoder: add support for high throughput \(HTJ2K\) decoding. [\#1381](https://github.com/uclouvain/openjpeg/pull/1381)
* Decoder: add support for partial bitstream decoding [\#1407](https://github.com/uclouvain/openjpeg/pull/1407)
* Bug fixes (including security fixes)

## OpenJPEG 2.4.0 (December 2020)

No API/ABI break compared to v2.3.1, but additional symbols for subset of components decoding (hence the MINOR version bump).

* Encoder: add support for multithreading [\#1248](https://github.com/uclouvain/openjpeg/pull/1248)
* Encoder: add support for generation of PLT markers [\#1246](https://github.com/uclouvain/openjpeg/pull/1246)
* Encoder: single-threaded performance improvements in forward DWT for 5-3 and 9-7 (and other improvements) [\#1253](https://github.com/uclouvain/openjpeg/pull/1253)
* Encoder: support IMF profiles [\#1235](https://github.com/uclouvain/openjpeg/pull/1235)
* Many bug fixes (including security fixes)

## OpenJPEG 2.3.1 (April 2019)

No API/ABI break compared to v2.3.0

* Many bug fixes (including security fixes)

## OpenJPEG 2.3.0 (October 2017)

No API/ABI break compared to v2.2.0 but additional symbols for subset of components decoding (hence the MINOR version bump).

* Sub-tile decoding: when setting a window of interest through the API function opj_set_decode_area(), only codeblocks that intersect this window are now decoded (i.e. MCT, IDWT, and entropy decoding are only done on the window of interest). Moreover, memory allocation now depends on the size of the window of interest (instead of the full tile size). 
[\#990](https://github.com/uclouvain/openjpeg/pull/990) [\#1001](https://github.com/uclouvain/openjpeg/pull/1001) [\#1010](https://github.com/uclouvain/openjpeg/pull/1010)
* Ability to decode only a subset of components. This adds the following function `opj_set_decoded_components(opj_codec_t p_codec, OPJ_UINT32 numcomps, const OPJ_UINT32 comps_indices, OPJ_BOOL apply_color_transforms)` and equivalent `opj_decompress -c compno[,compno]*` 
option. 
[\#1022](https://github.com/uclouvain/openjpeg/pull/1022)
* Many bug fixes (including security fixes)

## OpenJPEG 2.2.0 (August 2017)

No API/ABI break compared to v2.1.2 but additional symbols for multithreading support (hence the MINOR version bump).

### Codebase improvements

* Memory consumption reduction at decoding side [\#968](https://github.com/uclouvain/openjpeg/pull/968)
* Multi-threading support at decoding side [\#786](https://github.com/uclouvain/openjpeg/pull/786)
* Tier-1 speed optimizations (encoder and decoder) [\#945](https://github.com/uclouvain/openjpeg/pull/945)
* Tier-1 decoder further optimization [\#783](https://github.com/uclouvain/openjpeg/pull/783)
* Inverse 5x3 DWT speed optimization: single-pass lifting and SSE2/AVX2 implementation [\#957](https://github.com/uclouvain/openjpeg/pull/957)
* Fixed a bug that prevented OpenJPEG to compress losslessly in some situations [\#949](https://github.com/uclouvain/openjpeg/pull/949)
* Fixed BYPASS/LAZY, RESTART/TERMALL and PTERM mode switches
* Many other bug fixes (including security fixes)

### Maintenance improvements

* Benchmarking scripts to automatically compare the speed of latest OpenJPEG build with latest release and/or Kakadu binaries [\#917](https://github.com/uclouvain/openjpeg/pull/917)
* CPU and RAM usage profiling scripts [\#918](https://github.com/uclouvain/openjpeg/pull/918)
* Codebase reformatting (with astyle) and scripts to automatically check that new commits comply with formatting guidelines [\#919](https://github.com/uclouvain/openjpeg/pull/919)
* Register OpenJPEG at Google OSS Fuzz initiative, so as to automatically have OpenJPEG tested against Google fuzzer [\#965](https://github.com/uclouvain/openjpeg/issues/965)

## OpenJPEG 2.1.2 (September 2016)

* Bug fixes (including security fixes)
* No API/ABI break compared to v2.1.1

## OpenJPEG 2.1.1 (July 2016)

* Huge amount of critical bugfixes
* Speed improvements
* No API/ABI break compared to v2.1

## OpenJPEG 2.1.0 (April 2014)

### New Features

    * Digital Cinema profiles have been fixed and updated
	* New option to disable MCT if needed
    * extended RAW support: it is now possible to input raw images
	  with subsampled color components (422, 420, etc)
    * New way to deal with profiles
	  
### API/ABI modifications
(see [here](http://www.openjpeg.org/abi-check/timeline/openjpeg/) for details)

    * Removed deprecated functions 
	    * opj_stream_create_default_file_stream(FILE*,...)
        * opj_stream_create_file_stream(FILE*,...)
        * opj_stream_set_user_data (opj_stream_t* p_stream, void * p_data)
	* Added 
        * opj_stream_create_default_file_stream(char*,...)
        * opj_stream_create_file_stream(char*,...)
        * opj_stream_destroy(opj_stream_t*)
        * opj_stream_set_user_data (opj_stream_t* p_stream, void * p_data, 
            ... opj_stream_free_user_data_fn p_function)
        * JPEG 2000 profiles and Part-2 extensions defined through '#define'
    * Changed
        * 'alpha' field added to 'opj_image_comp' structure
        * 'OPJ_CLRSPC_EYCC' added to enum COLOR_SPACE
        * 'OPJ_CLRSPC_CMYK' added to enum COLOR_SPACE
        * 'OPJ_CODEC_JPP' and 'OPJ_CODEC_JPX' added to CODEC_FORMAT
          (not yet used in use)
        * 'max_cs_size' and 'rsiz' fields added to opj_cparameters_t
    
### Misc

    * OpenJPEG is now officially conformant with JPEG 2000 Part-1
	  and will soon become official reference software at the 
	  JPEG committee.
	* Huge amount of bug fixes. See CHANGES for details.


## OpenJPEG 2.0.0

### New Features

    * streaming capabilities
    * merge JP3D

### API modifications
(see [here](http://www.openjpeg.org/abi-check/timeline/openjpeg/) for details)

    * Use a 64bits capable API
    
### Misc

    * removed autotools build system
    * folders hierarchies reorganisation
    * Huge amount of bug fixes. See CHANGES for details.
