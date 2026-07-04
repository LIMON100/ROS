/**
 * ws_server.h — minimal RFC 6455 WebSocket server with:
 *   - 30 Hz Rev. B telemetry broadcast (single nested JSON)
 *   - JSON-RPC text frames (stream/start, stream/stop, stream/status)
 *
 * Endpoint: ws://{ip}:5001/v1
 * Subprotocol: airys.telem.v1
 *
 * The server runs its own IO thread + telemetry tick thread.
 */
#ifndef AIRYS_WS_SERVER_H
#define AIRYS_WS_SERVER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WsServer WsServer;
struct TelemetryGen;

/** RPC handler — return true if recognized.
 *  Set 'result_json' (or 'error_msg') to return.  */
typedef bool (*WsRpcHandler)(const char* method,
                             const char* params_json,
                             char*       result_buf,
                             size_t      result_cap,
                             char*       error_buf,
                             size_t      error_cap,
                             void*       user);

typedef struct {
    const char*   bind_addr;          /* "0.0.0.0"        */
    int           port;               /* 5001             */
    int           max_clients;        /* 4                */
    const char*   path;               /* "/v1"            */
    const char*   subprotocol;        /* "airys.telem.v1" */
} WsServerCfg;

void           ws_server_default_cfg(WsServerCfg* out);
WsServer*      ws_server_create(const WsServerCfg* cfg, struct TelemetryGen* gen);
bool           ws_server_start (WsServer* ws);
void           ws_server_stop  (WsServer* ws);
void           ws_server_destroy(WsServer* ws);

void           ws_server_set_rpc_handler(WsServer* ws, WsRpcHandler cb, void* user);
int            ws_server_client_count(WsServer* ws);

#ifdef __cplusplus
}
#endif
#endif
