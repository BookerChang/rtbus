/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "include/runtime_api_core.h"

#define COMPONENT_MAGIC 0x5746434dU
#define HEADER_VERSION 2U
#define HEADER_SIZE 128U
#define FW_COMPONENT_FLAG_NATIVE 0x00000001U
#define FW_COMPONENT_ABI_VERSION_SHIFT 24U
#define FW_COMPONENT_ABI_VERSION_MASK 0xff000000U
#define FW_COMPONENT_FLAGS_MAKE_NATIVE_ABI(_version) \
    (FW_COMPONENT_FLAG_NATIVE | \
     (((uint32_t)(_version) << FW_COMPONENT_ABI_VERSION_SHIFT) & \
      FW_COMPONENT_ABI_VERSION_MASK))
#define IHEX_RECORD_DATA 0x00U
#define IHEX_RECORD_EOF 0x01U
#define IHEX_RECORD_EXT_LINEAR_ADDR 0x04U
#define IHEX_ROW_SIZE 16U
#define PATH_BUFFER_SIZE 1024U

struct application_symbols {
    uint32_t slot_start;
    uint32_t flash_start;
    uint32_t entry;
    uint32_t data_load;
    uint32_t data_start;
    uint32_t data_end;
    uint32_t bss_start;
    uint32_t bss_end;
    uint32_t ram_start;
    uint32_t ram_end;
};

static void put_le16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
}

static void put_le32(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)value;
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value >> 16);
    buf[3] = (uint8_t)(value >> 24);
}

static void put_fixed_string(uint8_t *buf, size_t size, const char *value)
{
    size_t len = strlen(value);

    memset(buf, 0, size);
    if (len >= size) {
        len = size - 1U;
    }
    memcpy(buf, value, len);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = 0U - (crc & 1U);

            crc = (crc >> 1) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *fp;
    long size;
    uint8_t *data;

    fp = fopen(path, "rb");
    if (fp == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", path, strerror(errno));
        return NULL;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return NULL;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return NULL;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return NULL;
    }

    data = malloc((size_t)size);
    if (data == NULL) {
        fclose(fp);
        return NULL;
    }
    if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return NULL;
    }

    fclose(fp);
    *size_out = (size_t)size;
    return data;
}

static int mkdir_one(const char *path)
{
#ifdef _WIN32
    if (_mkdir(path) == 0 || errno == EEXIST) {
#else
    if (mkdir(path, 0777) == 0 || errno == EEXIST) {
#endif
        return 0;
    }

    fprintf(stderr, "mkdir failed: %s: %s\n", path, strerror(errno));
    return -1;
}

static int mkdir_p(const char *path)
{
    char tmp[PATH_BUFFER_SIZE];
    size_t len = strlen(path);

    if (len == 0U || len >= sizeof(tmp)) {
        fprintf(stderr, "invalid directory path: %s\n", path);
        return -1;
    }

    memcpy(tmp, path, len + 1U);
    for (size_t i = 1U; i < len; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char saved = tmp[i];

            tmp[i] = '\0';
            if (mkdir_one(tmp) != 0) {
                return -1;
            }
            tmp[i] = saved;
        }
    }

    return mkdir_one(tmp);
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *last = slash;

    if (backslash != NULL && (last == NULL || backslash > last)) {
        last = backslash;
    }
    return last == NULL ? path : last + 1;
}

static int join_path(char *out, size_t out_size, const char *dir,
                     const char *name)
{
    size_t dir_len = strlen(dir);
    const char *sep = (dir_len > 0U &&
                       (dir[dir_len - 1U] == '/' ||
                        dir[dir_len - 1U] == '\\')) ? "" : "/";
    int written = snprintf(out, out_size, "%s%s%s", dir, sep, name);

    if (written < 0 || (size_t)written >= out_size) {
        fprintf(stderr, "path too long: %s/%s\n", dir, name);
        return -1;
    }
    return 0;
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    uint8_t buf[32768];
    size_t n;

    if (in == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", src, strerror(errno));
        return -1;
    }

    out = fopen(dst, "wb");
    if (out == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", dst, strerror(errno));
        fclose(in);
        return -1;
    }

    while ((n = fread(buf, 1, sizeof(buf), in)) > 0U) {
        if (fwrite(buf, 1, n, out) != n) {
            fprintf(stderr, "write failed: %s\n", dst);
            fclose(out);
            fclose(in);
            return -1;
        }
    }
    if (ferror(in)) {
        fprintf(stderr, "read failed: %s\n", src);
        fclose(out);
        fclose(in);
        return -1;
    }

    fclose(out);
    fclose(in);
    return 0;
}

