#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define __DISABLE_IRQ__
#define __ENABLE_IRQ__


#define __MAX_ALIGNMENT__   4
#define POOL_NUM_MAX    2

/*
 * Per-pool allocator state.
 *
 * _heap_of_memory[] points to the beginning of each caller-provided memory
 * pool. _last_heap_object[] is the current high-water mark in that pool, and
 * _top_of_heap[] is the last byte accepted as pool storage.
 *
 * this_idx selects the active pool for all static bmalloc APIs below. The
 * current implementation does not expose a public pool switch API, so callers
 * must not assume that a pointer can be freed while another pool is active.
 */
static char *_last_heap_object[POOL_NUM_MAX];
static char *_heap_of_memory[POOL_NUM_MAX];
static char *_top_of_heap[POOL_NUM_MAX];
static uint8_t this_active[POOL_NUM_MAX];

/******************************************************************************/

typedef struct {
    /* Non-zero when the block is allocated. */
    char isBusy : 4;
    /* Pool index recorded for diagnostics/future multi-pool support. */
    char index : 4;
} __attribute__((packed)) MemHdrFlag_t;

/*
 * Header stored before every user allocation.
 *
 * Blocks are linked in address order. next points to the next block header or
 * to _last_heap_object[index] when this is the last block in use.
 */
typedef struct
{
    MemHdrFlag_t flag;
    char *next;
} __attribute__((aligned(4))) _m_header;

/*
 * Attach a caller-provided memory range to the active pool and reset that
 * pool's allocation state.
 *
 * The caller is responsible for passing an aligned heap base and a length large
 * enough to hold at least one _m_header plus payload.
 */
static void init_mem_pool(uint8_t index, void *heap, size_t len)
{
    uintptr_t heap_start;
    uintptr_t aligned_start;
    size_t align_skip;
    char *heap_base;

    if (index >= POOL_NUM_MAX) {
        return;
    }

    _last_heap_object[index] = 0;
    _heap_of_memory[index] = NULL;
    _top_of_heap[index] = NULL;
    this_active[index] = 0;

    if (heap == NULL || len <= sizeof(_m_header)) {
        return;
    }

    heap_start = (uintptr_t)heap;
    aligned_start = (heap_start + (__MAX_ALIGNMENT__ - 1U)) & ~((uintptr_t)__MAX_ALIGNMENT__ - 1U);
    align_skip = (size_t)(aligned_start - heap_start);
    if (len <= (align_skip + sizeof(_m_header))) {
        return;
    }

    len -= align_skip;
    heap_base = (char *)aligned_start;

    memset(heap_base, 0, len);
    _heap_of_memory[index] = heap_base;
    _top_of_heap[index] = heap_base + (len - 1);
    this_active[index] = 1;
}

/*
 * Split a reused free block when the unused tail is large enough to hold
 * another block header.
 */
static void _make_new_mem_hole_xdata(_m_header *head_p, char *ptr)
{
    char *p;

    if (head_p->next - ptr > sizeof(_m_header))
    {
        p = head_p->next;
        head_p->next = ptr;
        head_p = (_m_header *) ptr;
        head_p->flag.isBusy = 0;
        head_p->next = p;
    }
}

/*
 * Allocate from the active pool.
 *
 * Allocation strategy:
 * 1. Round the requested size up to __MAX_ALIGNMENT__.
 * 2. Prefer appending a new block at the pool high-water mark.
 * 3. If the top has no room, scan from the pool start and reuse the first
 *    large-enough free block.
 *
 * Returns a pointer to payload storage after the internal header, or NULL when
 * no suitable space exists.
 *
 * Note: __DISABLE_IRQ__/__ENABLE_IRQ__ are currently empty placeholders. This
 * function is not thread-safe until those macros are replaced by a real Zephyr
 * lock or the API is restricted to one scheduler thread.
 */
