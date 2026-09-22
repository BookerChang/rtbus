/*
 * SPDX-License-Identifier: MPL-2.0
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#endif

#define YMODEM_SOH 0x01
#define YMODEM_STX 0x02
#define YMODEM_EOT 0x04
#define YMODEM_ACK 0x06
#define YMODEM_NAK 0x15
#define YMODEM_CAN 0x18
#define YMODEM_CRC 'C'

#define YMODEM_BLOCK_128 128U
#define YMODEM_BLOCK_1024 1024U
#define SERIAL_READ_SLICE_MS 20
#define READY_TIMEOUT_MS 15000
#define PACKET_TIMEOUT_MS 10000
#define FINAL_TIMEOUT_MS 1500

struct serial_port {
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
};

static int64_t now_ms(void)
{
#ifdef _WIN32
    return (int64_t)GetTickCount();
#else
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
        return 0;
    }
    return ((int64_t)tv.tv_sec * 1000) + (tv.tv_usec / 1000);
#endif
}

static void sleep_ms(unsigned int ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    struct timeval tv;

    tv.tv_sec = (time_t)(ms / 1000U);
    tv.tv_usec = (suseconds_t)((ms % 1000U) * 1000U);
    (void)select(0, NULL, NULL, NULL, &tv);
#endif
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

static uint16_t crc16_xmodem(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

#ifdef _WIN32
static char *windows_port_name(const char *port)
{
    static char name[64];

    if (strncmp(port, "\\\\.\\", 4) == 0) {
        return (char *)port;
    }

    snprintf(name, sizeof(name), "\\\\.\\%s", port);
    return name;
}
#endif

static int serial_open(struct serial_port *serial, const char *port, int baud)
{
#ifdef _WIN32
    DCB dcb;
    COMMTIMEOUTS timeouts;

    serial->handle = CreateFileA(windows_port_name(port), GENERIC_READ | GENERIC_WRITE,
                                 0, NULL, OPEN_EXISTING, 0, NULL);
    if (serial->handle == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "open serial failed: %s\n", port);
        return -1;
    }

    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(serial->handle, &dcb)) {
        fprintf(stderr, "GetCommState failed\n");
        CloseHandle(serial->handle);
        return -1;
    }

    dcb.BaudRate = (DWORD)baud;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    if (!SetCommState(serial->handle, &dcb)) {
        fprintf(stderr, "SetCommState failed\n");
        CloseHandle(serial->handle);
        return -1;
    }

    memset(&timeouts, 0, sizeof(timeouts));
    timeouts.ReadIntervalTimeout = MAXDWORD;
    timeouts.ReadTotalTimeoutConstant = SERIAL_READ_SLICE_MS;
    timeouts.WriteTotalTimeoutConstant = 5000;
    if (!SetCommTimeouts(serial->handle, &timeouts)) {
        fprintf(stderr, "SetCommTimeouts failed\n");
        CloseHandle(serial->handle);
        return -1;
    }

    PurgeComm(serial->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return 0;
#else
    struct termios tio;
    speed_t speed = B115200;

    serial->fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (serial->fd < 0) {
        fprintf(stderr, "open serial failed: %s: %s\n", port, strerror(errno));
        return -1;
    }

    switch (baud) {
    case 9600:
        speed = B9600;
        break;
    case 57600:
        speed = B57600;
        break;
    case 115200:
        speed = B115200;
        break;
#ifdef B230400
    case 230400:
        speed = B230400;
        break;
#endif
    default:
        fprintf(stderr, "unsupported baud: %d\n", baud);
        close(serial->fd);
        return -1;
    }

    memset(&tio, 0, sizeof(tio));
    if (tcgetattr(serial->fd, &tio) != 0) {
        fprintf(stderr, "tcgetattr failed: %s\n", strerror(errno));
        close(serial->fd);
        return -1;
    }

    cfsetispeed(&tio, speed);
    cfsetospeed(&tio, speed);
    tio.c_cflag &= ~(CSIZE | PARENB | CSTOPB);
#ifdef CRTSCTS
    tio.c_cflag &= ~CRTSCTS;
#endif
    tio.c_cflag |= CS8;
    tio.c_cflag |= CLOCAL | CREAD;
    tio.c_iflag = 0;
    tio.c_oflag = 0;
    tio.c_lflag = 0;
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;

    if (tcsetattr(serial->fd, TCSANOW, &tio) != 0) {
        fprintf(stderr, "tcsetattr failed: %s\n", strerror(errno));
        close(serial->fd);
        return -1;
    }

    tcflush(serial->fd, TCIOFLUSH);
    return 0;
#endif
}

static void serial_close(struct serial_port *serial)
{
#ifdef _WIN32
    CloseHandle(serial->handle);
#else
    close(serial->fd);
#endif
}

static int serial_write_all(struct serial_port *serial, const uint8_t *data,
                            size_t len)
{
    size_t written_total = 0;

    while (written_total < len) {
#ifdef _WIN32
        DWORD written = 0;

        if (!WriteFile(serial->handle, data + written_total,
                       (DWORD)(len - written_total), &written, NULL)) {
            return -1;
        }
#else
        ssize_t written;

        written = write(serial->fd, data + written_total, len - written_total);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
#endif
        if (written == 0) {
            return -1;
        }
        written_total += (size_t)written;
    }

    return 0;
}

static int serial_read_byte(struct serial_port *serial, uint8_t *byte,
                            int timeout_ms)
{
    int64_t deadline = now_ms() + timeout_ms;

    while (now_ms() < deadline) {
#ifdef _WIN32
        DWORD got = 0;

        if (!ReadFile(serial->handle, byte, 1, &got, NULL)) {
            return -1;
        }
        if (got == 1) {
            return 0;
        }
#else
        ssize_t got;

        got = read(serial->fd, byte, 1);
        if (got == 1) {
            return 0;
        }
        if (got < 0 && errno != EINTR && errno != EAGAIN) {
            return -1;
        }
#endif
        sleep_ms(1);
    }

    return -1;
}

static void serial_drain(struct serial_port *serial)
{
    uint8_t byte;

    while (serial_read_byte(serial, &byte, 20) == 0) {
    }
}

static int wait_byte(struct serial_port *serial, uint8_t expected,
                     int timeout_ms)
{
    uint8_t byte;
    int64_t deadline = now_ms() + timeout_ms;

    while (now_ms() < deadline) {
        if (serial_read_byte(serial, &byte, SERIAL_READ_SLICE_MS) != 0) {
            continue;
        }
        if (byte == expected) {
            return 0;
        }
        if (byte == YMODEM_CAN) {
            fprintf(stderr, "remote canceled transfer\n");
            return -1;
        }
    }

    fprintf(stderr, "timeout waiting for 0x%02x\n", expected);
    return -1;
}

static int wait_ready_and_crc(struct serial_port *serial, int timeout_ms)
{
    char window[96];
    size_t used = 0;
    uint8_t byte;
    int saw_ready = 0;
    int64_t deadline = now_ms() + timeout_ms;

    memset(window, 0, sizeof(window));
    while (now_ms() < deadline) {
        if (serial_read_byte(serial, &byte, SERIAL_READ_SLICE_MS) != 0) {
            continue;
        }

        fputc(byte, stdout);
        fflush(stdout);

        if (byte == YMODEM_CRC) {
            if (!saw_ready) {
                printf("\nYMODEM receiver ready\n");
            }
            return 0;
        }

        if (used < sizeof(window) - 1U) {
            window[used++] = (char)byte;
            window[used] = '\0';
        } else {
            memmove(window, window + 1, sizeof(window) - 2U);
            window[sizeof(window) - 2U] = (char)byte;
            window[sizeof(window) - 1U] = '\0';
        }

        if (strstr(window, "READY") != NULL) {
            saw_ready = 1;
        }
        if (strstr(window, "ERROR") != NULL) {
            fprintf(stderr, "\nremote returned ERROR before YMODEM\n");
            return -1;
        }
    }

    fprintf(stderr, "timeout waiting for DFU READY or YMODEM CRC\n");
    return -1;
}

static int wait_final_status(struct serial_port *serial, int timeout_ms)
{
    char window[160];
    size_t used = 0;
    uint8_t byte;
    int64_t deadline = now_ms() + timeout_ms;

    memset(window, 0, sizeof(window));
    while (now_ms() < deadline) {
        if (serial_read_byte(serial, &byte, SERIAL_READ_SLICE_MS) != 0) {
            continue;
        }

        fputc(byte, stdout);
        fflush(stdout);

        if (used < sizeof(window) - 1U) {
            window[used++] = (char)byte;
            window[used] = '\0';
        } else {
            memmove(window, window + 1, sizeof(window) - 2U);
            window[sizeof(window) - 2U] = (char)byte;
            window[sizeof(window) - 1U] = '\0';
        }

        if (strstr(window, "MODULE STAGED") != NULL ||
            strstr(window, "IMAGE ACCEPTED") != NULL ||
            strstr(window, "PATCH STAGED") != NULL) {
            return 0;
        }
        if (strstr(window, "ERROR") != NULL) {
            fprintf(stderr, "\nremote returned ERROR after YMODEM\n");
            return -1;
        }
    }

    printf("\nfinal DFU status not reported; assuming YMODEM completion is success\n");
    return 0;
}

static int send_packet(struct serial_port *serial, uint8_t start,
                       uint8_t block_no, const uint8_t *data, size_t len)
{
    uint8_t header[3];
    uint8_t crc_bytes[2];
    uint16_t crc;

    header[0] = start;
    header[1] = block_no;
    header[2] = (uint8_t)~block_no;
    crc = crc16_xmodem(data, len);
    crc_bytes[0] = (uint8_t)(crc >> 8);
    crc_bytes[1] = (uint8_t)crc;

    if (serial_write_all(serial, header, sizeof(header)) != 0 ||
        serial_write_all(serial, data, len) != 0 ||
        serial_write_all(serial, crc_bytes, sizeof(crc_bytes)) != 0) {
        fprintf(stderr, "serial write failed\n");
        return -1;
    }

    return 0;
}

static int send_ymodem(struct serial_port *serial, const char *path,
                       const uint8_t *data, size_t size)
{
    uint8_t packet[YMODEM_BLOCK_1024];
    uint8_t eot = YMODEM_EOT;
    uint8_t block_no = 1;
    size_t offset = 0;
    int percent_last = -1;

    memset(packet, 0, YMODEM_BLOCK_128);
    snprintf((char *)packet, YMODEM_BLOCK_128, "%s%c%lu",
             base_name(path), '\0', (unsigned long)size);
    if (send_packet(serial, YMODEM_SOH, 0, packet, YMODEM_BLOCK_128) != 0 ||
        wait_byte(serial, YMODEM_ACK, PACKET_TIMEOUT_MS) != 0 ||
        wait_byte(serial, YMODEM_CRC, PACKET_TIMEOUT_MS) != 0) {
        return -1;
    }

    while (offset < size) {
        size_t chunk = size - offset;
        int percent;

        if (chunk > YMODEM_BLOCK_1024) {
            chunk = YMODEM_BLOCK_1024;
        }
        memset(packet, 0x1a, sizeof(packet));
        memcpy(packet, data + offset, chunk);

        if (send_packet(serial, YMODEM_STX, block_no, packet,
                        YMODEM_BLOCK_1024) != 0 ||
            wait_byte(serial, YMODEM_ACK, PACKET_TIMEOUT_MS) != 0) {
            return -1;
        }

        offset += chunk;
        block_no++;
        percent = (int)((offset * 100U) / size);
        if (percent != percent_last) {
            printf("\rYMODEM: %d%%", percent);
            fflush(stdout);
            percent_last = percent;
        }
    }
    printf("\n");

    if (serial_write_all(serial, &eot, 1) != 0 ||
        wait_byte(serial, YMODEM_NAK, PACKET_TIMEOUT_MS) != 0 ||
        serial_write_all(serial, &eot, 1) != 0 ||
        wait_byte(serial, YMODEM_ACK, PACKET_TIMEOUT_MS) != 0 ||
        wait_byte(serial, YMODEM_CRC, PACKET_TIMEOUT_MS) != 0) {
        return -1;
    }

    memset(packet, 0, YMODEM_BLOCK_128);
    if (send_packet(serial, YMODEM_SOH, 0, packet, YMODEM_BLOCK_128) != 0 ||
        wait_byte(serial, YMODEM_ACK, PACKET_TIMEOUT_MS) != 0) {
        return -1;
    }

    return 0;
}

static uint8_t *read_file(const char *path, size_t *size_out)
{
    FILE *fp;
    uint8_t *data;
    long size;

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
    if (size <= 0) {
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

int main(int argc, char **argv)
{
    const char *port;
    const char *image_path;
    const char *command = "@RTBUS:DFU=APP\r\n";
    struct serial_port serial;
    uint8_t *image;
    size_t image_size;
    int baud;
    int rc = 1;

    if (argc < 4 || argc > 5) {
        fprintf(stderr, "usage: %s <serial-port> <baud> <image.bin> [DFU command]\n",
                argv[0]);
        return 2;
    }

    port = argv[1];
    baud = atoi(argv[2]);
    image_path = argv[3];
    if (argc == 5) {
        command = argv[4];
    }

    image = read_file(image_path, &image_size);
    if (image == NULL) {
        return 1;
    }

    printf("RTDuo YMODEM upload\n");
    printf("Port: %s\n", port);
    printf("Image: %s (%lu bytes)\n", image_path, (unsigned long)image_size);

    if (serial_open(&serial, port, baud) != 0) {
        free(image);
        return 1;
    }

    serial_drain(&serial);
    if (serial_write_all(&serial, (const uint8_t *)"\r\n", 2) != 0) {
        fprintf(stderr, "failed to write wake line\n");
        goto out;
    }
    sleep_ms(100);
    serial_drain(&serial);

    printf("Command: %s", command);
    if (serial_write_all(&serial, (const uint8_t *)command, strlen(command)) != 0) {
        fprintf(stderr, "failed to write DFU command\n");
        goto out;
    }

    if (wait_ready_and_crc(&serial, READY_TIMEOUT_MS) != 0) {
        goto out;
    }
    if (send_ymodem(&serial, image_path, image, image_size) != 0) {
        goto out;
    }
    if (wait_final_status(&serial, FINAL_TIMEOUT_MS) != 0) {
        goto out;
    }

    printf("\nYMODEM upload done\n");
    rc = 0;

out:
    serial_close(&serial);
    free(image);
    return rc;
}