static int export_file(const char *export_dir, const char *src)
{
    char dst[PATH_BUFFER_SIZE];

    if (join_path(dst, sizeof(dst), export_dir, base_name(src)) != 0) {
        return -1;
    }
    return copy_file(src, dst);
}

static void remove_export_file(const char *export_dir, const char *name)
{
    char path[PATH_BUFFER_SIZE];

    if (join_path(path, sizeof(path), export_dir, name) == 0) {
        remove(path);
    }
}

static int export_outputs(const char *export_dir, const char **paths,
                          size_t path_count)
{
    if (export_dir == NULL || export_dir[0] == '\0') {
        return 0;
    }
    if (mkdir_p(export_dir) != 0) {
        return -1;
    }
    remove_export_file(export_dir, "application.payload.bin");
    remove_export_file(export_dir, "application.signed.bin");
    remove_export_file(export_dir, "application.signed.hex");
    for (size_t i = 0U; i < path_count; i++) {
        if (export_file(export_dir, paths[i]) != 0) {
            return -1;
        }
    }
    printf("Application artifacts: %s\n", export_dir);
    return 0;
}

static int read_text_trimmed(const char *path, char *buf, size_t size)
{
    FILE *fp;
    size_t len;

    if (size == 0U) {
        return -1;
    }

    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fgets(buf, (int)size, fp) == NULL) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    len = strlen(buf);
    while (len > 0U && (buf[len - 1U] == '\n' || buf[len - 1U] == '\r')) {
        buf[--len] = '\0';
    }

    return len == 0U ? -1 : 0;
}

