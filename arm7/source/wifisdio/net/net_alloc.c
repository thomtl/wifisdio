#include "net_alloc.h"
#include <nds.h>

#include "../wifisdio.h"

// TODO: Linked list allocator, not such a simple bump allocator

TWL_BSS uint8_t block[0x1000] = {0};
TWL_BSS uint8_t* curr = NULL;
TWL_BSS size_t n_allocations = 0;

void* net_malloc(size_t s) {
    if(!curr)
        curr = block;

    s += 7;
    s &= ~7; // Align up

    uint8_t* ret = curr;
    curr += s;

    if(ret >= (block + 0x1000))
        panic("net_malloc: OOM\n");

    n_allocations++;

    return ret;
}

void net_free(void* ptr) {
    (void)ptr;

    n_allocations -= 1;
    if(n_allocations == 0) // No more things allocated so we can reset curr
        curr = block;
}