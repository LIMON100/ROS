/**
 * ble_sim.c — TCP-based BLE GATT simulator.
 */
#define _GNU_SOURCE
#include "ble_sim.h"
#include "common.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TAG "ble_sim"
#define MAX_CLIENTS 4
#define LINE_BUF    2048

typedef struct {
    int      fd;
    char     rx[LINE_BUF];
    size_t   rx_len;
    bool     paired;
} Client;

struct BleSim {
    BleSimCfg          cfg;
    int                listen_fd;
    pthread_t          thread;
    atomic_int         running;

    pthread_mutex_t    mtx;
    Client             clients[MAX_CLIENTS];

    BleCmdHandler      cmd_cb;
    void*              cmd_user;
    BleSettingsHandler sett_cb;
    void*              sett_user;
};

void ble_sim_default_cfg(BleSimCfg* o) {
    if (!o) return;
    memset(o, 0, sizeof(*o));
    o->bind_addr = "0.0.0.0";
    o->tcp_port  = 5555;
}

/* ── helpers ─────────────────────────────────────────────────── */

static void set_nonblock(int fd) {
    int f = fcntl(fd, F_GETFL, 0);
    if (f >= 0) fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

static int hex_decode(const char* hex, uint8_t* out, size_t cap) {
    size_t hlen = strlen(hex);
    if (hlen % 2) return -1;
    size_t blen = hlen / 2;
    if (blen > cap) return -1;
    for (size_t i = 0; i < blen; ++i) {
        char c1 = hex[i*2], c2 = hex[i*2+1];
        int v1 = (c1 >= '0' && c1 <= '9') ? c1 - '0'
               : (c1 >= 'a' && c1 <= 'f') ? c1 - 'a' + 10
               : (c1 >= 'A' && c1 <= 'F') ? c1 - 'A' + 10 : -1;
        int v2 = (c2 >= '0' && c2 <= '9') ? c2 - '0'
               : (c2 >= 'a' && c2 <= 'f') ? c2 - 'a' + 10
               : (c2 >= 'A' && c2 <= 'F') ? c2 - 'A' + 10 : -1;
        if (v1 < 0 || v2 < 0) return -1;
        out[i] = (uint8_t)((v1 << 4) | v2);
    }
    return (int)blen;
}

/** Find a JSON string field. Returns malloc'd value or NULL. */
static char* json_find_str(const char* src, const char* key) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(src, pat);
    if (!p) return NULL;
    p = strchr(p + strlen(pat), ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return NULL;
    p++;
    const char* end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    char* out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, p, len); out[len] = '\0';
    return out;
}

static int json_find_int(const char* src, const char* key, int dflt) {
    char pat[64];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char* p = strstr(src, pat);
    if (!p) return dflt;
    p = strchr(p + strlen(pat), ':');
    if (!p) return dflt;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return atoi(p);
}

static void send_line(int fd, const char* msg) {
    size_t len = strlen(msg);
    char with_lf[2048];
    if (len >= sizeof(with_lf) - 1) return;
    memcpy(with_lf, msg, len);
    with_lf[len] = '\n';
    send(fd, with_lf, len + 1, MSG_NOSIGNAL);
}

static void broadcast_line(BleSim* b, const char* msg) {
    pthread_mutex_lock(&b->mtx);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (b->clients[i].fd >= 0 && b->clients[i].paired) {
            send_line(b->clients[i].fd, msg);
        }
    }
    pthread_mutex_unlock(&b->mtx);
}

/* ── Process one received line ──────────────────────────────── */

