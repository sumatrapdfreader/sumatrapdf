#ifndef TRACK_USAGE_H
#define TRACK_USAGE_H

#ifdef TRACK_USAGE

typedef struct track_usage_data_s {
	int count;
	const char *function;
	int line;
	const char *desc;
	struct track_usage_data_s *next;
} track_usage_data_t;

#define TRACK_LABEL(A) \
	do { \
		static track_usage_data_t USAGE_DATA = { 0 };\
		track_usage(&USAGE_DATA, __FILE__, __LINE__, A);\
	} while (0)

#define TRACK_FN() \
	do { \
		static track_usage_data_t USAGE_DATA = { 0 };\
		track_usage(&USAGE_DATA, __FILE__, __LINE__, __FUNCTION__);\
	} while (0)

void track_usage(track_usage_data_t *data, const char *function, int line, const char *desc);

#else

#define TRACK_LABEL(A) do { } while (0)
#define TRACK_FN() do { } while (0)

#endif

#endif
