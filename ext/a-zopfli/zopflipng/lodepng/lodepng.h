#ifndef LODEPNG_H
#define LODEPNG_H

#include <string.h>

extern const char* LODEPNG_VERSION_STRING;

#ifndef LODEPNG_NO_COMPILE_ZLIB
#define LODEPNG_COMPILE_ZLIB
#endif

#ifndef LODEPNG_NO_COMPILE_PNG
#define LODEPNG_COMPILE_PNG
#endif

#ifndef LODEPNG_NO_COMPILE_DECODER
#define LODEPNG_COMPILE_DECODER
#endif

#ifndef LODEPNG_NO_COMPILE_ENCODER
#define LODEPNG_COMPILE_ENCODER
#endif

#ifndef LODEPNG_NO_COMPILE_DISK
#define LODEPNG_COMPILE_DISK
#endif

#ifndef LODEPNG_NO_COMPILE_ANCILLARY_CHUNKS
#define LODEPNG_COMPILE_ANCILLARY_CHUNKS
#endif

#ifndef LODEPNG_NO_COMPILE_ERROR_TEXT
#define LODEPNG_COMPILE_ERROR_TEXT
#endif

#ifndef LODEPNG_NO_COMPILE_ALLOCATORS
#define LODEPNG_COMPILE_ALLOCATORS
#endif

#ifdef __cplusplus
#ifndef LODEPNG_NO_COMPILE_CPP
#define LODEPNG_COMPILE_CPP
#endif
#endif

#ifdef LODEPNG_COMPILE_CPP
#include <vector>
#include <string>
#endif

#ifdef LODEPNG_COMPILE_PNG

typedef enum LodePNGColorType {
  LCT_GREY = 0,
  LCT_RGB = 2,
  LCT_PALETTE = 3,
  LCT_GREY_ALPHA = 4,
  LCT_RGBA = 6,

  LCT_MAX_OCTET_VALUE = 255
} LodePNGColorType;

#ifdef LODEPNG_COMPILE_DECODER

unsigned lodepng_decode_memory(unsigned char** out, unsigned* w, unsigned* h,
                               const unsigned char* in, size_t insize,
                               LodePNGColorType colortype, unsigned bitdepth);

unsigned lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h,
                          const unsigned char* in, size_t insize);

unsigned lodepng_decode24(unsigned char** out, unsigned* w, unsigned* h,
                          const unsigned char* in, size_t insize);

#ifdef LODEPNG_COMPILE_DISK

unsigned lodepng_decode_file(unsigned char** out, unsigned* w, unsigned* h,
                             const char* filename,
                             LodePNGColorType colortype, unsigned bitdepth);

unsigned lodepng_decode32_file(unsigned char** out, unsigned* w, unsigned* h,
                               const char* filename);

unsigned lodepng_decode24_file(unsigned char** out, unsigned* w, unsigned* h,
                               const char* filename);
#endif
#endif

#ifdef LODEPNG_COMPILE_ENCODER

unsigned lodepng_encode_memory(unsigned char** out, size_t* outsize,
                               const unsigned char* image, unsigned w, unsigned h,
                               LodePNGColorType colortype, unsigned bitdepth);

unsigned lodepng_encode32(unsigned char** out, size_t* outsize,
                          const unsigned char* image, unsigned w, unsigned h);

unsigned lodepng_encode24(unsigned char** out, size_t* outsize,
                          const unsigned char* image, unsigned w, unsigned h);

#ifdef LODEPNG_COMPILE_DISK

unsigned lodepng_encode_file(const char* filename,
                             const unsigned char* image, unsigned w, unsigned h,
                             LodePNGColorType colortype, unsigned bitdepth);

unsigned lodepng_encode32_file(const char* filename,
                               const unsigned char* image, unsigned w, unsigned h);

unsigned lodepng_encode24_file(const char* filename,
                               const unsigned char* image, unsigned w, unsigned h);
#endif
#endif

