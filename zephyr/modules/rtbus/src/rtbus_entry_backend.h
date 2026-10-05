#ifndef RTBUS_ENTRY_BACKEND_H_
#define RTBUS_ENTRY_BACKEND_H_

#include <stdint.h>

#include "emos.h"

int emos_entries_init(const emos_entry_t *entry_tbl, uint8_t entry_count,
                      uint8_t *registered_count);
const emos_entry_t *emos_entry_find(uint8_t task_id);
const emos_entry_t *emos_entry_get_by_index(uint8_t index);
void emos_entries_reset(void);

#endif /* RTBUS_ENTRY_BACKEND_H_ */