static void * bmalloc(uint8_t index , size_t n)
{
    __DISABLE_IRQ__
    char *ptr;
    char *ret_ptr;
    char *result_ptr;
    _m_header *head_p;
    result_ptr = NULL;

    if(index >= POOL_NUM_MAX || !this_active[index] || n == 0)
    {
        goto FUNC_EXIT;
    }

    n += __MAX_ALIGNMENT__ - 1;
    n &= ~(__MAX_ALIGNMENT__ - 1);

    /* Lazy-initialize the high-water mark to the active pool base. */
    if(_last_heap_object[index] == 0)
    {
        char * p = (void *)_heap_of_memory[index];
        _last_heap_object[index] = p;
    }

    if ((n + sizeof(_m_header)) <= (_top_of_heap[index] - _last_heap_object[index]))
    {
        /* Fast path: append a new block at the current high-water mark. */
        head_p = (_m_header *) _last_heap_object[index];
        head_p->flag.isBusy = 1;
        head_p->flag.index = index;
        _last_heap_object[index] += n + sizeof(_m_header);
        head_p->next = _last_heap_object[index];
        result_ptr = _last_heap_object[index] - n;
    }
    else
    {
        /* Slow path: first-fit search through previously freed holes. */
        ptr = _heap_of_memory[index];
        while (ptr < _last_heap_object[index])
        {
            head_p = (_m_header *) ptr;
            if (   ! head_p->flag.isBusy
                && (head_p->next - ptr) - sizeof(_m_header) >= n )
            {
                ret_ptr = ptr + sizeof(_m_header);
                head_p->flag.isBusy = 1;
                head_p->flag.index = index;

                /* Preserve any usable tail of the free block as a new hole. */
                _make_new_mem_hole_xdata(head_p, ret_ptr + n);
                result_ptr = ret_ptr;
                goto FUNC_EXIT;
            }
            ptr = head_p->next;
        }
        result_ptr = 0;
    }

    /* Top allocations are zeroed before being returned. */
    if ( result_ptr != 0)
    {
        memset(result_ptr,0,n);
    }
    
FUNC_EXIT:
    if(!result_ptr)
    {
    }

    __ENABLE_IRQ__
    
    return (void *)result_ptr;
}

/*
 * Free a block from the active pool.
 *
 * The function linearly scans the active pool, marks the matching block free,
 * and coalesces adjacent free blocks. If the freed block reaches the current
 * high-water mark, the high-water mark is moved back so future allocations can
 * reuse the top of the pool.
 *
 * Passing NULL is allowed. Passing a pointer not owned by the active pool is a
 * no-op after the scan completes.
 */
static void bfree( uint8_t index ,void *ptr)
{
    __DISABLE_IRQ__
    char *prev = _heap_of_memory[index];
    char *curr = _heap_of_memory[index];
    _m_header *prev_h;
    _m_header *curr_h;
    _m_header *next_h;
    _m_header *now_h;
    (void)now_h;

    if (index >= POOL_NUM_MAX || !this_active[index] || ptr == 0)
    {
        goto FUNC_EXIT;
    }

    now_h = (_m_header *) ptr;

    while (curr < _last_heap_object[index])
    {
        if (ptr == curr + sizeof(_m_header))
        {
            curr_h = (_m_header *) curr;
            if (curr_h->flag.isBusy)
            {
                prev_h = (_m_header *) prev;
                next_h = curr_h->next < _last_heap_object[index] ?
                (_m_header *) curr_h->next : curr_h;

                /* Merge with the previous free block, and with next if needed. */
                if (!prev_h->flag.isBusy)
                {
                    prev_h->next = next_h->flag.isBusy ? curr_h->next : next_h->next;
                    curr_h = prev_h;
                }
                else
                {
                    /* Otherwise merge only with the following free block. */
                    if (!next_h->flag.isBusy)
                    {
                        curr_h->next = next_h->next;
                    }
                    curr_h->flag.isBusy = 0;
                }

                if (curr_h->next == _last_heap_object[index])
                {
                    _last_heap_object[index] = (char *) curr_h;
                }
            }
            goto FUNC_EXIT;
        }
        prev = curr;
        curr = ((_m_header *) curr)->next;
    }

FUNC_EXIT:
    __ENABLE_IRQ__
    return;
}

