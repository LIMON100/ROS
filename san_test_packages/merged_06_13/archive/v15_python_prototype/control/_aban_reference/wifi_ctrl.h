/**
 * wifi_ctrl.h — controlled bring-up / tear-down of board's Wi-Fi AP.
 *
 * On WIFI_ON:
 *   1. Bring up the wireless interface (default wlan0)
 *      and assign its IP via 'ip addr add'.
 *   2. Generate a fresh hostapd.conf in /tmp using the configured SSID + PSK.
 *   3. Start hostapd  (5GHz when available; falls back to 2.4GHz).
 *   4. Start dnsmasq for DHCP (.42 + DNS).
 *   5. Verify both pids are alive — emit WIFI_BRINGUP_25/50/75 progress.
 *   6. On success: WIFI_BRINGUP_100 + WIFI_CRED notification.
 *
 * On WIFI_OFF:
 *   1. SIGTERM hostapd + dnsmasq.
 *   2. ip link set wlan0 down.
 *
 * If the binary is not run as root, every privileged step is replaced with
 * a stub that just sleeps to simulate progress; this is meant for desktop
 * dev iteration. The 'wifi_enabled' config flag toggles real vs stub.
 */
#ifndef AIRYS_WIFI_CTRL_H
#define AIRYS_WIFI_CTRL_H

#include "common.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Progress callback: pct in {25,50,75,100}. Negative on error. */
typedef void (*WifiProgressCb)(int pct, const char* phase, void* user);

typedef struct WifiCtrl WifiCtrl;

WifiCtrl* wifi_ctrl_create(const AirysConfig* cfg);
void      wifi_ctrl_destroy(WifiCtrl* w);

/** Bring AP up. Blocking (5–10s). Returns false on failure. */
bool      wifi_ctrl_up(WifiCtrl* w, WifiProgressCb cb, void* user);

/** Tear down AP. Idempotent. */
bool      wifi_ctrl_down(WifiCtrl* w);

/** Render WIFI_CRED JSON envelope (Rev. B form with video_port + ws_port). */
size_t    wifi_ctrl_render_creds_json(const WifiCtrl* w, int video_port,
                                      int ws_port, const char* transport,
                                      char* out, size_t out_cap);

/** True if currently up. */
bool      wifi_ctrl_is_up(const WifiCtrl* w);

#ifdef __cplusplus
}
#endif
#endif
