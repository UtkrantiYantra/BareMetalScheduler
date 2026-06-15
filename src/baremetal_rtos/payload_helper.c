#include "mempool.h"
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>

/* Simple wrapper implementation for task_payload_alloc and free.
   For embedded determinism you should create static pools and register them. */

mempool_t *global_pool = NULL;

void init_default_payload_pool(void *buf, size_t blk_size, size_t count){
    mempool_init(&global_pool, buf, blk_size, count);
}

void* task_payload_alloc(size_t size){
    (void)size;
    if(!global_pool) return NULL;
    return mempool_alloc(global_pool);
}

void task_payload_free_impl(void *p){
    if(!global_pool) return;
    mempool_free(global_pool, p);
}
