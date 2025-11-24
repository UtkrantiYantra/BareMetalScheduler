#ifndef MEMPOOL_H
#define MEMPOOL_H
#include <stdint.h>
#include <stddef.h>

typedef struct mempool mempool_t;

void mempool_init(mempool_t **out, void *buffer, size_t block_size, size_t count);
void* mempool_alloc(mempool_t *p);
void mempool_free(mempool_t *p, void *ptr);

#endif // MEMPOOL_H