#ifdef LODEPNG_COMPILE_CPP
namespace lodepng {
#ifdef LODEPNG_COMPILE_DECODER

unsigned decode(std::vector<unsigned char>& out, unsigned& w, unsigned& h,
                const unsigned char* in, size_t insize,
                LodePNGColorType colortype = LCT_RGBA, unsigned bitdepth = 8);
unsigned decode(std::vector<unsigned char>& out, unsigned& w, unsigned& h,
                const std::vector<unsigned char>& in,
                LodePNGColorType colortype = LCT_RGBA, unsigned bitdepth = 8);
#ifdef LODEPNG_COMPILE_DISK

unsigned decode(std::vector<unsigned char>& out, unsigned& w, unsigned& h,
                const std::string& filename,
                LodePNGColorType colortype = LCT_RGBA, unsigned bitdepth = 8);
#endif
#endif

#ifdef LODEPNG_COMPILE_ENCODER

unsigned encode(std::vector<unsigned char>& out,
                const unsigned char* in, unsigned w, unsigned h,
                LodePNGColorType colortype = LCT_RGBA, unsigned bitdepth = 8);
unsigned encode(std::vector<unsigned char>& out,
                const std::vector<unsigned char>& in, unsigned w, unsigned h,
                LodePNGColorType colortype = LCT_RGBA, unsigned bitdepth = 8);
#ifdef LODEPNG_COMPILE_DISK

unsigned encode(const std::string& filename,
                const unsigned char* in, unsigned w, unsigned h,
                LodePNGColorType colortype = LCT_RGBA, unsigned bitdepth = 8);
unsigned encode(const std::string& filename,
                const std::vector<unsigned char>& in, unsigned w, unsigned h,
                LodePNGColorType colortype = LCT_RGBA, unsigned bitdepth = 8);
#endif
#endif
}
#endif
#endif

#ifdef LODEPNG_COMPILE_ERROR_TEXT

const char* lodepng_error_text(unsigned code);
#endif

#ifdef LODEPNG_COMPILE_DECODER

typedef struct LodePNGDecompressSettings LodePNGDecompressSettings;
struct LodePNGDecompressSettings {

  unsigned ignore_adler32;
  unsigned ignore_nlen;

  size_t max_output_size;

  unsigned (*custom_zlib)(unsigned char**, size_t*,
                          const unsigned char*, size_t,
                          const LodePNGDecompressSettings*);

  unsigned (*custom_inflate)(unsigned char**, size_t*,
                             const unsigned char*, size_t,
                             const LodePNGDecompressSettings*);

  const void* custom_context;
};

extern const LodePNGDecompressSettings lodepng_default_decompress_settings;
void lodepng_decompress_settings_init(LodePNGDecompressSettings* settings);
#endif

#ifdef LODEPNG_COMPILE_ENCODER

typedef struct LodePNGCompressSettings LodePNGCompressSettings;
struct LodePNGCompressSettings  {

  unsigned btype;
  unsigned use_lz77;
  unsigned windowsize;
  unsigned minmatch;
  unsigned nicematch;
  unsigned lazymatching;

  unsigned (*custom_zlib)(unsigned char**, size_t*,
                          const unsigned char*, size_t,
                          const LodePNGCompressSettings*);

  unsigned (*custom_deflate)(unsigned char**, size_t*,
                             const unsigned char*, size_t,
                             const LodePNGCompressSettings*);

  const void* custom_context;
};

extern const LodePNGCompressSettings lodepng_default_compress_settings;
void lodepng_compress_settings_init(LodePNGCompressSettings* settings);
#endif

#ifdef LODEPNG_COMPILE_PNG

typedef struct LodePNGColorMode {

  LodePNGColorType colortype;
  unsigned bitdepth;

  unsigned char* palette;
  size_t palettesize;

  unsigned key_defined;
  unsigned key_r;
  unsigned key_g;
  unsigned key_b;
} LodePNGColorMode;

void lodepng_color_mode_init(LodePNGColorMode* info);
void lodepng_color_mode_cleanup(LodePNGColorMode* info);

unsigned lodepng_color_mode_copy(LodePNGColorMode* dest, const LodePNGColorMode* source);