static int write_component_file(const char *path, const uint8_t *header,
                                const uint8_t *payload, size_t payload_size)
{
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fwrite(header, 1, HEADER_SIZE, fp) != HEADER_SIZE ||
        fwrite(payload, 1, payload_size, fp) != payload_size) {
        fprintf(stderr, "write failed: %s\n", path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int write_binary_file(const char *path, const uint8_t *data, size_t size)
{
    FILE *fp = fopen(path, "wb");

    if (fp == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", path, strerror(errno));
        return -1;
    }
    if (fwrite(data, 1, size, fp) != size) {
        fprintf(stderr, "write failed: %s\n", path);
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int write_ihex_record(FILE *fp, uint8_t type, uint16_t address,
                             const uint8_t *data, uint8_t len)
{
    uint8_t sum = len + (uint8_t)(address >> 8) + (uint8_t)address + type;

    if (fprintf(fp, ":%02X%04X%02X", len, address, type) < 0) {
        return -1;
    }

    for (uint8_t i = 0; i < len; i++) {
        sum = (uint8_t)(sum + data[i]);
        if (fprintf(fp, "%02X", data[i]) < 0) {
            return -1;
        }
    }

    if (fprintf(fp, "%02X\n", (uint8_t)(0U - sum)) < 0) {
        return -1;
    }

    return 0;
}

static int write_component_ihex(const char *path, uint32_t base_addr,
                                const uint8_t *header, const uint8_t *payload,
                                size_t payload_size)
{
    FILE *fp = fopen(path, "w");
    uint32_t address = base_addr;
    uint32_t current_upper = UINT32_MAX;
    size_t offset = 0U;
    size_t total_size = HEADER_SIZE + payload_size;

    if (fp == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", path, strerror(errno));
        return -1;
    }

    while (offset < total_size) {
        uint8_t row[IHEX_ROW_SIZE];
        size_t row_len = total_size - offset;
        uint32_t upper = address >> 16;

        if (row_len > sizeof(row)) {
            row_len = sizeof(row);
        }

        for (size_t i = 0; i < row_len; i++) {
            size_t source_offset = offset + i;

            if (source_offset < HEADER_SIZE) {
                row[i] = header[source_offset];
            } else {
                row[i] = payload[source_offset - HEADER_SIZE];
            }
        }

        if (upper != current_upper) {
            uint8_t upper_data[2] = {
                (uint8_t)(upper >> 8),
                (uint8_t)upper,
            };

            if (write_ihex_record(fp, IHEX_RECORD_EXT_LINEAR_ADDR, 0U,
                                  upper_data, sizeof(upper_data)) != 0) {
                fclose(fp);
                return -1;
            }
            current_upper = upper;
        }

        if (write_ihex_record(fp, IHEX_RECORD_DATA, (uint16_t)address,
                              row, (uint8_t)row_len) != 0) {
            fclose(fp);
            return -1;
        }

        address += (uint32_t)row_len;
        offset += row_len;
    }

    if (write_ihex_record(fp, IHEX_RECORD_EOF, 0U, NULL, 0U) != 0) {
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

static int write_slot_address(const char *path, uint32_t slot_start)
{
    FILE *fp = fopen(path, "w");

    if (fp == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(fp, "0x%08x\n", slot_start);
    fclose(fp);
    return 0;
}

static void set_symbol(struct application_symbols *symbols, const char *name,
                       uint32_t address)
{
    if (strcmp(name, "__module_slot_start") == 0) {
        symbols->slot_start = address;
    } else if (strcmp(name, "__module_flash_start") == 0) {
        symbols->flash_start = address;
    } else if (strcmp(name, "__module_entry") == 0) {
        symbols->entry = address;
    } else if (strcmp(name, "__module_data_load") == 0) {
        symbols->data_load = address;
    } else if (strcmp(name, "__module_data_start") == 0) {
        symbols->data_start = address;
    } else if (strcmp(name, "__module_data_end") == 0) {
        symbols->data_end = address;
    } else if (strcmp(name, "__module_bss_start") == 0) {
        symbols->bss_start = address;
    } else if (strcmp(name, "__module_bss_end") == 0) {
        symbols->bss_end = address;
    } else if (strcmp(name, "__module_ram_start") == 0) {
        symbols->ram_start = address;
    } else if (strcmp(name, "__module_ram_end") == 0) {
        symbols->ram_end = address;
    }
}

static int read_symbols_from_map(const char *path, struct application_symbols *symbols)
{
    FILE *fp = fopen(path, "r");
    char line[256];

    if (fp == NULL) {
        fprintf(stderr, "open failed: %s: %s\n", path, strerror(errno));
        return -1;
    }

    memset(symbols, 0, sizeof(*symbols));
    while (fgets(line, sizeof(line), fp) != NULL) {
        unsigned long address;
        char name[128];

        if (sscanf(line, " %lx %127s", &address, name) == 2) {
            set_symbol(symbols, name, (uint32_t)address);
        }
    }

    fclose(fp);
    return 0;
}

static int validate_symbols(const struct application_symbols *symbols,
                            uint32_t *ram_size_out)
{
    uint32_t ram_size;

    if (symbols->slot_start == 0U || symbols->flash_start == 0U ||
        symbols->entry == 0U || symbols->data_load == 0U ||
        symbols->data_start == 0U || symbols->data_end == 0U ||
        symbols->bss_start == 0U || symbols->bss_end == 0U ||
        symbols->ram_start == 0U || symbols->ram_end == 0U) {
        fprintf(stderr, "missing application symbols\n");
        return -1;
    }
    if (symbols->ram_end <= symbols->ram_start) {
        fprintf(stderr, "invalid module RAM range: 0x%08x..0x%08x\n",
                symbols->ram_start, symbols->ram_end);
        return -1;
    }

    ram_size = symbols->ram_end - symbols->ram_start;
    if (symbols->data_start < symbols->ram_start ||
        symbols->data_end > symbols->ram_end ||
        symbols->bss_start < symbols->ram_start ||
        symbols->bss_end > symbols->ram_end) {
        fprintf(stderr, "module sections exceed module RAM range\n");
        return -1;
    }

    *ram_size_out = ram_size;
    return 0;
}

static void build_header(uint8_t *header, const struct application_symbols *symbols,
                         const char *name, const char *version,
                         uint32_t ram_size, uint32_t payload_size,
                         uint32_t payload_crc)
{
    uint8_t *reserved = header + 104U;

    memset(header, 0, HEADER_SIZE);
    put_le32(header + 0U, COMPONENT_MAGIC);
    put_le16(header + 4U, HEADER_VERSION);
    put_le16(header + 6U, HEADER_SIZE);
    put_le32(header + 8U, payload_size);
    put_le32(header + 12U, payload_crc);
    put_fixed_string(header + 16U, 16U, version);
    put_fixed_string(header + 32U, 64U, name);
    put_le32(header + 96U, ram_size);
    put_le32(header + 100U, FW_COMPONENT_FLAGS_MAKE_NATIVE_ABI(WZ_API_VERSION));

    put_le32(reserved + 0U, symbols->entry - symbols->flash_start);
    put_le32(reserved + 4U, symbols->data_load - symbols->flash_start);
    put_le32(reserved + 8U, symbols->data_start - symbols->ram_start);
    put_le32(reserved + 12U, symbols->data_end - symbols->data_start);
    put_le32(reserved + 16U, symbols->bss_start - symbols->ram_start);
    put_le32(reserved + 20U, symbols->bss_end - symbols->bss_start);
}

int main(int argc, char **argv)
{
    struct application_symbols symbols;
    uint8_t header[HEADER_SIZE];
    uint8_t *payload;
    size_t payload_size;
    uint32_t ram_size;
    uint32_t crc;
    char version_base[32];
    char version_build[32];
    char version[64];

    if (argc != 12 && argc != 13 && argc != 15) {
        fprintf(stderr,
                "usage: %s <pack> <project> <payload.bin> <payload.out> <map> "
                "<signed.bin> <signed.hex> <slot.txt> <name> <version_file> "
                "<build_file> [export_dir] [raw.hex raw.elf export_dir]\n",
                argv[0]);
        return 2;
    }

    if (strcmp(argv[1], "true") != 0 && strcmp(argv[1], "1") != 0 &&
        strcmp(argv[1], "yes") != 0 && strcmp(argv[1], "on") != 0) {
        return 0;
    }

    if (read_text_trimmed(argv[10], version_base, sizeof(version_base)) != 0 ||
        read_text_trimmed(argv[11], version_build, sizeof(version_build)) != 0) {
        return 1;
    }
    snprintf(version, sizeof(version), "%s+%s", version_base, version_build);

    if (read_symbols_from_map(argv[5], &symbols) != 0 ||
        validate_symbols(&symbols, &ram_size) != 0) {
        return 1;
    }

    payload = read_file(argv[3], &payload_size);
    if (payload == NULL) {
        return 1;
    }

    crc = crc32_update(0U, payload, payload_size);
    build_header(header, &symbols, argv[9], version, ram_size,
                 (uint32_t)payload_size, crc);

    if (write_binary_file(argv[4], payload, payload_size) != 0 ||
        write_component_file(argv[6], header, payload, payload_size) != 0 ||
        write_component_ihex(argv[7], symbols.slot_start, header, payload,
                             payload_size) != 0 ||
        write_slot_address(argv[8], symbols.slot_start) != 0) {
        free(payload);
        return 1;
    }

    if (argc == 13) {
        const char *outputs[] = {
            argv[3],
            argv[4],
            argv[5],
            argv[6],
            argv[7],
            argv[8],
        };

        if (export_outputs(argv[12], outputs,
                           sizeof(outputs) / sizeof(outputs[0])) != 0) {
            free(payload);
            return 1;
        }
    } else if (argc == 15) {
        const char *outputs[] = {
            argv[3],
            argv[4],
            argv[5],
            argv[6],
            argv[7],
            argv[8],
            argv[12],
            argv[13],
        };

        if (export_outputs(argv[14], outputs,
                           sizeof(outputs) / sizeof(outputs[0])) != 0) {
            free(payload);
            return 1;
        }
    }

    printf("Application image: %s name=%s version=%s payload=%u ram=%u "
           "entry=0x%08x data=%u bss=%u crc=0x%08x\n",
           argv[6], argv[9], version, (unsigned int)payload_size,
           (unsigned int)ram_size, symbols.entry,
           (unsigned int)(symbols.data_end - symbols.data_start),
           (unsigned int)(symbols.bss_end - symbols.bss_start), crc);
    printf("Application HEX: %s slot=0x%08x\n", argv[7], symbols.slot_start);

    free(payload);
    return 0;
}
