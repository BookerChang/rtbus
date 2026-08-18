#ifndef RAK_DEFINE_H_
#define RAK_DEFINE_H_

#include <stddef.h>
#include <stdint.h>

#if defined(CONFIG_SINGLE_APPLICATION_SLOT)
static inline void set_shared_data_rc(uint32_t rc)
{
    (void)rc;
}
#else
void set_shared_data_rc(uint32_t rc);
#endif

#define SHARED_DATA_ADDR CONFIG_WISNODEZ_OTA_SHARED_DATA_ADDR
#define OTA_MAGIC_NUM  0x424F4F54 
typedef struct __packed {
    uint32_t magic;
    union {
        uint8_t all_reason;
        struct {
            uint8_t fouta:1;
            uint8_t User:1;
            uint8_t reserved:6;
        } bits;
    } start_reason;
    uint8_t done;
    uint16_t rc;
} ota_monitor_t;

extern ota_monitor_t * shared_data;

/* FUOTA 錯誤代碼定義 (基於您的規範) */

/* 類別：Session/Reception (0x01-0x02) */
#define FUOTA_ERR_SESSION_CREATE_FAILED       0x0101
#define FUOTA_ERR_FRAGMENT_TIMEOUT            0x0201
#define FUOTA_ERR_TOO_MANY_MISSING_FRAGMENTS  0x0202

/* 類別：Integrity (0x03) */
#define FUOTA_ERR_IMAGE_HASH_MISMATCH         0x0301
#define FUOTA_ERR_IMAGE_SIGNATURE_INVALID     0x0302

/* 類別：Application/Apply (0x04-0x06) */
#define FUOTA_ERR_APPLY_FAILED                0x0401
#define FUOTA_ERR_NEW_FW_BOOT_FAILED          0x0402
#define FUOTA_ERR_ROLLBACK_FAILED             0x0501
#define FUOTA_ERR_LOW_VDD                     0x0601

/* 類別：Flash/Patch (0x07-0x09) */
#define FUOTA_ERR_FLASH_INSUFFICIENT          0x0701
#define FUOTA_ERR_PATCH_BASE_MISMATCH         0x0901
#define FUOTA_ERR_DECOMPRESS_FAILED           0x0902
#define FUOTA_ERR_MODULE_DEPENDENCY_MISMATCH  0x0903

#endif /* MOD_LORAWAN_FUOTA_PATCH_H_ */