LodePNGColorMode lodepng_color_mode_make(LodePNGColorType colortype, unsigned bitdepth);

void lodepng_palette_clear(LodePNGColorMode* info);

unsigned lodepng_palette_add(LodePNGColorMode* info,
                             unsigned char r, unsigned char g, unsigned char b, unsigned char a);

unsigned lodepng_get_bpp(const LodePNGColorMode* info);

unsigned lodepng_get_channels(const LodePNGColorMode* info);

unsigned lodepng_is_greyscale_type(const LodePNGColorMode* info);

unsigned lodepng_is_alpha_type(const LodePNGColorMode* info);

unsigned lodepng_is_palette_type(const LodePNGColorMode* info);

unsigned lodepng_has_palette_alpha(const LodePNGColorMode* info);

unsigned lodepng_can_have_alpha(const LodePNGColorMode* info);

size_t lodepng_get_raw_size(unsigned w, unsigned h, const LodePNGColorMode* color);

#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS

typedef struct LodePNGTime {
  unsigned year;
  unsigned month;
  unsigned day;
  unsigned hour;
  unsigned minute;
  unsigned second;
} LodePNGTime;
#endif

typedef struct LodePNGInfo {

  unsigned compression_method;
  unsigned filter_method;
  unsigned interlace_method;
  LodePNGColorMode color;

#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS

  unsigned background_defined;
  unsigned background_r;
  unsigned background_g;
  unsigned background_b;

  size_t text_num;
  char** text_keys;
  char** text_strings;

  size_t itext_num;
  char** itext_keys;
  char** itext_langtags;
  char** itext_transkeys;
  char** itext_strings;

  unsigned time_defined;
  LodePNGTime time;

  unsigned phys_defined;
  unsigned phys_x;
  unsigned phys_y;
  unsigned phys_unit;

  unsigned gama_defined;
  unsigned gama_gamma;

  unsigned chrm_defined;
  unsigned chrm_white_x;
  unsigned chrm_white_y;
  unsigned chrm_red_x;
  unsigned chrm_red_y;
  unsigned chrm_green_x;
  unsigned chrm_green_y;
  unsigned chrm_blue_x;
  unsigned chrm_blue_y;

  unsigned srgb_defined;
  unsigned srgb_intent;

  unsigned iccp_defined;
  char* iccp_name;

  unsigned char* iccp_profile;
  unsigned iccp_profile_size;

  unsigned char* unknown_chunks_data[3];
  size_t unknown_chunks_size[3];
#endif
} LodePNGInfo;

void lodepng_info_init(LodePNGInfo* info);
void lodepng_info_cleanup(LodePNGInfo* info);

unsigned lodepng_info_copy(LodePNGInfo* dest, const LodePNGInfo* source);

#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS
unsigned lodepng_add_text(LodePNGInfo* info, const char* key, const char* str);
void lodepng_clear_text(LodePNGInfo* info);

unsigned lodepng_add_itext(LodePNGInfo* info, const char* key, const char* langtag,
                           const char* transkey, const char* str);
void lodepng_clear_itext(LodePNGInfo* info);

unsigned lodepng_set_icc(LodePNGInfo* info, const char* name, const unsigned char* profile, unsigned profile_size);
void lodepng_clear_icc(LodePNGInfo* info);
#endif

unsigned lodepng_convert(unsigned char* out, const unsigned char* in,
                         const LodePNGColorMode* mode_out, const LodePNGColorMode* mode_in,
                         unsigned w, unsigned h);

#ifdef LODEPNG_COMPILE_DECODER

typedef struct LodePNGDecoderSettings {
  LodePNGDecompressSettings zlibsettings;

  unsigned ignore_crc;
  unsigned ignore_critical;
  unsigned ignore_end;

  unsigned color_convert;

#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS
  unsigned read_text_chunks;

  unsigned remember_unknown_chunks;

  size_t max_text_size;

  size_t max_icc_size;
#endif
} LodePNGDecoderSettings;

void lodepng_decoder_settings_init(LodePNGDecoderSettings* settings);
#endif

