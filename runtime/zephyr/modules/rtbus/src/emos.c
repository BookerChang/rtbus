
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/kernel.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../lib/bmalloc.c"
#include "../lib/linklist.c"


#define RTBUS_NO_INIT_MACRO
#include "emos.h"
#include "rtbus_entry_backend.h"

#ifndef EMOS_CORE_API
#define EMOS_CORE_API
#endif

#ifndef EMOS_POST_AFTER
#define EMOS_POST_AFTER emos_post_after
#endif

#ifndef EMOS_PROCESS_ASYNC_POSTS
#define EMOS_PROCESS_ASYNC_POSTS emos_process_async_posts
#endif

#define THIS_POOL_NUM       1

#define POOL_ID  0

typedef struct
{
    void *next;
    uint32_t ctx_storage[];
} emos_proc_list_t;

#define EMOS_PROC_CTX(proc) ((emos_ctx_t *)((proc)->ctx_storage))

static struct
{
    emos_proc_list_t * task_tbl[EMOS_TASK_PRIORITY_NUMBER];
    uint8_t entry_count;
    uint8_t poll_next_index;
} param[THIS_POOL_NUM];

#define entry_count     param[pool_id].entry_count
#define task_tbl        param[pool_id].task_tbl
#define poll_next_index param[pool_id].poll_next_index

#ifndef EMOS_INTERNAL_BUILD
__weak void emos_post_after(uint8_t task_id, uint32_t ctx_id,
                            const void *payload, uint32_t payload_len)
{
    ARG_UNUSED(task_id);
    ARG_UNUSED(ctx_id);
    ARG_UNUSED(payload);
    ARG_UNUSED(payload_len);
}
#endif

EMOS_CORE_API int emos_post(uint8_t task_id, uint32_t ctx_id,
                            const void *payload, uint32_t payload_len)
{
    uint8_t pool_id = POOL_ID;
    uint8_t task_priority = EMOS_TASK_PRIORITY_NUMBER;
    emos_proc_list_t * new_task;
    const emos_entry_t *entry;
    size_t alloc_len;

    if (entry_count == 0) {
        return -ENODEV;
    }

    entry = emos_entry_find(task_id);
    if (entry == NULL) {
        return -ENOENT;
    }

    if (payload_len != 0 && payload == NULL) {
        return -EINVAL;
    }

    task_priority = entry->priority;
    if (task_priority >= EMOS_TASK_PRIORITY_NUMBER) {
        return -EINVAL;
    }

    alloc_len = sizeof(emos_proc_list_t) + sizeof(emos_ctx_t) + payload_len;
    if (alloc_len > UINT16_MAX) {
        return -ENOMEM;
    }

    void **head = (void **)&task_tbl[task_priority];
    new_task = linklist_new(pool_id,
                                   head,
                                   (uint16_t)alloc_len);
    if( new_task == NULL )
    {
        return -ENOMEM;
    }

    emos_ctx_t *ctx = EMOS_PROC_CTX(new_task);

    ctx->task_id = task_id;
    ctx->id = ctx_id;
    ctx->payload_len = payload_len;

    if( (payload != NULL) && (payload_len != 0))
    {
        memcpy(ctx->payload, payload, payload_len);
    }

    EMOS_POST_AFTER(task_id, ctx_id, payload, payload_len);
    return 0;
}

static int emos_init_pool(uint8_t *pool_addr, uint16_t pool_size)
{
    uint8_t pool_id = POOL_ID;

    memset(task_tbl, 0, sizeof(task_tbl));
    emos_entries_reset();
    entry_count = 0;
    poll_next_index = 0;

    if (pool_addr == NULL || pool_size == 0) {
        return -EINVAL;
    }

    init_mem_pool(pool_id, pool_addr, pool_size);

    return 0;
}

EMOS_CORE_API int emos_init(uint8_t *pool_addr, uint16_t pool_size,
                            const emos_entry_t *new_entry_tbl,
                            uint8_t new_entry_count)
{
    uint8_t pool_id = POOL_ID;
    uint8_t registered_count = 0;
    int ret;

    ret = emos_init_pool(pool_addr, pool_size);
    if (ret != 0) {
        return ret;
    }

    ret = emos_entries_init(new_entry_tbl, new_entry_count, &registered_count);
    if (ret != 0) {
        return ret;
    }

    entry_count = registered_count;

    uint32_t ctx_id = 0;
    ctx_id = CTX_SYS_SHIFT(EMOS_SYS_INIT);

    for (uint8_t i = 0; i < entry_count; i++) {
        const emos_entry_t *entry = emos_entry_get_by_index(i);
        if (entry == NULL) {
            return -EINVAL;
        }

        if (entry->init) {
            ret = emos_post(entry->task_id, ctx_id, NULL, 0);
            if (ret != 0) {
                return ret;
            }
        }
    }

    return 0;
}

static int emos_process_queued_event(uint8_t pool_id)
{
    uint8_t task_priority = 0;
    uint8_t task_id = 0;
    const emos_entry_t *this_task;

    do
    {
        while(task_tbl[task_priority] != NULL)
        {
            emos_proc_list_t * emos_proc = task_tbl[task_priority];
            emos_ctx_t *p_ctx = EMOS_PROC_CTX(emos_proc);

            task_id = p_ctx->task_id;
            this_task = emos_entry_find(task_id);
            if (this_task == NULL) {
                void **head = (void **)&task_tbl[task_priority];

                linklist_remove(pool_id, head, emos_proc);
                return 1;
            }

            if(this_task->task_entry != NULL)
            {
                (void)this_task->task_entry(p_ctx);
            }

            void **head = (void **)&task_tbl[task_priority];

            {
                linklist_remove(pool_id, head, emos_proc);
            }

            return 1;
        }
        task_priority++;
    }while(task_priority < EMOS_TASK_PRIORITY_NUMBER );

    return 0;
}

static int emos_process_poll_event(uint8_t pool_id)
{
    uint32_t ctx_storage[(sizeof(emos_ctx_t) + sizeof(uint32_t) - 1) /
                         sizeof(uint32_t)];
    emos_ctx_t *ctx = (emos_ctx_t *)ctx_storage;

    for (uint8_t i = 0; i < entry_count; i++) {
        uint8_t index = (uint8_t)((poll_next_index + i) % entry_count);
        const emos_entry_t *entry = emos_entry_get_by_index(index);

        if (entry == NULL) {
            return 0;
        }

        if (!entry->always || entry->task_entry == NULL) {
            continue;
        }

        ctx->task_id = entry->task_id;
        ctx->id = SYS_EVT(EMOS_SYS_POLL);
        ctx->payload_len = 0;

        (void)entry->task_entry(ctx);

        poll_next_index = (uint8_t)((index + 1) % entry_count);
        return 1;
    }

    return 0;
}

#ifndef EMOS_INTERNAL_BUILD
__weak int emos_process_async_posts(void)
{
    return 0;
}
#endif

EMOS_CORE_API int emos_process( void )
{
    uint8_t pool_id = POOL_ID;
    int ret;

    if (entry_count == 0) {
        return 0;
    }

    ret = EMOS_PROCESS_ASYNC_POSTS();
    if (ret != 0) {
        return ret;
    }

    ret = emos_process_queued_event(pool_id);
    if (ret != 0) {
        return ret;
    }

    return emos_process_poll_event(pool_id);
}
