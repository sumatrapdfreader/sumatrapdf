#include "zlib.h"

extern "C"
{

static int (*__deflateInit_)(z_streamp strm, int level, const char *version, int stream_size);
int deflateInit_(z_streamp strm, int level, const char *version, int stream_size) {   return __deflateInit_(strm,level,version,stream_size); } 
static int (*__deflateReset)(z_streamp strm);
int deflateReset(z_streamp strm) {   return __deflateReset(strm); } 
static int (*__deflateEnd)(z_streamp strm);
int deflateEnd(z_streamp strm) {   return __deflateEnd(strm); } 
static int (*__deflate)(z_streamp strm, int);
int deflate(z_streamp strm, int fl) {   return __deflate(strm,fl); } 

static int (*__inflateInit_)(z_streamp strm, const char *version, int stream_size);
int inflateInit_(z_streamp strm, const char *version, int stream_size) {   return __inflateInit_(strm,version,stream_size); } 
static int (*__inflateReset)(z_streamp strm);
int inflateReset(z_streamp strm) {   return __inflateReset(strm); } 
static int (*__inflateEnd)(z_streamp strm);
int inflateEnd(z_streamp strm) {   return __inflateEnd(strm); } 
static int (*__inflate)(z_streamp strm, int);
int inflate(z_streamp strm, int fl) {   return __inflate(strm,fl); } 

};


#define ZLIB_IMPORT_ALL(IMPORT_FUNC) \
  IMPORT_FUNC(inflateInit_) \
  IMPORT_FUNC(inflateReset) \
  IMPORT_FUNC(inflate) \
  IMPORT_FUNC(inflateEnd) \
  IMPORT_FUNC(deflateInit_)  \
  IMPORT_FUNC(deflateReset) \
  IMPORT_FUNC(deflate) \
  IMPORT_FUNC(deflateEnd)
