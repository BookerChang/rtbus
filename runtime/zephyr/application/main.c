/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Standalone native application entry for the Zephyr application Makefile path.
 */

#include <stdint.h>

#include "runtime_api.h"

static uint32_t module_count;

__attribute__((used))
int main(void)
{
    if (!runtime_api_is_ready()) {
        return -WZ_ENOSYS;
    }

    (void)k_delay(2000U);

    module_count = runtime_validation_add((int32_t)module_count, 2);
    printk("application: validation add result=%d\n", module_count);

    return 0;
}
