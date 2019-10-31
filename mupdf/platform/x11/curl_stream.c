#include "mupdf/fitz.h"
#include "curl_stream.h"

#include <assert.h>
#include <string.h>
#include <ctype.h>

#include <curl/curl.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

#undef DEBUG_BLOCK_FETCHING

#ifdef DEBUG_BLOCK_FETCHING
#ifdef _WIN32
#include <varargs.h>
static void
output(const char *fmt, ...)
{
	va_list args;
	char text[256];

	va_start(args, fmt);
	vsnprintf(text, sizeof(text), fmt, args);
	va_end(args);

	OutputDebugString(text);
}
#else
#define output printf
#endif

#define DEBUG_MESSAGE(A) do { output A; } while(0)
#else
#define DEBUG_MESSAGE(A) do { } while(0)
#endif

#define BLOCK_SHIFT 18
#define BLOCK_SIZE (1<<BLOCK_SHIFT)

#define HAVE_BLOCK(map, num) (((map)[(num)>>3] & (1<<((num) & 7))) != 0)

typedef struct curlstate
{
	fz_context *ctx;
	CURL *easy;

	/* START: The following entries are protected by the lock */
	CURLcode curl_error;
	int data_arrived;
	int complete;
	int kill_thread;
	int accept_ranges;
	int head;

	/* content buffer */
	size_t content_length; /* 0 => Unknown length */
	unsigned char *buffer;
	size_t buffer_fill;
	size_t buffer_max;

	/* map of which blocks we have */
	unsigned char *map;
	size_t map_length;

	/* outstanding curl request info */
	size_t next_fill_start; /* The next file offset we will fetch to */
	size_t current_fill_start; /* The current file offset we are fetching to */
	size_t current_fill_end;
	/* END: The above entries are protected by the lock */

	void (*more_data)(void *,int);
	void *more_data_arg;

	unsigned char public_buffer[4096];

	/* We assume either Windows threads or pthreads here. */
#ifdef _WIN32
	void *thread;
	DWORD thread_id;
	HANDLE mutex;
#else
	pthread_t thread;
	pthread_mutex_t mutex;
#endif
} curlstate;

#ifdef _WIN32
static int locked;

static void
lock(curlstate *state)
{
	WaitForSingleObject(state->mutex, INFINITE);
	assert(locked == 0);
	locked = 1;
}

static void
unlock(curlstate *state)
{
	assert(locked == 1);
	locked = 0;
	ReleaseMutex(state->mutex);
}
#else
static void
lock(curlstate *state)
{
	pthread_mutex_lock(&state->mutex);
}

static void
unlock(curlstate *state)
{
	pthread_mutex_unlock(&state->mutex);
}
#endif

static size_t on_curl_header(void *ptr, size_t size, size_t nmemb, void *state_)
{
	struct curlstate *state = state_;

	lock(state);
	if (strncmp(ptr, "Accept-Ranges: bytes", 20) == 0)
	{
		DEBUG_MESSAGE(("header arrived with Accept-Ranges!\n"));
		state->accept_ranges = 1;
	}

	if (strncmp(ptr, "Content-Length:", 15) == 0)
	{
		char *s = ptr;
		state->content_length = fz_atoi(s + 15);
		DEBUG_MESSAGE(("header arrived with Content-Length: %d\n", state->content_length));
	}
	unlock(state);

	return nmemb * size;
}

