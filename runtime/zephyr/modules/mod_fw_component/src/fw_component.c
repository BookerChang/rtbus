/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include "fw_component.h"

#include <errno.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

LOG_MODULE_REGISTER(fw_component, LOG_LEVEL_INF);

#define FW_COMPONENT_MAGIC 0x5746434dU /* WFCM */
#define FW_COMPONENT_HEADER_VERSION 2U
#define FW_COMPONENT_HEADER_SIZE FW_COMPONENT_IMAGE_HEADER_SIZE
#define FW_COMPONENT_SLOT_AREA_ID PARTITION_ID(component_slot_partition)
#if DT_NODE_EXISTS(DT_NODELABEL(patch_partition))
#define FW_COMPONENT_DIRECT_STORE_AREA_ID PARTITION_ID(patch_partition)
#else
#define FW_COMPONENT_DIRECT_STORE_AREA_ID FW_COMPONENT_SLOT_AREA_ID
#endif
#define FW_COMPONENT_RAM_NODE DT_NODELABEL(shared_upgrade_ram)
#define FW_COMPONENT_RAM_BASE DT_REG_ADDR(FW_COMPONENT_RAM_NODE)
#define FW_COMPONENT_RAM_SIZE DT_REG_SIZE(FW_COMPONENT_RAM_NODE)
#define FW_COMPONENT_FLASH_PENDING_MAX 16U

struct fw_component_image_header {
    uint32_t magic;
    uint16_t header_version;
    uint16_t header_size;
    uint32_t payload_size;
    uint32_t payload_crc32;
    char version[FW_COMPONENT_VERSION_MAX];
    char name[FW_COMPONENT_NAME_MAX];
    uint32_t required_ram_size;
    uint32_t flags;
    uint8_t reserved[24];
};

BUILD_ASSERT(sizeof(struct fw_component_image_header) == FW_COMPONENT_HEADER_SIZE);
BUILD_ASSERT(offsetof(struct fw_component_image_header, payload_size) ==
             FW_COMPONENT_IMAGE_PAYLOAD_SIZE_OFFSET);
BUILD_ASSERT(sizeof(struct fw_component_native_metadata) <=
             sizeof(((struct fw_component_image_header *)0)->reserved));

enum fw_component_store_backend {
    FW_COMPONENT_STORE_BACKEND_NONE,
    FW_COMPONENT_STORE_BACKEND_RAM,
    FW_COMPONENT_STORE_BACKEND_FLASH,
};

struct fw_component_store_context {
    enum fw_component_store_backend backend;
    uint8_t *ram_buf;
    size_t ram_buf_size;
    const struct flash_area *flash_area;
    size_t flash_write_offset;
    size_t flash_align;
    uint8_t flash_pending[FW_COMPONENT_FLASH_PENDING_MAX];
    size_t flash_pending_len;
    size_t expected_size;
    size_t written;
    bool active;
};

static struct fw_component_store_context store_ctx;

static int fw_component_open_slot(const struct flash_area **area)
{
    return flash_area_open(FW_COMPONENT_SLOT_AREA_ID, area);
}

#if defined(CONFIG_WISMOD_FW_COMPONENT_STORE_DIRECT_FLASH)
static int fw_component_open_direct_store(const struct flash_area **area)
{
    return flash_area_open(FW_COMPONENT_DIRECT_STORE_AREA_ID, area);
}
#endif

static size_t fw_component_payload_capacity(const struct flash_area *area)
{
    if (area->fa_size <= FW_COMPONENT_HEADER_SIZE) {
        return 0U;
    }

    return area->fa_size - FW_COMPONENT_HEADER_SIZE;
}

static void fw_component_store_reset(void)
{
    if (store_ctx.flash_area != NULL) {
        flash_area_close(store_ctx.flash_area);
    }

    memset(&store_ctx, 0, sizeof(store_ctx));
}

