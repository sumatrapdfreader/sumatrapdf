#include "fitz_base.h"
#include "fitz_stream.h"

#define noDEBUG 1

typedef struct fz_pipeline_s fz_pipeline;

fz_error fz_processpipeline(fz_filter *filter, fz_buffer *in, fz_buffer *out);

struct fz_pipeline_s
{
	fz_filter super;
	fz_filter *head;
	fz_buffer *buffer;
	fz_filter *tail;
	int tailneedsin;
};

fz_filter *
fz_chainpipeline(fz_filter *head, fz_filter *tail, fz_buffer *buf)
{
	FZ_NEWFILTER(fz_pipeline, p, pipeline);
	p->head = fz_keepfilter(head);
	p->tail = fz_keepfilter(tail);
	p->tailneedsin = 1;
	p->buffer = fz_keepbuffer(buf);
	return (fz_filter*)p;
}

void
fz_unchainpipeline(fz_filter *filter, fz_filter **oldfp, fz_buffer **oldbp)
{
	fz_pipeline *p = (fz_pipeline*)filter;

	*oldfp = fz_keepfilter(p->head);
	*oldbp = fz_keepbuffer(p->buffer);

	fz_dropfilter(filter);
}

fz_filter *
fz_newpipeline(fz_filter *head, fz_filter *tail)
{
	FZ_NEWFILTER(fz_pipeline, p, pipeline);

	p->buffer = fz_newbuffer(FZ_BUFSIZE);
	p->head = fz_keepfilter(head);
	p->tail = fz_keepfilter(tail);
	p->tailneedsin = 1;

	return (fz_filter*)p;
}

void
fz_droppipeline(fz_filter *filter)
{
	fz_pipeline *p = (fz_pipeline*)filter;
	fz_dropfilter(p->head);
	fz_dropfilter(p->tail);
	fz_dropbuffer(p->buffer);
}

fz_error
fz_processpipeline(fz_filter *filter, fz_buffer *in, fz_buffer *out)
{
	fz_pipeline *p = (fz_pipeline*)filter;
	fz_error e;

	if (p->buffer->eof)
		goto tail;

	if (p->tailneedsin && p->head->produced)
		goto tail;

head:
	e = fz_process(p->head, in, p->buffer);

	if (e == fz_ioneedin)
		return fz_ioneedin;

	else if (e == fz_ioneedout)
	{
		if (p->tailneedsin && !p->head->produced)
		{
			if (p->buffer->rp > p->buffer->bp)
				fz_rewindbuffer(p->buffer);
			else
				fz_growbuffer(p->buffer);
			goto head;
		}
		goto tail;
	}

	else if (e == fz_iodone)
		goto tail;

	else if (e)
		return fz_rethrow(e, "cannot process head filter");

	else
		return fz_okay;

tail:
	e = fz_process(p->tail, p->buffer, out);

	if (e == fz_ioneedin)
	{
		if (p->buffer->eof)
			return fz_throw("ioerror: premature eof in pipeline");
		p->tailneedsin = 1;
		goto head;
	}

	else if (e == fz_ioneedout)
	{
		p->tailneedsin = 0;
		return fz_ioneedout;
	}

	else if (e == fz_iodone)
	{
		/* Make sure that the head is also done.
		 * It may still contain end-of-data markers or garbage.
		 */
		e = fz_process(p->head, in, p->buffer);
		if (e != fz_iodone)
			fz_catch(e, "head filter not done");
		return fz_iodone;
	}

	else if (e)
		return fz_rethrow(e, "cannot process tail filter");

	else
		return fz_okay;
}