static size_t on_curl_data(void *ptr, size_t size, size_t nmemb, void *state_)
{
	struct curlstate *state = state_;
	size_t old_start;

	size *= nmemb;

	lock(state);
	if (state->data_arrived == 0)
	{
		/* This is the first time data has arrived.
		 * If the header has Accept-Ranges then we can do byte requests.
		 * We know the Content-Length from having processed the header already.
		 */
		if (state->content_length == 0)
		{
			/* What a crap server. Won't tell us how big the file
			 * is. We'll have to expand as data as arrives. */
			DEBUG_MESSAGE(("have no length!\n"));
		}
		else if (state->accept_ranges)
		{
			/* We got a range header, and the correct http response
			 * code. We can assume that byte fetches are accepted
			 * and we'll run without progressive mode. */
			size_t len = state->content_length;
			state->map_length = (len+BLOCK_SIZE-1)>>BLOCK_SHIFT;
			state->map = fz_malloc_no_throw(state->ctx, (state->map_length+7)>>3);
			state->buffer = fz_malloc_no_throw(state->ctx, len);
			state->buffer_max = len;
			if (state->map == NULL || state->buffer == NULL)
			{
				unlock(state);
				return 0;
			}
			memset(state->map, 0, (state->map_length+7)>>3);
			DEBUG_MESSAGE(("have range header content_length=%d!\n", state->content_length));
		}
		else
		{
			/* We know the length, and that we can use ByteRanges -
			 * we can run as a progressive file. */
			state->buffer = fz_malloc_no_throw(state->ctx, state->content_length);
			if (state->buffer == NULL)
			{
				unlock(state);
				return 0;
			}
			state->buffer_max = state->content_length;
		}

		state->data_arrived = 1;
	}

	if (state->content_length == 0)
	{
		size_t newsize = (state->current_fill_start + size);
		if (newsize > state->buffer_max)
		{
			/* Expand the buffer */
			size_t new_max = state->buffer_max * 2;
			if (new_max == 0)
				new_max = 4096;
			fz_try(state->ctx)
				state->buffer = fz_realloc_array(state->ctx, state->buffer, new_max, unsigned char);
			fz_catch(state->ctx)
			{
				unlock(state);
				return 0;
			}
			state->buffer_max = new_max;
		}
	}

	DEBUG_MESSAGE(("data arrived: offset=%ld len=%ld\n", state->current_fill_start, size));
	/* Although we always trigger fills starting on block boundaries,
	 * code this to allow for curl calling us to copy smaller blocks
	 * as they arrive. */
	old_start = state->current_fill_start;
	memcpy(state->buffer + state->current_fill_start, ptr, size);
	state->current_fill_start += size;
	/* If we've reached the end, or at least a different block
	 * mark that we've got that block. */
	if (state->map && (state->current_fill_start == state->content_length ||
		(((state->current_fill_start ^ old_start) & ~(BLOCK_SIZE-1)) != 0)))
	{
		old_start >>= BLOCK_SHIFT;
		state->map[old_start>>3] |= 1<<(old_start & 7);
	}
	unlock(state);

	return size;
}

static void fetch_chunk(struct curlstate *state)
{
	char text[32];
	size_t block, start, end;
	CURLcode ret;

	ret = curl_easy_perform(state->easy);
	if (ret != CURLE_OK) {
		/* If we get an error, store it, and kill the thread.
		 * The next fetch will return it. */
		lock(state);
		state->curl_error = ret;
		state->kill_thread = 1;
		unlock(state);
		return;
	}

	/* We finished the header, now request the body. */
	lock(state);
	if (state->head)
	{
		state->head = 0;
		curl_easy_setopt(state->easy, CURLOPT_NOBODY, 0);
		curl_easy_setopt(state->easy, CURLOPT_HEADERFUNCTION, NULL);
		curl_easy_setopt(state->easy, CURLOPT_WRITEHEADER, NULL);
		if (state->accept_ranges)
		{
			fz_snprintf(text, 32, "%d-%d", 0, BLOCK_SIZE-1);
			curl_easy_setopt(state->easy, CURLOPT_RANGE, text);
			state->next_fill_start = BLOCK_SIZE;
		}
		unlock(state);
		return;
	}

	/* We finished the current body. If not accepting ranges, that's the end. */
	if (!state->accept_ranges)
	{
		DEBUG_MESSAGE(("we got it all, in one request.\n"));
		state->complete = 1;
		state->kill_thread = 1;
		unlock(state);
		return;
	}

	/* Find the next block to fetch */
	assert((state->next_fill_start & (BLOCK_SHIFT-1)) == 0);
	block = state->next_fill_start>>BLOCK_SHIFT;
	if (state->content_length > 0)
	{
		/* Find the next block that we haven't got */
		size_t map_length = state->map_length;
		unsigned char *map = state->map;
		while (block < map_length && HAVE_BLOCK(map, block))
			++block;
		if (block == map_length)
		{
			block = 0;
			while (block < map_length && HAVE_BLOCK(map, block))
				++block;
			if (block == map_length)
			{
				/* We've got it all! */
				DEBUG_MESSAGE(("we got it all block=%d map_length=%d!\n", block, map_length));
				state->complete = 1;
				state->kill_thread = 1;
				unlock(state);
				return;
			}
		}
	}
	else
	{
		state->complete = 1;
		state->kill_thread = 1;
		unlock(state);
		return;
	}

	DEBUG_MESSAGE(("block requested was %d, fetching %d\n", state->next_fill_start>>BLOCK_SHIFT, block));

	/* Set up fetch of that block */
	start = block<<BLOCK_SHIFT;
	end = start + BLOCK_SIZE-1;
	state->current_fill_start = start;
	if (state->content_length > 0 && end >= state->content_length)
		end = state->content_length-1;
	state->current_fill_end = end;
	fz_snprintf(text, 32, "%d-%d", start, end);

	/* Unless anyone changes this in the meantime, the
	 * next block we fetch will follow on from this one. */
	state->next_fill_start = state->current_fill_start+BLOCK_SIZE;
	unlock(state);

	/* Request next range! */
	DEBUG_MESSAGE(("requesting range %s\n", text));
	curl_easy_setopt(state->easy, CURLOPT_RANGE, text);
}