static int fw_component_store_read(size_t offset, void *data, size_t size)
{
    if (data == NULL || offset > store_ctx.written ||
        size > (store_ctx.written - offset)) {
        return -EINVAL;
    }

    if (store_ctx.backend == FW_COMPONENT_STORE_BACKEND_RAM) {
        if (store_ctx.ram_buf == NULL || offset > store_ctx.ram_buf_size ||
            size > (store_ctx.ram_buf_size - offset)) {
            return -EINVAL;
        }

        memcpy(data, store_ctx.ram_buf + offset, size);
        return 0;
    }

    if (store_ctx.backend == FW_COMPONENT_STORE_BACKEND_FLASH) {
        if (store_ctx.flash_area == NULL ||
            offset > store_ctx.flash_area->fa_size ||
            size > (store_ctx.flash_area->fa_size - offset)) {
            return -EINVAL;
        }

        return flash_area_read(store_ctx.flash_area, (off_t)offset, data, size);
    }

    return -EINVAL;
}

static int fw_component_read_header_from_area(const struct flash_area *area,
                                              struct fw_component_image_header *header)
{
    int rc;

    if (area == NULL || header == NULL) {
        return -EINVAL;
    }

    rc = flash_area_read(area, 0, header, sizeof(*header));
    if (rc != 0) {
        return rc;
    }

    if (header->magic != FW_COMPONENT_MAGIC ||
        header->header_version != FW_COMPONENT_HEADER_VERSION ||
        header->header_size != FW_COMPONENT_HEADER_SIZE ||
        header->payload_size == 0U ||
        header->payload_size > fw_component_payload_capacity(area)) {
        return -ENODATA;
    }

    return 0;
}

static int fw_component_read_header_from_store(struct fw_component_image_header *header)
{
    if (header == NULL) {
        return -EINVAL;
    }

    if (fw_component_store_read(0, header, sizeof(*header)) != 0 ||
        header->magic != FW_COMPONENT_MAGIC ||
        header->header_version != FW_COMPONENT_HEADER_VERSION ||
        header->header_size != FW_COMPONENT_HEADER_SIZE ||
        header->payload_size == 0U ||
        header->payload_size > (store_ctx.expected_size - FW_COMPONENT_HEADER_SIZE)) {
        return -ENODATA;
    }

    return 0;
}

static int fw_component_calculate_payload_crc(const struct flash_area *area,
                                              size_t payload_size,
                                              uint32_t *crc_out)
{
    uint8_t buf[128];
    size_t offset = 0U;
    uint32_t crc = 0U;
    int rc;

    if (area == NULL || crc_out == NULL) {
        return -EINVAL;
    }

    while (offset < payload_size) {
        size_t read_len = MIN(sizeof(buf), payload_size - offset);

        rc = flash_area_read(area, FW_COMPONENT_HEADER_SIZE + offset, buf, read_len);
        if (rc != 0) {
            return rc;
        }

        crc = crc32_ieee_update(crc, buf, read_len);
        offset += read_len;
    }

    *crc_out = crc;
    return 0;
}

static int fw_component_calculate_store_payload_crc(size_t payload_size,
                                                    uint32_t *crc_out)
{
    uint8_t buf[128];
    size_t offset = 0U;
    uint32_t crc = 0U;
    int rc;

    if (crc_out == NULL) {
        return -EINVAL;
    }

    while (offset < payload_size) {
        size_t read_len = MIN(sizeof(buf), payload_size - offset);

        rc = fw_component_store_read(FW_COMPONENT_HEADER_SIZE + offset, buf, read_len);
        if (rc != 0) {
            return rc;
        }

        crc = crc32_ieee_update(crc, buf, read_len);
        offset += read_len;
    }

    *crc_out = crc;
    return 0;
}

static int fw_component_commit_ram_to_slot(void)
{
    const struct flash_area *area;
    int rc;

    if (store_ctx.ram_buf == NULL || store_ctx.written != store_ctx.expected_size) {
        return -EINVAL;
    }

    rc = fw_component_open_slot(&area);
    if (rc != 0) {
        return rc;
    }

    if (store_ctx.expected_size > area->fa_size) {
        flash_area_close(area);
        return -EFBIG;
    }

    rc = flash_area_flatten(area, 0, area->fa_size);
    if (rc == 0) {
        rc = flash_area_write(area, 0, store_ctx.ram_buf, store_ctx.written);
    }

    flash_area_close(area);
    return rc;
}

