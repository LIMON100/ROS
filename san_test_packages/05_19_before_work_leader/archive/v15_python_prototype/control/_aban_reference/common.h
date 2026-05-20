/**
 * common.h — shared types, logging, and small helpers
 */
#ifndef AIRYS_COMMON_H
#define AIRYS_COMMON_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Logging ─────────────────────────────────────────────────── */

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3,
} LogLevel;

void  log_set_level(LogLevel level);
void  log_msg(LogLevel level, const char* tag, const char* fmt, ...);

#define LOGD(tag, ...)  log_msg(LOG_DEBUG, tag, __VA_ARGS__)
#define LOGI(tag, ...)  log_msg(LOG_INFO,  tag, __VA_ARGS__)
#define LOGW(tag, ...)  log_msg(LOG_WARN,  tag, __VA_ARGS__)
#define LOGE(tag, ...)  log_msg(LOG_ERROR, tag, __VA_ARGS__)

/* ── Time helpers (CLOCK_MONOTONIC microseconds) ─────────────── */

static inline uint64_t mono_us(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000ULL + (uint64_t)t.tv_nsec / 1000ULL;
}
static inline long mono_ms(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

/* ── Global config ───────────────────────────────────────────── */

typedef enum {
    VIDEO_SRC_TESTPATTERN = 0,
    VIDEO_SRC_V4L2        = 1,
} VideoSourceType;

typedef enum {
    TRANSPORT_UDP = 0,
    TRANSPORT_SRT = 1,
} TransportType;

typedef enum {
    BLE_MODE_SIMULATOR = 0,    /* TCP-based simulator, no real BLE adapter */
    BLE_MODE_REAL      = 1,    /* BlueZ D-Bus GATT server                  */
} BleMode;

typedef struct {
    /* video pipeline */
    VideoSourceType video_src;
    const char*     video_device;        /* /dev/video0 etc. */
    TransportType   transport;
    int             video_port;          /* default 5000 */
    int             video_bitrate_kbps;  /* default 4000 */

    /* services */
    int             ws_port;             /* default 5001 */
    int             http_port;           /* default 8000 */
    const char*     bind_addr;           /* default "0.0.0.0" */
    const char*     recordings_dir;      /* default "./recordings" */

    /* BLE + Wi-Fi */
    BleMode         ble_mode;
    int             ble_tcp_port;        /* simulator mode, default 5555 */
    bool            wifi_enabled;        /* if false, hostapd/dnsmasq skipped */
    const char*     wifi_ssid;           /* default "AIRYS-TEST" */
    const char*     wifi_psk;            /* default randomly generated */
    const char*     wifi_iface;          /* default "wlan0" */
    const char*     wifi_ip;             /* default "192.168.42.1" */

    /* misc */
    bool            generate_samples;    /* on startup, create dummy MP4s */
    const char*     telemetry_csv;       /* optional CSV file path; NULL = synthetic */
    int             verbose;
} AirysConfig;

/* Default-init fills with sensible defaults. */
void airys_config_defaults(AirysConfig* c);

#ifdef __cplusplus
}
#endif
#endif /* AIRYS_COMMON_H */
