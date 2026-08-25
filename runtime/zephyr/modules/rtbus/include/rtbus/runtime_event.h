/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RTBUS_RUNTIME_EVENT_H_
#define RTBUS_RUNTIME_EVENT_H_

#include <stdint.h>

void runtime_event_signal(void);
int runtime_event_wait(uint32_t timeout_ms);

#endif /* RTBUS_RUNTIME_EVENT_H_ */