static int fw_component_commit_store_to_slot(void)
{
    const struct flash_area *area;
    uint8_t buf[128];
    size_t offset = 0U;
    size_t slot_align;
    int rc;

    if (store_ctx.flash_area == NULL ||
        store_ctx.written != store_ctx.expected_size) {
        return -EINVAL;
    }

    if (store_ctx.flash_area->fa_id == FW_COMPONENT_SLOT_AREA_ID) {
        return 0;
    }

    rc = fw_component_open_slot(&area);
    if (rc != 0) {
        return rc;
    }

    if (store_ctx.expected_size > area->fa_size) {
        flash_area_close(area);
        return -EFBIG;
    }

    slot_align = flash_area_align(area);
    if (slot_align == 0U || slot_align > sizeof(buf)) {
        flash_area_close(area);
        return -EINVAL;
    }

    rc = flash_area_flatten(area, 0, area->fa_size);
    while (rc == 0 && offset < store_ctx.expected_size) {
        size_t read_len = MIN(sizeof(buf), store_ctx.expected_size - offset);
        size_t write_len = ROUND_UP(read_len, slot_align);

        rc = flash_area_read(store_ctx.flash_area, (off_t)offset, buf, read_len);
        if (rc != 0) {
            break;
        }

        if (write_len > read_len) {
            memset(buf + read_len, 0xff, write_len - read_len);
        }

        rc = flash_area_write(area, (off_t)offset, buf, write_len);
        offset += read_len;
    }

    flash_area_close(area);
    return rc;
}

static int fw_component_store_flush_flash(bool final)
{
    int rc;

    if (store_ctx.flash_area == NULL || store_ctx.flash_align == 0U ||
        store_ctx.flash_align > sizeof(store_ctx.flash_pending)) {
        return -EINVAL;
    }

    if (store_ctx.flash_pending_len == 0U) {
        return 0;
    }

    if (!final && store_ctx.flash_pending_len < store_ctx.flash_align) {
        return 0;
    }

    memset(store_ctx.flash_pending + store_ctx.flash_pending_len, 0xff,
           store_ctx.flash_align - store_ctx.flash_pending_len);

    rc = flash_area_write(store_ctx.flash_area,
                          (off_t)store_ctx.flash_write_offset,
                          store_ctx.flash_pending,
                          store_ctx.flash_align);
    if (rc != 0) {
        return rc;
    }

    store_ctx.flash_write_offset += store_ctx.flash_align;
    store_ctx.flash_pending_len = 0U;
    return 0;
}

