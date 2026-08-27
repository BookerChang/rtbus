/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * Standalone native application entry for the Zephyr application Makefile path.
 */

#include <stdint.h>

#include "runtime_api.h"

static uint32_t module_count;

__attribute__((used))
int main(void)
{
    if (!rtbus_api_is_ready()) {
        return -RTBUS_ENOSYS;
    }

    (void)k_delay(2000U);

    module_count = runtime_diagnostics_add((int32_t)module_count, 2);
    printk("application: diagnostics add result=%d\n", module_count);

    return 0;
}