static int cs_next(fz_context *ctx, fz_stream *stream, size_t len)
{
	struct curlstate *state = stream->state;
	size_t len_read = 0;
	int64_t read_point = stream->pos;
	int block = read_point>>BLOCK_SHIFT;
	size_t left_over = (-read_point) & (BLOCK_SIZE-1);
	unsigned char *buf = state->public_buffer;

	assert(len != 0);

	stream->rp = stream->wp = buf;
	lock(state);

	/* If we got an error from the fetching thread,
	 * throw it here (but just once). */
	if (state->curl_error)
	{
		CURLcode err = state->curl_error;
		state->curl_error = 0;
		unlock(state);
		fz_throw(ctx, FZ_ERROR_GENERIC, "cannot fetch data: %s", curl_easy_strerror(err));
	}

	if ((size_t) read_point > state->content_length)
	{
		unlock(state);
		if (state->data_arrived == 0)
			fz_throw(ctx, FZ_ERROR_TRYLATER, "read of a block we don't have (A) (offset=%ld)", read_point);
		return EOF;
	}

	if (len > sizeof(state->public_buffer))
		len = sizeof(state->public_buffer);

	if (state->map == NULL)
	{
		/* We are doing a simple linear fetch as we don't know the
		 * content length. */
		if (read_point + len > state->current_fill_start)
		{
			unlock(state);
			fz_throw(ctx, FZ_ERROR_TRYLATER, "read of a block we don't have (B) (offset=%ld)", read_point);
		}
		memcpy(buf, state->buffer + read_point, len);
		unlock(state);
		stream->wp = buf + len;
		stream->pos += len;
		if (len == 0)
			return EOF;
		return *stream->rp++;
	}

	/* We are reading from a "mapped" file */
	if (read_point + len > state->content_length)
		len = state->content_length - read_point;
	if (left_over > len)
		left_over = len;
	if (left_over > 0)
	{
		/* We are starting midway through a block */
		if (!HAVE_BLOCK(state->map, block))
		{
			state->next_fill_start = block<<BLOCK_SHIFT;
			unlock(state);
			fz_throw(ctx, FZ_ERROR_TRYLATER, "read of a block we don't have (C) (offset=%ld)", read_point);
		}
		block++;
		memcpy(buf, state->buffer + read_point, left_over);
		buf += left_over;
		read_point += left_over;
		len -= left_over;
		len_read += left_over;
	}

	/* Copy any complete blocks */
	while (len > BLOCK_SIZE)
	{
		if (!HAVE_BLOCK(state->map, block))
		{
			/* We don't have enough data to fulfill the request. */
			/* Fetch the next block from here. */
			unlock(state);
			state->next_fill_start = block<<BLOCK_SHIFT;
			stream->wp += len_read;
			stream->pos += len_read;
			/* If we haven't fetched anything, throw. */
			if (len_read == 0)
				fz_throw(ctx, FZ_ERROR_TRYLATER, "read of a block we don't have (D) (offset=%ld)", read_point);
			/* Otherwise, we got at least one byte, so we can safely return that. */
			return *stream->rp++;
		}
		block++;
		memcpy(buf, state->buffer + read_point, BLOCK_SIZE);
		buf += BLOCK_SIZE;
		read_point += BLOCK_SIZE;
		len -= BLOCK_SIZE;
		len_read += BLOCK_SIZE;
	}

	/* Copy any trailing bytes */
	if (len > 0)
	{
		if (!HAVE_BLOCK(state->map, block))
		{
			/* We don't have enough data to fulfill the request. */
			/* Fetch the next block from here. */
			unlock(state);
			state->next_fill_start = block<<BLOCK_SHIFT;
			stream->wp += len_read;
			stream->pos += len_read;
			/* If we haven't fetched anything, throw. */
			if (len_read == 0)
				fz_throw(ctx, FZ_ERROR_TRYLATER, "read of a block we don't have (E) (offset=%ld)", read_point);
			/* Otherwise, we got at least one byte, so we can safely return that. */
			return *stream->rp++;
		}
		memcpy(buf, state->buffer + read_point, len);
		len_read += len;
	}

	unlock(state);
	stream->wp += len_read;
	stream->pos += len_read;
	if (len_read == 0)
		return EOF;
	return *stream->rp++;
}