#ifdef LODEPNG_COMPILE_ENCODER

typedef enum LodePNGFilterStrategy {

  LFS_ZERO = 0,

  LFS_ONE = 1,
  LFS_TWO = 2,
  LFS_THREE = 3,
  LFS_FOUR = 4,

  LFS_MINSUM,

  LFS_ENTROPY,

  LFS_BRUTE_FORCE,

  LFS_PREDEFINED
} LodePNGFilterStrategy;

typedef struct LodePNGColorStats {
  unsigned colored;
  unsigned key;
  unsigned short key_r;
  unsigned short key_g;
  unsigned short key_b;
  unsigned alpha;
  unsigned numcolors;
  unsigned char palette[1024];
  unsigned bits;
  size_t numpixels;

  unsigned allow_palette;
  unsigned allow_greyscale;
} LodePNGColorStats;

void lodepng_color_stats_init(LodePNGColorStats* stats);

unsigned lodepng_compute_color_stats(LodePNGColorStats* stats,
                                     const unsigned char* image, unsigned w, unsigned h,
                                     const LodePNGColorMode* mode_in);

typedef struct LodePNGEncoderSettings {
  LodePNGCompressSettings zlibsettings;

  unsigned auto_convert;

  unsigned filter_palette_zero;

  LodePNGFilterStrategy filter_strategy;

  const unsigned char* predefined_filters;

  unsigned force_palette;
#ifdef LODEPNG_COMPILE_ANCILLARY_CHUNKS

  unsigned add_id;

  unsigned text_compression;
#endif
} LodePNGEncoderSettings;

void lodepng_encoder_settings_init(LodePNGEncoderSettings* settings);
#endif

#if defined(LODEPNG_COMPILE_DECODER) || defined(LODEPNG_COMPILE_ENCODER)

typedef struct LodePNGState {
#ifdef LODEPNG_COMPILE_DECODER
  LodePNGDecoderSettings decoder;
#endif
#ifdef LODEPNG_COMPILE_ENCODER
  LodePNGEncoderSettings encoder;
#endif
  LodePNGColorMode info_raw;
  LodePNGInfo info_png;
  unsigned error;
} LodePNGState;

void lodepng_state_init(LodePNGState* state);
void lodepng_state_cleanup(LodePNGState* state);
void lodepng_state_copy(LodePNGState* dest, const LodePNGState* source);
#endif

#ifdef LODEPNG_COMPILE_DECODER

unsigned lodepng_decode(unsigned char** out, unsigned* w, unsigned* h,
                        LodePNGState* state,
                        const unsigned char* in, size_t insize);

unsigned lodepng_inspect(unsigned* w, unsigned* h,
                         LodePNGState* state,
                         const unsigned char* in, size_t insize);
#endif

unsigned lodepng_inspect_chunk(LodePNGState* state, size_t pos,
                               const unsigned char* in, size_t insize);

#ifdef LODEPNG_COMPILE_ENCODER

unsigned lodepng_encode(unsigned char** out, size_t* outsize,
                        const unsigned char* image, unsigned w, unsigned h,
                        LodePNGState* state);
#endif

unsigned lodepng_chunk_length(const unsigned char* chunk);

void lodepng_chunk_type(char type[5], const unsigned char* chunk);

unsigned char lodepng_chunk_type_equals(const unsigned char* chunk, const char* type);

unsigned char lodepng_chunk_ancillary(const unsigned char* chunk);

unsigned char lodepng_chunk_private(const unsigned char* chunk);

unsigned char lodepng_chunk_safetocopy(const unsigned char* chunk);

unsigned char* lodepng_chunk_data(unsigned char* chunk);
const unsigned char* lodepng_chunk_data_const(const unsigned char* chunk);

unsigned lodepng_chunk_check_crc(const unsigned char* chunk);

void lodepng_chunk_generate_crc(unsigned char* chunk);

unsigned char* lodepng_chunk_next(unsigned char* chunk, unsigned char* end);
const unsigned char* lodepng_chunk_next_const(const unsigned char* chunk, const unsigned char* end);

