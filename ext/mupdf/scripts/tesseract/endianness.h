#ifdef _WIN32
#define TESSERACT_ENDIAN_DETECT 1
#else
#define TESSERACT_ENDIAN_DETECT 0
#endif

#if defined(__BYTE_ORDER) && __BYTE_ORDER == __BIG_ENDIAN ||	\
	defined(__BIG_ENDIAN__) ||				\
	defined(__ARMEB__) ||					\
	defined(__THUMBEB__) ||					\
	defined(__AARCH64EB__) ||				\
	defined(_MIBSEB) || defined(__MIBSEB) || defined(__MIBSEB__)
// It's a big-endian target architecture
#	define L_BIG_ENDIAN
#elif defined(__BYTE_ORDER) && __BYTE_ORDER == __LITTLE_ENDIAN ||	\
	defined(__LITTLE_ENDIAN__) ||					\
	defined(__ARMEL__) ||						\
	defined(__THUMBEL__) ||						\
	defined(__AARCH64EL__) ||					\
	defined(_MIPSEL) || defined(__MIPSEL) || defined(__MIPSEL__) ||	\
	defined(_M_IX86) || defined(_M_X64) ||				\
	defined(_M_IS64) || defined(_M_ARM) || \
	TESSERACT_ENDIAN_DETECT == 1
// It's a little-endian target architecture
#   define L_LITTLE_ENDIAN
#else
#error "I don't know what architecture this is!"
#endif
