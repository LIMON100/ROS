/**
 * ble_sim.h — TCP-based BLE GATT server simulator.
 *
 * Real BLE on Linux requires BlueZ + D-Bus + system bus + agent registration —
 * complex enough that for development/CI we expose the GATT protocol over a
 * plain TCP socket. AIRYS-APP can be modified (or a small adapter on phone)
 * to use TCP instead of BLE for development. The wire protocol below
 * mirrors the BLE GATT semantics 1:1.
 *
 * --------------------------------------------------------------------------
 * Wire protocol (line-oriented JSON over TCP)
 * --------------------------------------------------------------------------
 * Client (AIRYS-APP simulator) → Server:
 *     {"op":"connect"}                          → "Phase 1" pairing
 *     {"op":"write","char":"CMD","val":<byte>}   → BLE C3 CMD opcode
 *     {"op":"write","char":"SETTINGS","hex":"..."} → BLE C7 SETTINGS push
 *     {"op":"disconnect"}
 *
 * Server → Client (notify):
 *     {"notify":"STATE","val":<S code>}          → BLE C5 state machine
 *     {"notify":"WIFI_CRED","payload":{...}}      → BLE C4 credentials
 *     {"notify":"ERROR","code":<byte>}            → BLE C6 error
 *
 * Opcodes — SAN-BLE-WIFI-001 §4.3.2 (matches AirysGatt.Cmd in
 * AIRYS-APP/Models.kt):
 *     0x10 WIFI_ON          0x11 WIFI_OFF
 *     0x20 RESET            0x21 REBOOT
 *     0x30 KEEP_ALIVE       0x31 PING
 *     0x40 REC_TOGGLE       0x41 LRF_TRIGGER       0x42 SNAPSHOT
 *
 * State codes — SAN-BLE-WIFI-001 §4.3.1 (matches AirysGatt.StateCode):
 *     0x00 BOOT             0x01 BLE_ADV
 *     0x02 BLE_CONN         0x03 WIFI_BRINGUP
 *     0x04 WIFI_READY       0x05 STREAMING
 *     0x06 TEARDOWN         0x07 ERROR
 *
 * Error codes — SAN-BLE-WIFI-001 §4.3.4 (matches ErrorCode in Models.kt):
 *     0x10 WifiModuleFail   0x11 HostapdFail   0x12 DnsmasqFail
 *     0x13 BadInterface
 *     0x20 BleLinkLost      0x21 AuthFail
 *     0x30 StreamTimeout    0x31 StreamFail
 *     0x32 RtspRefused      0x33 SrtHandshake
 *     0x40 BatteryLow       0x41 ThermalLimit
 *     0xFE InternalError    0xFF Unknown
 */
#ifndef AIRYS_BLE_SIM_H
#define AIRYS_BLE_SIM_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BleSim BleSim;

/* CMD opcode handler — invoked when AIRYS-APP writes BLE C3 CMD.
 * Implementations should be quick (run in IO thread). */
typedef void (*BleCmdHandler)(uint8_t opcode, void* user);

/* SETTINGS write handler — invoked on BLE C7 16-byte payload. */
typedef void (*BleSettingsHandler)(const uint8_t* bytes, size_t len, void* user);

typedef struct {
    const char* bind_addr;       /* "0.0.0.0"          */
    int         tcp_port;        /* 5555               */
} BleSimCfg;

void    ble_sim_default_cfg(BleSimCfg* out);
BleSim* ble_sim_create(const BleSimCfg* cfg);
bool    ble_sim_start (BleSim* b);
void    ble_sim_stop  (BleSim* b);
void    ble_sim_destroy(BleSim* b);

void    ble_sim_set_cmd_handler     (BleSim* b, BleCmdHandler cb, void* user);
void    ble_sim_set_settings_handler(BleSim* b, BleSettingsHandler cb, void* user);

/* ── Notifications (server-initiated) ───────────────────────── */

void    ble_sim_notify_state(BleSim* b, uint8_t state_code);
void    ble_sim_notify_creds(BleSim* b, const char* json_payload);
void    ble_sim_notify_error(BleSim* b, uint8_t err_code);

#ifdef __cplusplus
}
#endif
#endif
