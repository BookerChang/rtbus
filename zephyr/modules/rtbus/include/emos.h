#ifndef RTBUS_EMOS_H_
#define RTBUS_EMOS_H_

#include <stddef.h>
#include <stdint.h>
#ifdef CONFIG_RTBUS_ITERABLE_ENTRY
#include <zephyr/sys/iterable_sections.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define CTX_APP_BITS        28U
#define CTX_SYS_BITS        4U

#define CTX_APP_OFFSET      0U
#define CTX_SYS_OFFSET      28U

#define CTX_APP_MASK        ((1UL << CTX_APP_BITS) - 1UL)
#define CTX_SYS_MASK        ((1UL << CTX_SYS_BITS) - 1UL)

#define CTX_APP_SHIFT(x)    ((((uint32_t)(x)) & CTX_APP_MASK) << CTX_APP_OFFSET)
#define CTX_SYS_SHIFT(x)    ((((uint32_t)(x)) & CTX_SYS_MASK) << CTX_SYS_OFFSET)

#define CTX_APP_ID(x)       (((uint32_t)(x) >> CTX_APP_OFFSET) & CTX_APP_MASK)
#define CTX_SYS_ID(x)       (((uint32_t)(x) >> CTX_SYS_OFFSET) & CTX_SYS_MASK)

#define EMOS_CTX_ID(sys_id, app_id)     (CTX_SYS_SHIFT(sys_id) | CTX_APP_SHIFT(app_id))
#define EMOS_SYS_EVENT(sys_id)          EMOS_CTX_ID((sys_id), 0)
#define EMOS_APP_EVENT(app_id)          EMOS_CTX_ID(EMOS_SYS_NONE, (app_id))
#define EVT_ID(sys_id, app_id)          EMOS_CTX_ID((sys_id), (app_id))
#define SYS_EVT(sys_id)                 EMOS_SYS_EVENT(sys_id)
#define APP_EVT(app_id)                 EMOS_APP_EVENT(app_id)

typedef struct
{
    uint8_t    task_id;
    uint32_t   id;
    uint32_t   payload_len;
    uint8_t    payload[];
} emos_ctx_t;

struct emos_entry
{
    uint8_t task_id;
    uint8_t priority;
    uint8_t (*task_entry)(emos_ctx_t *ctx);
    union
    {
        uint8_t action;
        struct
        {
            uint8_t init:1;
            uint8_t always:1;
        };
    };

} __attribute__((aligned(4)));

typedef struct emos_entry emos_entry_t;

typedef enum
{
    EMOS_TASK_PRIORITY_HIGH = 0,
    EMOS_TASK_PRIORITY_MID,
    EMOS_TASK_PRIORITY_LOW,
    EMOS_TASK_PRIORITY_NUMBER,
} emos_pri_enum;

typedef enum
{
    EMOS_SYS_NONE = 0,
    EMOS_SYS_INIT,
    EMOS_SYS_POLL,
} emos_ctx_sys_enum;

#ifdef CONFIG_RTBUS_ITERABLE_ENTRY
#define EMOS_TASK_REGISTER(_name, _task_id, _priority, _task_entry)           \
    const STRUCT_SECTION_ITERABLE(emos_entry, _name) = {                     \
        .task_id = (_task_id),                                                \
        .priority = (_priority),                                              \
        .task_entry = (_task_entry),                                          \
        .init = 1,                                                            \
    }

#define EMOS_TASK_REGISTER_POLL(_name, _task_id, _priority, _task_entry)      \
    const STRUCT_SECTION_ITERABLE(emos_entry, _name) = {                     \
        .task_id = (_task_id),                                                \
        .priority = (_priority),                                              \
        .task_entry = (_task_entry),                                          \
        .init = 1,                                                            \
        .always = 1,                                                          \
    }
#else
#define EMOS_TASK_REGISTER(_name, _task_id, _priority, _task_entry)
#define EMOS_TASK_REGISTER_POLL(_name, _task_id, _priority, _task_entry)
#endif

#define EMOS_TASK_ENTRY(_task_id, _priority, _task_entry)                     \
    {                                                                         \
        .task_id = (_task_id),                                                \
        .priority = (_priority),                                              \
        .task_entry = (_task_entry),                                          \
        .init = 1,                                                            \
    }

#define EMOS_TASK_ENTRY_POLL(_task_id, _priority, _task_entry)                \
    {                                                                         \
        .task_id = (_task_id),                                                \
        .priority = (_priority),                                              \
        .task_entry = (_task_entry),                                          \
        .init = 1,                                                            \
        .always = 1,                                                          \
    }

#ifdef __cplusplus
}
#endif

#endif /* RTBUS_EMOS_H_ */
