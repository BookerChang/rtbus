/*
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WISMOD_FW_COMPONENT_H_
#define WISMOD_FW_COMPONENT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FW_COMPONENT_NAME_MAX 64U
#define FW_COMPONENT_VERSION_MAX 16U
#define FW_COMPONENT_IMAGE_HEADER_SIZE 128U
#define FW_COMPONENT_IMAGE_PAYLOAD_SIZE_OFFSET 8U
#define FW_COMPONENT_FLAG_NATIVE 0x00000001U
#define FW_COMPONENT_ABI_VERSION_SHIFT 24U
#define FW_COMPONENT_ABI_VERSION_MASK 0xff000000U
#define FW_COMPONENT_FLAGS_MAKE_NATIVE_ABI(_version) \
    (FW_COMPONENT_FLAG_NATIVE | \
     (((uint32_t)(_version) << FW_COMPONENT_ABI_VERSION_SHIFT) & \
      FW_COMPONENT_ABI_VERSION_MASK))
#define FW_COMPONENT_FLAGS_ABI_VERSION(_flags) \
    (((uint32_t)(_flags) & FW_COMPONENT_ABI_VERSION_MASK) >> \
     FW_COMPONENT_ABI_VERSION_SHIFT)

struct fw_component_native_metadata {
    uint32_t entry_offset;
    uint32_t data_load_offset;
    uint32_t data_ram_offset;
    uint32_t data_size;
    uint32_t bss_ram_offset;
    uint32_t bss_size;
};

struct fw_component_store_info {
    size_t image_size;
};

struct fw_component_status {
    bool valid;
    char name[FW_COMPONENT_NAME_MAX];
    char version[FW_COMPONENT_VERSION_MAX];
    size_t payload_size;
    uint32_t payload_crc32;
    uint32_t required_ram_size;
    uint32_t flags;
};

int fw_component_store_begin(const struct fw_component_store_info *info);
int fw_component_store_write(size_t offset, const uint8_t *data, size_t size);
int fw_component_store_finish(int status);
int fw_component_commit_ram_image(uint32_t address, size_t image_size);
int fw_component_get_status(struct fw_component_status *status);
int fw_component_get_native_metadata(struct fw_component_native_metadata *metadata);
int fw_component_read_payload(size_t offset, uint8_t *data, size_t size,
                              size_t *read_size);

#ifdef __cplusplus
}
#endif

#endif /* WISMOD_FW_COMPONENT_H_ */
