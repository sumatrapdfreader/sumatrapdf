/*
 * Output streams.
 */

#include "fitz-base.h"
#include "fitz-stream.h"

int
fz_wtell(fz_stream *stm)
{
    fz_buffer *buf = stm->buffer;
    int t;

    if (stm->dead)
        return EOF;

    if (stm->mode != FZ_SWRITE)
        return EOF;

    switch (stm->kind)
    {
    case FZ_SFILE:
        t = lseek(stm->file, 0, 1);
        if (t < 0)
        {
            fz_warn("syserr: lseek: %s", strerror(errno));
            stm->dead = 1;
            return EOF;
        }
        return t + (buf->wp - buf->rp);

    case FZ_SFILTER:
        return stm->filter->count + (buf->wp - buf->rp);

    case FZ_SBUFFER:
        return buf->wp - buf->bp;

    default:
        return EOF;
    }
}

fz_error *
fz_wseek(fz_stream *stm, int offset, int whence)
{
    fz_buffer *buf = stm->buffer;
    int t;

    if (stm->dead)
        return fz_throw("assert: seek in dead stream");

    if (stm->mode != FZ_SWRITE)
        return fz_throw("assert: write operation on reading stream");

    if (stm->kind != FZ_SFILE)
        return fz_throw("assert: write seek on non-file stream");

    t = lseek(stm->file, offset, whence);
    if (t < 0)
    {
        stm->dead = 1;
        return fz_throw("syserr: lseek: %s", strerror(errno));
    }

    buf->rp = buf->bp;
    buf->wp = buf->bp;
    buf->eof = 0;

    return fz_okay;
}

static fz_error *
fz_flushfilterimp(fz_stream *stm)
{
    fz_buffer *buf = stm->buffer;
    fz_error *error;
    fz_error *reason;

loop:

    reason = fz_process(stm->filter, stm->buffer, stm->chain->buffer);

    if (reason == fz_ioneedin)
    {
        if (buf->rp > buf->ep)
        {
            error = fz_rewindbuffer(buf);
            if (error)
            {
                stm->dead = 1;
                return fz_rethrow(error, "cannot rewind buffer");
            }
        }
        else
        {
            error = fz_growbuffer(buf);
            if (error)
            {
                stm->dead = 1;
                return fz_rethrow(error, "cannot grow buffer");
            }
        }
    }

    else if (reason == fz_ioneedout)
    {
        error = fz_flush(stm->chain);
        if (error)
            return fz_rethrow(error, "cannot flush chain buffer");
    }

    else if (reason == fz_iodone)
    {
        stm->dead = 2;	/* special flag that we are dead because of eod */
    }

    else
    {
        stm->dead = 1;
        return fz_rethrow(reason, "cannot process filter");
    }

    /* if we are at eof, repeat until other filter sets otherside to eof */
    if (buf->eof && !stm->chain->buffer->eof)
        goto loop;

    return fz_okay;
}

/*
 * Empty the buffer into the sink.
 * Promise to make more space available.
 * Called by fz_write and fz_dropstream.
 * If buffer is eof, then all data must be flushed.
 */
fz_error *
fz_flush(fz_stream *stm)
{
    fz_buffer *buf = stm->buffer;
    fz_error *error;
    int t;

    if (stm->dead == 2) /* eod flag */
        return fz_okay;

    if (stm->dead)
        return fz_throw("assert: flush on dead stream");

    if (stm->mode != FZ_SWRITE)
        return fz_throw("assert: write operation on reading stream");

    switch (stm->kind)
    {
    case FZ_SFILE:
        while (buf->rp < buf->wp)
        {
            t = write(stm->file, buf->rp, buf->wp - buf->rp);
            if (t < 0)
            {
                stm->dead = 1;
                return fz_throw("syserr: write: %s", strerror(errno));
            }

            buf->rp += t;
        }

        if (buf->rp > buf->bp)
        {
            error = fz_rewindbuffer(buf);
            if (error)
            {
                stm->dead = 1;
                return fz_rethrow(error, "cannot rewind buffer");
            }
        }

        return fz_okay;

    case FZ_SFILTER:
        error = fz_flushfilterimp(stm);
        if (error)
            return fz_rethrow(error, "cannot flush through filter");
        return fz_okay;

    case FZ_SBUFFER:
        if (!buf->eof && buf->wp == buf->ep)
        {
            error = fz_growbuffer(buf);
            if (error)
            {
                stm->dead = 1;
                return fz_rethrow(error, "cannot grow buffer");
            }
        }
        return fz_okay;

    default:
        return fz_throw("unknown stream type");
    }
}

/*
 * Write data to stream.
 * Buffer until internal buffer is full.
 * When full, call fz_flush to make more space available.
 * Return error if all the data could not be written.
 */
fz_error *
fz_write(fz_stream *stm, unsigned char *mem, int n)
{
    fz_buffer *buf = stm->buffer;
    fz_error *error;
    int i = 0;

    if (stm->dead)
        return fz_throw("assert: write on dead stream");

    if (stm->mode != FZ_SWRITE)
        return fz_throw("assert: write on reading stream");

    while (i < n)
    {
        while (buf->wp < buf->ep && i < n)
            *buf->wp++ = mem[i++];

        if (buf->wp == buf->ep && i < n)
        {
            error = fz_flush(stm);
            if (error)
                return fz_rethrow(error, "cannot flush buffer");
            if (stm->dead)
                return fz_throw("assert: write on dead stream");
        }
    }

    return fz_okay;
}

fz_error *
fz_printstr(fz_stream *stm, char *s)
{
    return fz_write(stm, s, strlen(s));
}

fz_error *
fz_printobj(fz_stream *file, fz_obj *obj, int tight)
{
    fz_error *error;
    char buf[1024];
    char *ptr;
    int n;

    n = fz_sprintobj(nil, 0, obj, tight);
    if (n < sizeof buf)
    {
        fz_sprintobj(buf, sizeof buf, obj, tight);
        error = fz_write(file, buf, n);
        if (error)
            return fz_rethrow(error, "cannot write buffer");
        return fz_okay;
    }
    else
    {
        ptr = fz_malloc(n);
        if (!ptr)
            return fz_throw("outofmem: scratch buffer");
        fz_sprintobj(ptr, n, obj, tight);
        error = fz_write(file, ptr, n);
        if (error)
            error = fz_rethrow(error, "cannot write buffer");
        fz_free(ptr);
        return error;
    }
}

fz_error *
fz_print(fz_stream *stm, char *fmt, ...)
{
    fz_error *error;
    va_list ap;
    char buf[1024];
    char *p;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);

    if (n < sizeof buf)
    {
        error = fz_write(stm, buf, n);
        if (error)
            return fz_rethrow(error, "cannot write buffer");
        return fz_okay;
    }

    p = fz_malloc(n);
    if (!p)
        return fz_throw("outofmem: scratch buffer");

    va_start(ap, fmt);
    vsnprintf(p, n, fmt, ap);
    va_end(ap);

    error = fz_write(stm, p, n);
    if (error)
        error = fz_rethrow(error, "cannot write buffer");

    fz_free(p);

    return error;
}

