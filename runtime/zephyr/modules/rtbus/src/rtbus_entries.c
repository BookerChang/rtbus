#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "rtbus_entry_backend.h"

static const emos_entry_t *registered_entries;
static uint8_t registered_entry_count;

void emos_entries_reset(void)
{
    registered_entries = NULL;
    registered_entry_count = 0;
}

const emos_entry_t *emos_entry_find(uint8_t task_id)
{
    if (registered_entries == NULL) {
        return NULL;
    }

    for (uint8_t i = 0; i < registered_entry_count; i++) {
        if (registered_entries[i].task_id == task_id) {
            return &registered_entries[i];
        }
    }

    return NULL;
}

const emos_entry_t *emos_entry_get_by_index(uint8_t index)
{
    if (registered_entries == NULL || index >= registered_entry_count) {
        return NULL;
    }

    return &registered_entries[index];
}

static bool emos_entry_id_is_duplicate(const emos_entry_t *entry)
{
    uint8_t matches = 0;

    for (uint8_t i = 0; i < registered_entry_count; i++) {
        if (registered_entries[i].task_id == entry->task_id) {
            matches++;
        }
    }

    return matches > 1;
}

int emos_entries_init(const emos_entry_t *entry_tbl, uint8_t entry_count,
                      uint8_t *registered_count)
{
    if (entry_tbl == NULL || entry_count == 0 || registered_count == NULL) {
        return -EINVAL;
    }

    registered_entries = entry_tbl;
    registered_entry_count = entry_count;

    for (uint8_t i = 0; i < registered_entry_count; i++) {
        if (registered_entries[i].priority >= EMOS_TASK_PRIORITY_NUMBER ||
            registered_entries[i].task_entry == NULL) {
            emos_entries_reset();
            return -EINVAL;
        }

        if (emos_entry_id_is_duplicate(&registered_entries[i])) {
            emos_entries_reset();
            return -EALREADY;
        }
    }

    *registered_count = registered_entry_count;
    return 0;
}
