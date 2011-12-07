// 
// Custom include file that I've added to all libctiny source code.
// Here we handle global settings/overrides (e.g. ensuring Unicode is not defined).
// 

#ifndef TOTAL_RECALL_COMMON_MINICRT_LIBCTINY_H__
#define TOTAL_RECALL_COMMON_MINICRT_LIBCTINY_H__

// Make sure WIN32_LEAN_AND_MEAN is defined
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Make sure Unicode is not defined
#ifdef UNICODE
#undef UNICODE
#endif

#ifdef _UNICODE
#undef _UNICODE
#endif

#endif  // TOTAL_RECALL_COMMON_MINICRT_LIBCTINY_H__