static void process_line(BleSim* b, Client* c, const char* line) {
    char* op = json_find_str(line, "op");
    if (!op) return;
    if (strcmp(op, "connect") == 0) {
        c->paired = true;
        /* SAN-BLE-WIFI-001 §4.2 — 7-characteristic GATT service.
         * Order matches AirysGatt C1..C7 in AIRYS-APP/Models.kt:
         *   C1 DEVICE_INFO   read  device id, fw version, serial
         *   C2 STATE         notify+read scope state machine byte
         *   C3 CMD           write opcodes (WIFI_ON, REC_TOGGLE, ...)
         *   C4 WIFI_CRED     notify+read SSID/PSK/IPs (JSON envelope)
         *   C5 TELEMETRY     notify (optional — Rev. C.2 deprecated;
         *                            kept for legacy firmware compat)
         *   C6 ERROR         notify single-byte error code
         *   C7 SETTINGS      write 16-byte settings block + CRC */
        send_line(c->fd, "{\"event\":\"paired\",\"service\":\"AIRYS\","
                          "\"chars\":[\"DEVICE_INFO\",\"STATE\",\"CMD\","
                          "\"WIFI_CRED\",\"TELEMETRY\",\"ERROR\",\"SETTINGS\"]}");
        LOGI(TAG, "client fd=%d paired", c->fd);
    } else if (strcmp(op, "disconnect") == 0) {
        c->paired = false;
        send_line(c->fd, "{\"event\":\"disconnected\"}");
    } else if (strcmp(op, "write") == 0) {
        char* ch = json_find_str(line, "char");
        if (!ch) { free(op); return; }
        if (strcmp(ch, "CMD") == 0) {
            int v = json_find_int(line, "val", -1);
            if (v >= 0 && v <= 0xFF && b->cmd_cb) {
                LOGI(TAG, "BLE C3 CMD opcode=0x%02X", v);
                b->cmd_cb((uint8_t)v, b->cmd_user);
            }
        } else if (strcmp(ch, "SETTINGS") == 0) {
            char* hex = json_find_str(line, "hex");
            if (hex && b->sett_cb) {
                uint8_t buf[64];
                int blen = hex_decode(hex, buf, sizeof(buf));
                if (blen > 0) {
                    LOGI(TAG, "BLE C7 SETTINGS write %d bytes", blen);
                    b->sett_cb(buf, (size_t)blen, b->sett_user);
                }
            }
            free(hex);
        }
        free(ch);
    }
    free(op);
}

/* ── IO thread ─────────────────────────────────────────────── */

static void* io_loop(void* arg) {
    BleSim* b = (BleSim*)arg;
    LOGI(TAG, "BLE simulator listening on TCP %s:%d",
         b->cfg.bind_addr, b->cfg.tcp_port);
    while (atomic_load(&b->running)) {
        struct pollfd pfds[1 + MAX_CLIENTS];
        int nfds = 0;
        pfds[nfds].fd     = b->listen_fd;
        pfds[nfds].events = POLLIN; nfds++;
        pthread_mutex_lock(&b->mtx);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (b->clients[i].fd >= 0) {
                pfds[nfds].fd = b->clients[i].fd;
                pfds[nfds].events = POLLIN;
                nfds++;
            }
        }
        pthread_mutex_unlock(&b->mtx);

        int rc = poll(pfds, nfds, 100);
        if (rc < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (rc == 0) continue;

        if (pfds[0].revents & POLLIN) {
            int afd = accept(b->listen_fd, NULL, NULL);
            if (afd >= 0) {
                set_nonblock(afd);
                pthread_mutex_lock(&b->mtx);
                int slot = -1;
                for (int i = 0; i < MAX_CLIENTS; ++i)
                    if (b->clients[i].fd < 0) { slot = i; break; }
                if (slot < 0) close(afd);
                else {
                    memset(&b->clients[slot], 0, sizeof(Client));
                    b->clients[slot].fd = afd;
                    LOGI(TAG, "BLE sim accept fd=%d slot=%d", afd, slot);
                }
                pthread_mutex_unlock(&b->mtx);
            }
        }

        pthread_mutex_lock(&b->mtx);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            Client* c = &b->clients[i];
            if (c->fd < 0) continue;
            for (int j = 1; j < nfds; ++j) {
                if (pfds[j].fd != c->fd) continue;
                if (pfds[j].revents & (POLLERR | POLLHUP)) {
                    close(c->fd); c->fd = -1; c->rx_len = 0; c->paired = false;
                    break;
                }
                if (pfds[j].revents & POLLIN) {
                    ssize_t r = recv(c->fd, c->rx + c->rx_len,
                                     sizeof(c->rx) - 1 - c->rx_len, 0);
                    if (r <= 0) {
                        if (r == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
                            close(c->fd); c->fd = -1; c->rx_len = 0; c->paired = false;
                        }
                        break;
                    }
                    c->rx_len += r;
                    c->rx[c->rx_len] = '\0';
                    /* Process line-by-line. */
                    char* nl;
                    while ((nl = memchr(c->rx, '\n', c->rx_len)) != NULL) {
                        *nl = '\0';
                        if (nl > c->rx && *(nl-1) == '\r') *(nl-1) = '\0';
                        process_line(b, c, c->rx);
                        size_t consumed = (size_t)(nl - c->rx + 1);
                        memmove(c->rx, c->rx + consumed, c->rx_len - consumed);
                        c->rx_len -= consumed;
                    }
                }
            }
        }
        pthread_mutex_unlock(&b->mtx);
    }
    return NULL;
}

