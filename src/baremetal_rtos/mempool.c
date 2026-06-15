#include "mempool.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

struct mempool {
    uint8_t *buffer;
    size_t block_size;
    size_t block_count;
    void *free_head;
    struct mempool *next;
};

static struct mempool *registry = NULL;

void mempool_init(mempool_t **out, void *buffer, size_t block_size, size_t count)
{
    if(!out || !buffer) return;
    struct mempool *p = (struct mempool*)malloc(sizeof(struct mempool));
    if(!p) return;
    p->buffer = (uint8_t*)buffer;
    p->block_size = block_size;
    p->block_count = count;
    p->free_head = NULL;
    p->next = NULL;

    /* build free list */
    for(size_t i=0;i<count;i++){
        void *blk = p->buffer + i*block_size;
        *(void**)blk = p->free_head;
        p->free_head = blk;
    }

    CRIT_ENTER();
    p->next = registry;
    registry = p;
    CRIT_EXIT();
    *out = (mempool_t*)p;
}

void* mempool_alloc(mempool_t *p)
{
    if(!p) return NULL;
    CRIT_ENTER();
    void *blk = p->free_head;
    if(blk) p->free_head = *(void**)blk;
    CRIT_EXIT();
    return blk;
}

void mempool_free(mempool_t *p, void *ptr)
{
    if(!p || !ptr) return;
    CRIT_ENTER();
    *(void**)ptr = p->free_head;
    p->free_head = ptr;
    CRIT_EXIT();
}