static int fw_component_store_write_flash(size_t offset, const uint8_t *data,
                                          size_t size)
{
    if (store_ctx.flash_area == NULL || data == NULL ||
        offset != store_ctx.written) {
        return -EINVAL;
    }

    while (size > 0U) {
        size_t copy_len = MIN(size, store_ctx.flash_align - store_ctx.flash_pending_len);
        int rc;

        memcpy(store_ctx.flash_pending + store_ctx.flash_pending_len, data, copy_len);
        store_ctx.flash_pending_len += copy_len;
        data += copy_len;
        size -= copy_len;

        rc = fw_component_store_flush_flash(false);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

int fw_component_store_begin(const struct fw_component_store_info *info)
{
    size_t slot_size;
#if !defined(CONFIG_WISMOD_FW_COMPONENT_STORE_DIRECT_FLASH)
    size_t staging_size = FW_COMPONENT_RAM_SIZE;
#endif

    if (info == NULL || info->image_size < FW_COMPONENT_HEADER_SIZE) {
        return -EINVAL;
    }

    if (store_ctx.active) {
        return -EBUSY;
    }

    slot_size = DT_REG_SIZE(DT_NODELABEL(component_slot_partition));

    memset(&store_ctx, 0, sizeof(store_ctx));
    if (info->image_size > slot_size) {
        return -EFBIG;
    }

#if defined(CONFIG_WISMOD_FW_COMPONENT_STORE_DIRECT_FLASH)
    const struct flash_area *area;
    int rc;

    rc = fw_component_open_direct_store(&area);
    if (rc != 0) {
        fw_component_store_reset();
        return rc;
    }

    if (!device_is_ready(area->fa_dev)) {
        flash_area_close(area);
        fw_component_store_reset();
        return -ENODEV;
    }

    if (info->image_size > area->fa_size) {
        flash_area_close(area);
        fw_component_store_reset();
        return -EFBIG;
    }

    store_ctx.flash_align = flash_area_align(area);
    if (store_ctx.flash_align == 0U ||
        store_ctx.flash_align > sizeof(store_ctx.flash_pending)) {
        flash_area_close(area);
        fw_component_store_reset();
        return -EINVAL;
    }

    rc = flash_area_flatten(area, 0, area->fa_size);
    if (rc != 0) {
        flash_area_close(area);
        fw_component_store_reset();
        return rc;
    }

    store_ctx.backend = FW_COMPONENT_STORE_BACKEND_FLASH;
    store_ctx.flash_area = area;
    store_ctx.expected_size = info->image_size;
    store_ctx.active = true;
    memset(store_ctx.flash_pending, 0xff, sizeof(store_ctx.flash_pending));

    return 0;
#else
    if (info->image_size > staging_size) {
        return -ENOSPC;
    }

    store_ctx.backend = FW_COMPONENT_STORE_BACKEND_RAM;
    store_ctx.ram_buf = (uint8_t *)(uintptr_t)FW_COMPONENT_RAM_BASE;
    store_ctx.ram_buf_size = staging_size;
    store_ctx.expected_size = info->image_size;
    store_ctx.active = true;
    memset(store_ctx.ram_buf, 0xff, store_ctx.ram_buf_size);

    return 0;
#endif
}

int fw_component_store_write(size_t offset, const uint8_t *data, size_t size)
{
    if (!store_ctx.active || data == NULL) {
        return -EINVAL;
    }

    if (offset != store_ctx.written) {
        return -EINVAL;
    }

    if (size > (store_ctx.expected_size - store_ctx.written)) {
        return -EFBIG;
    }

    if (store_ctx.backend == FW_COMPONENT_STORE_BACKEND_RAM) {
        if (store_ctx.ram_buf == NULL ||
            (store_ctx.written + size) > store_ctx.ram_buf_size) {
            return -ENOSPC;
        }
        memcpy(store_ctx.ram_buf + offset, data, size);
    } else if (store_ctx.backend == FW_COMPONENT_STORE_BACKEND_FLASH) {
        int rc = fw_component_store_write_flash(offset, data, size);

        if (rc != 0) {
            return rc;
        }
    } else {
        return -EINVAL;
    }

    store_ctx.written += size;
    return 0;
}

int fw_component_store_finish(int status)
{
    struct fw_component_image_header header;
    uint32_t crc = 0U;
    int rc = 0;

    if (!store_ctx.active) {
        return -EINVAL;
    }

    if (status == 0 && store_ctx.written != store_ctx.expected_size) {
        status = -EIO;
    }

    if (status != 0) {
        rc = status;
        goto out;
    }

    if (store_ctx.backend == FW_COMPONENT_STORE_BACKEND_FLASH) {
        rc = fw_component_store_flush_flash(true);
        if (rc != 0) {
            goto out;
        }
    }

    rc = fw_component_read_header_from_store(&header);
    if (rc != 0) {
        goto out;
    }

    if (header.payload_size > (store_ctx.expected_size - FW_COMPONENT_HEADER_SIZE)) {
        rc = -EBADMSG;
        goto out;
    }

    if (header.required_ram_size > CONFIG_WISMOD_FW_COMPONENT_RAM_MAX) {
        LOG_ERR("Component requires %u bytes RAM, max is %u",
                header.required_ram_size, CONFIG_WISMOD_FW_COMPONENT_RAM_MAX);
        rc = -ENOMEM;
        goto out;
    }

    rc = fw_component_calculate_store_payload_crc(header.payload_size, &crc);
    if (rc != 0) {
        goto out;
    }

    if (crc != header.payload_crc32) {
        rc = -EBADMSG;
        goto out;
    }

    if (store_ctx.backend == FW_COMPONENT_STORE_BACKEND_RAM) {
        rc = fw_component_commit_ram_to_slot();
    } else if (store_ctx.backend == FW_COMPONENT_STORE_BACKEND_FLASH) {
        rc = fw_component_commit_store_to_slot();
    }

out:
    fw_component_store_reset();
    return rc;
}

int fw_component_commit_ram_image(uint32_t address, size_t image_size)
{
    uintptr_t ram_base = (uintptr_t)FW_COMPONENT_RAM_BASE;
    uintptr_t ram_end = ram_base + FW_COMPONENT_RAM_SIZE;
    uintptr_t image_addr = (uintptr_t)address;

    if (image_size < FW_COMPONENT_HEADER_SIZE ||
        image_addr < ram_base ||
        image_addr > ram_end ||
        image_size > (ram_end - image_addr)) {
        return -EINVAL;
    }

    if (store_ctx.active) {
        return -EBUSY;
    }

    memset(&store_ctx, 0, sizeof(store_ctx));
    store_ctx.backend = FW_COMPONENT_STORE_BACKEND_RAM;
    store_ctx.ram_buf = (uint8_t *)image_addr;
    store_ctx.ram_buf_size = image_size;
    store_ctx.expected_size = image_size;
    store_ctx.written = image_size;
    store_ctx.active = true;

    return fw_component_store_finish(0);
}

int fw_component_get_status(struct fw_component_status *status)
{
    struct fw_component_image_header header;
    const struct flash_area *area;
    uint32_t crc = 0U;
    int rc;

    if (status == NULL) {
        return -EINVAL;
    }

    memset(status, 0, sizeof(*status));

    rc = fw_component_open_slot(&area);
    if (rc != 0) {
        return rc;
    }

    rc = fw_component_read_header_from_area(area, &header);
    if (rc != 0) {
        flash_area_close(area);
        return rc;
    }

    rc = fw_component_calculate_payload_crc(area, header.payload_size, &crc);
    flash_area_close(area);
    if (rc != 0) {
        return rc;
    }

    strncpy(status->name, header.name, sizeof(status->name) - 1U);
    strncpy(status->version, header.version, sizeof(status->version) - 1U);
    status->payload_size = header.payload_size;
    status->payload_crc32 = header.payload_crc32;
    status->required_ram_size = header.required_ram_size;
    status->flags = header.flags;
    status->valid = (crc == header.payload_crc32);

    return status->valid ? 0 : -EBADMSG;
}

int fw_component_get_native_metadata(struct fw_component_native_metadata *metadata)
{
    struct fw_component_image_header header;
    const struct flash_area *area;
    int rc;

    if (metadata == NULL) {
        return -EINVAL;
    }

    memset(metadata, 0, sizeof(*metadata));

    rc = fw_component_open_slot(&area);
    if (rc != 0) {
        return rc;
    }

    rc = fw_component_read_header_from_area(area, &header);
    flash_area_close(area);
    if (rc != 0) {
        return rc;
    }

    if ((header.flags & FW_COMPONENT_FLAG_NATIVE) == 0U) {
        return -ENOTSUP;
    }

    memcpy(metadata, header.reserved, sizeof(*metadata));
    return 0;
}

int fw_component_read_payload(size_t offset, uint8_t *data, size_t size,
                              size_t *read_size)
{
    struct fw_component_image_header header;
    const struct flash_area *area;
    size_t len;
    int rc;

    if (data == NULL || read_size == NULL) {
        return -EINVAL;
    }

    *read_size = 0U;

    rc = fw_component_open_slot(&area);
    if (rc != 0) {
        return rc;
    }

    rc = fw_component_read_header_from_area(area, &header);
    if (rc != 0) {
        flash_area_close(area);
        return rc;
    }

    if (offset >= header.payload_size) {
        flash_area_close(area);
        return 0;
    }

    len = MIN(size, header.payload_size - offset);
    rc = flash_area_read(area, FW_COMPONENT_HEADER_SIZE + offset, data, len);
    flash_area_close(area);
    if (rc != 0) {
        return rc;
    }

    *read_size = len;
    return 0;
}