/* ── Public API ────────────────────────────────────────────── */

BleSim* ble_sim_create(const BleSimCfg* cfg) {
    BleSim* b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->cfg = *cfg;
    pthread_mutex_init(&b->mtx, NULL);
    for (int i = 0; i < MAX_CLIENTS; ++i) b->clients[i].fd = -1;
    b->listen_fd = -1;
    return b;
}

bool ble_sim_start(BleSim* b) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return false;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)b->cfg.tcp_port);
    sa.sin_addr.s_addr = inet_addr(b->cfg.bind_addr);
    if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0 ||
        listen(fd, 4) < 0) {
        LOGE(TAG, "bind/listen: %s", strerror(errno));
        close(fd); return false;
    }
    b->listen_fd = fd;
    atomic_store(&b->running, 1);
    pthread_create(&b->thread, NULL, io_loop, b);
    return true;
}

void ble_sim_stop(BleSim* b) {
    if (!b || !atomic_load(&b->running)) return;
    atomic_store(&b->running, 0);
    if (b->listen_fd >= 0) { close(b->listen_fd); b->listen_fd = -1; }
    pthread_join(b->thread, NULL);
    pthread_mutex_lock(&b->mtx);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        if (b->clients[i].fd >= 0) close(b->clients[i].fd);
    }
    pthread_mutex_unlock(&b->mtx);
}

void ble_sim_destroy(BleSim* b) {
    if (!b) return;
    ble_sim_stop(b);
    pthread_mutex_destroy(&b->mtx);
    free(b);
}

void ble_sim_set_cmd_handler(BleSim* b, BleCmdHandler cb, void* user) {
    if (!b) return;
    b->cmd_cb = cb; b->cmd_user = user;
}

void ble_sim_set_settings_handler(BleSim* b, BleSettingsHandler cb, void* user) {
    if (!b) return;
    b->sett_cb = cb; b->sett_user = user;
}

void ble_sim_notify_state(BleSim* b, uint8_t code) {
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"notify\":\"STATE\",\"val\":%u}", code);
    broadcast_line(b, msg);
}

void ble_sim_notify_creds(BleSim* b, const char* json_payload) {
    char msg[1024];
    snprintf(msg, sizeof(msg), "{\"notify\":\"WIFI_CRED\",\"payload\":%s}",
             json_payload);
    broadcast_line(b, msg);
}

void ble_sim_notify_error(BleSim* b, uint8_t code) {
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"notify\":\"ERROR\",\"code\":%u}", code);
    broadcast_line(b, msg);
}
