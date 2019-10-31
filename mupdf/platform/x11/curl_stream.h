#ifndef CURL_STREAM_H
#define CURL_STREAM_H
fz_stream *fz_open_file_progressive(fz_context *ctx, const char *filename, int kbps,
	void (*on_data)(void*,int), void *arg);
fz_stream *fz_open_url(fz_context *ctx, const char *url, int kbps,
	void (*on_data)(void*,int), void *arg);
#endif