static void cs_close(fz_context *ctx, void *state_)
{
	struct curlstate *state = state_;

	lock(state);
	state->kill_thread = 1;
	unlock(state);

#ifdef _WIN32
	WaitForSingleObject(state->thread, INFINITE);
	CloseHandle(state->thread);
	CloseHandle(state->mutex);
#else
	pthread_join(state->thread, NULL);
	pthread_mutex_destroy(&state->mutex);
#endif

	curl_easy_cleanup(state->easy);
	fz_free(ctx, state->buffer);
	fz_free(ctx, state->map);
	fz_free(ctx, state);
}

static void cs_seek(fz_context *ctx, fz_stream *stm, int64_t offset, int whence)
{
	struct curlstate *state = stm->state;

	stm->wp = stm->rp;
	if (whence == SEEK_END)
	{
		size_t clen;
		int data_arrived;
		lock(state);
		data_arrived = state->data_arrived;
		clen = state->content_length;
		unlock(state);
		if (!data_arrived)
			fz_throw(ctx, FZ_ERROR_TRYLATER, "still awaiting file length");
		stm->pos = clen + offset;
	}
	else if (whence == SEEK_CUR)
		stm->pos += offset;
	else
		stm->pos = offset;
	if (stm->pos < 0)
		stm->pos = 0;
}

static void
fetcher_thread(curlstate *state)
{
	/* Keep fetching chunks on a background thread until
	 * either we have to kill the thread, or the fetch
	 * is complete. */
	while (1) {
		int complete;
		lock(state);
		complete = state->complete || state->kill_thread;
		unlock(state);
		if (complete)
			break;
		fetch_chunk(state);
		if (state->more_data)
			state->more_data(state->more_data_arg, 0);
	}
	if (state->more_data)
		state->more_data(state->more_data_arg, 1);
}

#ifdef _WIN32
static DWORD WINAPI
win_thread(void *lparam)
{
	fetcher_thread((curlstate *)lparam);

	return 0;
}
#else
static void *
pthread_thread(void *arg)
{
	fetcher_thread((curlstate *)arg);
	return NULL;
}
#endif

fz_stream *fz_open_url(fz_context *ctx, const char *url, int kbps, void (*more_data)(void *,int), void *more_data_arg)
{
	struct curlstate *state;
	fz_stream *stm;
	CURLcode code;

	state = fz_malloc_struct(ctx, struct curlstate);
	state->ctx = ctx;

	code = curl_global_init(CURL_GLOBAL_ALL);
	if (code != CURLE_OK)
		fz_throw(ctx, FZ_ERROR_GENERIC, "curl_global_init failed");

	state->easy = curl_easy_init();
	if (!state->easy)
		fz_throw(ctx, FZ_ERROR_GENERIC, "curl_easy_init failed");

	curl_easy_setopt(state->easy, CURLOPT_URL, url);
	curl_easy_setopt(state->easy, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(state->easy, CURLOPT_MAXREDIRS, 12);
	curl_easy_setopt(state->easy, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(state->easy, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(state->easy, CURLOPT_MAX_RECV_SPEED_LARGE, kbps * 1024);
	curl_easy_setopt(state->easy, CURLOPT_HEADERFUNCTION, on_curl_header);
	curl_easy_setopt(state->easy, CURLOPT_WRITEHEADER, state);
	curl_easy_setopt(state->easy, CURLOPT_WRITEFUNCTION, on_curl_data);
	curl_easy_setopt(state->easy, CURLOPT_WRITEDATA, state);

	/* Get only the HEAD first. */
	state->head = 1;
	curl_easy_setopt(state->easy, CURLOPT_NOBODY, 1);

#ifdef _WIN32
	state->mutex = CreateMutex(NULL, FALSE, NULL);
	if (state->mutex == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "mutex creation failed");

	state->thread = CreateThread(NULL, 0, win_thread, state, 0, &state->thread_id);
	if (state->thread == NULL)
		fz_throw(ctx, FZ_ERROR_GENERIC, "thread creation failed");
#else
	if (pthread_mutex_init(&state->mutex, NULL))
		fz_throw(ctx, FZ_ERROR_GENERIC, "mutex creation failed");

	if (pthread_create(&state->thread, NULL, pthread_thread, state))
		fz_throw(ctx, FZ_ERROR_GENERIC, "thread creation failed");
#endif
	state->more_data = more_data;
	state->more_data_arg = more_data_arg;

	stm = fz_new_stream(ctx, state, cs_next, cs_close);
	stm->progressive = 1;
	stm->seek = cs_seek;
	return stm;
}
