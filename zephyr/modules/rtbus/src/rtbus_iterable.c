#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/sys/iterable_sections.h>

#include "rtbus_entry_backend.h"

void emos_entries_reset(void)
{
}

const emos_entry_t *emos_entry_find(uint8_t task_id)
{
    STRUCT_SECTION_FOREACH(emos_entry, entry) {
        if (entry->task_id == task_id) {
            return entry;
        }
    }

    return NULL;
}

const emos_entry_t *emos_entry_get_by_index(uint8_t index)
{
    uint8_t current = 0;

    STRUCT_SECTION_FOREACH(emos_entry, entry) {
        if (current == index) {
            return entry;
        }

        current++;
    }

    return NULL;
}

static bool emos_entry_id_is_duplicate(const emos_entry_t *needle)
{
    uint8_t matches = 0;

    STRUCT_SECTION_FOREACH(emos_entry, entry) {
        if (entry->task_id == needle->task_id) {
            matches++;
        }
    }

    return matches > 1;
}

int emos_entries_init(const emos_entry_t *entry_tbl, uint8_t entry_count,
                      uint8_t *registered_count)
{
    uint8_t found = 0;

    ARG_UNUSED(entry_tbl);
    ARG_UNUSED(entry_count);

    if (registered_count == NULL) {
        return -EINVAL;
    }

    STRUCT_SECTION_FOREACH(emos_entry, entry) {
        if (entry->priority >= EMOS_TASK_PRIORITY_NUMBER ||
            entry->task_entry == NULL) {
            return -EINVAL;
        }

        if (emos_entry_id_is_duplicate(entry)) {
            return -EALREADY;
        }

        found++;
    }

    if (found == 0) {
        return -ENODEV;
    }

    *registered_count = found;
    return 0;
}
