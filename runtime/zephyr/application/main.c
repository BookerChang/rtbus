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
    (void)rtbus_serial_write((const uint8_t *)"application: diagnostics add complete\n",
                             sizeof("application: diagnostics add complete\n") - 1U);

    return 0;
}
