#ifndef RTBUS_H_
#define RTBUS_H_

#include "emos.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef emos_ctx_t rtbus_ctx_t;
typedef emos_entry_t rtbus_entry_t;
typedef emos_pri_enum rtbus_pri_enum;
typedef emos_ctx_sys_enum rtbus_ctx_sys_enum;

#define RTBUS_CTX_ID(sys_id, app_id)     EMOS_CTX_ID((sys_id), (app_id))
#define RTBUS_SYS_EVENT(sys_id)          EMOS_SYS_EVENT(sys_id)
#define RTBUS_APP_EVENT(app_id)          EMOS_APP_EVENT(app_id)
#define RTBUS_EVT_ID(sys_id, app_id)     EVT_ID((sys_id), (app_id))
#define RTBUS_SYS_EVT(sys_id)            SYS_EVT(sys_id)
#define RTBUS_APP_EVT(app_id)            APP_EVT(app_id)

#define RTBUS_TASK_PRIORITY_HIGH         EMOS_TASK_PRIORITY_HIGH
#define RTBUS_TASK_PRIORITY_MID          EMOS_TASK_PRIORITY_MID
#define RTBUS_TASK_PRIORITY_LOW          EMOS_TASK_PRIORITY_LOW
#define RTBUS_TASK_PRIORITY_NUMBER       EMOS_TASK_PRIORITY_NUMBER

#define RTBUS_SYS_NONE                   EMOS_SYS_NONE
#define RTBUS_SYS_INIT                   EMOS_SYS_INIT
#define RTBUS_SYS_POLL                   EMOS_SYS_POLL

#define RTBUS_TASK_REGISTER(_name, _task_id, _priority, _task_entry)       \
    EMOS_TASK_REGISTER(_name, _task_id, _priority, _task_entry)
#define RTBUS_TASK_REGISTER_POLL(_name, _task_id, _priority, _task_entry)  \
    EMOS_TASK_REGISTER_POLL(_name, _task_id, _priority, _task_entry)
#define RTBUS_TASK_ENTRY(_task_id, _priority, _task_entry)                 \
    EMOS_TASK_ENTRY(_task_id, _priority, _task_entry)
#define RTBUS_TASK_ENTRY_POLL(_task_id, _priority, _task_entry)            \
    EMOS_TASK_ENTRY_POLL(_task_id, _priority, _task_entry)

int rtbus_init(uint8_t *pool_addr, uint16_t pool_size,
                  const rtbus_entry_t *entry_tbl, uint8_t entry_count);
int rtbus_post(uint8_t task_id, uint32_t ctx_id, const void *payload,
                  uint32_t payload_len);
int rtbus_post_delay(uint8_t task_id, uint32_t ctx_id, const void *payload,
                        uint32_t payload_len, uint32_t delay_ms);
void rtbus_post_after(uint8_t task_id, uint32_t ctx_id,
                         const void *payload, uint32_t payload_len);
int rtbus_process_async_posts(void);
int32_t rtbus_next_timeout_ms(void);
int rtbus_process(void);

#ifdef __cplusplus
}
#endif

#endif /* RTBUS_H_ */
