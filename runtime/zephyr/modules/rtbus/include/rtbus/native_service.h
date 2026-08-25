/*
 * SPDX-License-Identifier: MPL-2.0
 */

#ifndef RTBUS_NATIVE_SERVICE_H_
#define RTBUS_NATIVE_SERVICE_H_

#include <stdbool.h>
#include <stdint.h>

int native_service_last_error(void);
int native_service_emit_event(uint16_t group, uint16_t tag,
                                   const void *value, uint16_t len);
int native_service_complete_result(uint32_t result_addr, int32_t result);
int native_service_handoff_ram_for_patch(void);
void native_service_request_stop_for_upgrade(void);
int native_service_stop_for_upgrade(void);
void native_service_resume_after_upgrade(void);
void native_service_suppress_console(bool suppress);
int native_service_start(void);

#endif /* RTBUS_NATIVE_SERVICE_H_ */
