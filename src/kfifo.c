/*
 * kfifo.c
 *
 *  Created on: May 19, 2020
 *      Author: bise
 */

#include "kfifo.h"
#include <string.h>

#define min(a, b)	((a < b) ? a : b)
#define smp_wmb()	(__sync_synchronize())

static inline int fls(int x)
{
	int pos, i;
	if (x != 0)
	{
		for (i = (x >> 1), pos = 0; i != 0; pos++)
			i >>= 1;
	}
	else
		pos = -1;

	return (pos + 1);
}

int roundup_pow_of_two(int x)
{
	return 1UL << fls(x - 1);
}

int rounddown_pow_of_two(int x)
{
	return 1UL << (fls(x) - 1);
}
/*
 * internal helper to calculate the unused elements in a fifo
 */
static inline unsigned int kfifo_unused(struct kfifo *fifo)
{
	return (fifo->mask + 1) - (fifo->in - fifo->out);
}

int kfifo_alloc(struct kfifo *fifo, unsigned int size, size_t esize)
{
	/*
	 * round down to the next power of 2, since our 'let the indices
	 * wrap' technique works only in this case.
	 */
	size = roundup_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = esize;

	if (size < 2)
	{
		fifo->data = NULL;
		fifo->mask = 0;
		return -1;
	}

	fifo->data = malloc(size * esize);

	if (!fifo->data)
	{
		fifo->mask = 0;
		return -1;
	}
	fifo->mask = size - 1;

	return 0;
}

void kfifo_free(struct kfifo *fifo)
{

	free(fifo->data);
	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = 0;
	fifo->data = NULL;
	fifo->mask = 0;

	//pthread_mutex_destroy(&(fifo->mutex));
}

int kfifo_init(struct kfifo *fifo, void *buffer, unsigned int size, size_t esize)
{
	size /= esize;

	size = roundup_pow_of_two(size);

	fifo->in = 0;
	fifo->out = 0;
	fifo->esize = esize;
	fifo->data = buffer;

	if (size < 2)
	{
		fifo->mask = 0;
		return -1;
	}
	fifo->mask = size - 1;
	return 0;
}

static void kfifo_copy_in(struct kfifo *fifo, const void *src, unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;

	off &= fifo->mask;
	if (esize != 1)
	{
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	memcpy(fifo->data + off, src, l);
	memcpy(fifo->data, src + l, len - l);
	/*
	 * make sure that the data in the fifo is up to date before
	 * incrementing the fifo->in index counter
	 */
	smp_wmb();
}

unsigned int kfifo_write(struct kfifo *fifo, const void *buf, unsigned int len)
{
	unsigned int l;

	l = kfifo_unused(fifo);
	if (len > l)
		len = l;

	kfifo_copy_in(fifo, buf, len, fifo->in);
	fifo->in += len;
	return len;
}

static void kfifo_copy_out(struct kfifo *fifo, void *dst, unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;

	off &= fifo->mask;
	if (esize != 1)
	{
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	memcpy(dst, fifo->data + off, l);
	memcpy(dst + l, fifo->data, len - l);
	/*
	 * make sure that the data is copied before
	 * incrementing the fifo->out index counter
	 */
	smp_wmb();
}

unsigned int kfifo_out_peek(struct kfifo *fifo, void *buf, unsigned int len)
{
	unsigned int l;

	l = fifo->in - fifo->out;
	if (len > l)
		len = l;

	kfifo_copy_out(fifo, buf, len, fifo->out);
	return len;
}

unsigned int kfifo_read(struct kfifo *fifo, void *buf, unsigned int len)
{
	len = kfifo_out_peek(fifo, buf, len);
	fifo->out += len;
	return len;
}

int kfifo_write_pack(struct kfifo *fifo, const void *buf, unsigned int len)
{
	unsigned int l;
	l = kfifo_unused(fifo);
	if (len + sizeof(len) > l)
	{

		return PIPE_FULL;
	}
	kfifo_write(fifo, &len, sizeof(len));
	int ret = kfifo_write(fifo, buf, len);
	return ret;

}

int kfifo_read_pack(struct kfifo *fifo, void *buf, unsigned int len)
{
	unsigned int l, i;
	i = kfifo_read(fifo, &l, sizeof(l));
	if (i != sizeof(l))
	{

		return PIPE_EMPTY;
	}

	else
	{
		if (l > len)
		{
			fifo->out += l;

			return PIPE_NOMEM;
		}
		else
		{
			int ret = kfifo_read(fifo, buf, l);

			return ret;
		}
	}
}

static void kfifo_copy_out2(struct kfifo *fifo, unsigned int len, unsigned int off)
{
	unsigned int size = fifo->mask + 1;
	unsigned int esize = fifo->esize;
	unsigned int l;

	off &= fifo->mask;
	if (esize != 1)
	{
		off *= esize;
		size *= esize;
		len *= esize;
	}
	l = min(len, size - off);

	//    memcpy(dst, fifo->data + off, l);
	//    memcpy(dst + l, fifo->data, len - l);
	/*
	 * make sure that the data is copied before
	 * incrementing the fifo->out index counter
	 */
	smp_wmb();
}

unsigned int kfifo_out_peek2(struct kfifo *fifo, unsigned int len)
{
	unsigned int l;

	l = fifo->in - fifo->out;
	if (len > l)
		len = l;

	kfifo_copy_out2(fifo, len, fifo->out);
	return len;
}

unsigned int kfifo_read2(struct kfifo *fifo, unsigned int len)
{
	len = kfifo_out_peek2(fifo, len);
	fifo->out += len;
	return len;
}