unsigned char* lodepng_chunk_find(unsigned char* chunk, unsigned char* end, const char type[5]);
const unsigned char* lodepng_chunk_find_const(const unsigned char* chunk, const unsigned char* end, const char type[5]);

unsigned lodepng_chunk_append(unsigned char** out, size_t* outsize, const unsigned char* chunk);

unsigned lodepng_chunk_create(unsigned char** out, size_t* outsize, unsigned length,
                              const char* type, const unsigned char* data);

unsigned lodepng_crc32(const unsigned char* buf, size_t len);
#endif

#ifdef LODEPNG_COMPILE_ZLIB

#ifdef LODEPNG_COMPILE_DECODER

unsigned lodepng_inflate(unsigned char** out, size_t* outsize,
                         const unsigned char* in, size_t insize,
                         const LodePNGDecompressSettings* settings);

unsigned lodepng_zlib_decompress(unsigned char** out, size_t* outsize,
                                 const unsigned char* in, size_t insize,
                                 const LodePNGDecompressSettings* settings);
#endif

#ifdef LODEPNG_COMPILE_ENCODER

unsigned lodepng_zlib_compress(unsigned char** out, size_t* outsize,
                               const unsigned char* in, size_t insize,
                               const LodePNGCompressSettings* settings);

unsigned lodepng_huffman_code_lengths(unsigned* lengths, const unsigned* frequencies,
                                      size_t numcodes, unsigned maxbitlen);

unsigned lodepng_deflate(unsigned char** out, size_t* outsize,
                         const unsigned char* in, size_t insize,
                         const LodePNGCompressSettings* settings);

#endif
#endif

#ifdef LODEPNG_COMPILE_DISK

unsigned lodepng_load_file(unsigned char** out, size_t* outsize, const char* filename);

unsigned lodepng_save_file(const unsigned char* buffer, size_t buffersize, const char* filename);
#endif

#ifdef LODEPNG_COMPILE_CPP

namespace lodepng {
#ifdef LODEPNG_COMPILE_PNG
class State : public LodePNGState {
  public:
    State();
    State(const State& other);
    ~State();
    State& operator=(const State& other);
};

#ifdef LODEPNG_COMPILE_DECODER

unsigned decode(std::vector<unsigned char>& out, unsigned& w, unsigned& h,
                State& state,
                const unsigned char* in, size_t insize);
unsigned decode(std::vector<unsigned char>& out, unsigned& w, unsigned& h,
                State& state,
                const std::vector<unsigned char>& in);
#endif

#ifdef LODEPNG_COMPILE_ENCODER

unsigned encode(std::vector<unsigned char>& out,
                const unsigned char* in, unsigned w, unsigned h,
                State& state);
unsigned encode(std::vector<unsigned char>& out,
                const std::vector<unsigned char>& in, unsigned w, unsigned h,
                State& state);
#endif

#ifdef LODEPNG_COMPILE_DISK

unsigned load_file(std::vector<unsigned char>& buffer, const std::string& filename);

unsigned save_file(const std::vector<unsigned char>& buffer, const std::string& filename);
#endif
#endif

#ifdef LODEPNG_COMPILE_ZLIB
#ifdef LODEPNG_COMPILE_DECODER

unsigned decompress(std::vector<unsigned char>& out, const unsigned char* in, size_t insize,
                    const LodePNGDecompressSettings& settings = lodepng_default_decompress_settings);

unsigned decompress(std::vector<unsigned char>& out, const std::vector<unsigned char>& in,
                    const LodePNGDecompressSettings& settings = lodepng_default_decompress_settings);
#endif

#ifdef LODEPNG_COMPILE_ENCODER

unsigned compress(std::vector<unsigned char>& out, const unsigned char* in, size_t insize,
                  const LodePNGCompressSettings& settings = lodepng_default_compress_settings);

unsigned compress(std::vector<unsigned char>& out, const std::vector<unsigned char>& in,
                  const LodePNGCompressSettings& settings = lodepng_default_compress_settings);
#endif
#endif
}
#endif

#endif
