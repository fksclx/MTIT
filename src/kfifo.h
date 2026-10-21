#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

struct kfifo
{
	unsigned int in;
	unsigned int out;
	unsigned int mask;
	unsigned int esize;
	void *data;

	pthread_mutex_t mutex;
};

#define KBYTE_MEM(x)		(x * 1024)
#define MBYTE_MEM(x)		(x * 1024 * 1024)

#define PIPE_EMPTY	-1
#define PIPE_FULL	-2
#define PIPE_NOMEM	-3

__BEGIN_DECLS

int roundup_pow_of_two(int x);
int rounddown_pow_of_two(int x);

int kfifo_alloc(struct kfifo *fifo, unsigned int size, size_t esize);
void kfifo_free(struct kfifo *fifo);

int kfifo_init(struct kfifo *fifo, void *buffer, unsigned int size, size_t esize);
unsigned int kfifo_write(struct kfifo *fifo, const void *buf, unsigned int len);
unsigned int kfifo_read(struct kfifo *fifo, void *buf, unsigned int len);

int kfifo_write_pack(struct kfifo *fifo, const void *buf, unsigned int len);
int kfifo_read_pack(struct kfifo *fifo, void *buf, unsigned int len);

__END_DECLS


