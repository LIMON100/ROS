/**
 * ws_server.c — minimal RFC 6455 server with Rev. B broadcast + JSON-RPC.
 *
 * Architecture:
 *   IO thread:        accept(), poll(), read TEXT frames, dispatch RPC.
 *   Tick thread:      30Hz timer → telemetry_gen_render_json() → broadcast.
 *
 * Frame ops we send: TEXT (0x1), CLOSE (0x8), PONG (0xA).
 * Frame ops we recv: TEXT, PING, CLOSE.  No fragmentation, no binary.
 */
#define _GNU_SOURCE
#include "ws_server.h"
#include "telemetry_gen.h"
#include "common.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/bio.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define TAG "ws_server"
#define WS_GUID  "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define MAX_FRAME_SZ  (64 * 1024)
#define HANDSHAKE_RX_MAX  4096

typedef struct {
    int      fd;
    bool     handshaken;
    bool     closing;
    /* RX assembly */
    uint8_t  rx[HANDSHAKE_RX_MAX];
    size_t   rx_len;
    /* TX (single pending — leaky drop on overflow) */
    uint8_t* tx_buf;
    size_t   tx_len;
    size_t   tx_off;
    long     last_pong_ms;
} WsClient;

struct WsServer {
    WsServerCfg     cfg;
    int             listen_fd;
    pthread_t       io_thread;
    pthread_t       tick_thread;
    atomic_int      running;

    pthread_mutex_t mtx;
    WsClient*       clients;
    int             clients_cap;

    WsRpcHandler    rpc_cb;
    void*           rpc_user;

    struct TelemetryGen* gen;
};

void ws_server_default_cfg(WsServerCfg* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->bind_addr   = "0.0.0.0";
    out->port        = 5001;
    out->max_clients = 4;
    out->path        = "/v1";
    out->subprotocol = "airys.telem.v1";
}

/* ── Helpers ────────────────────────────────────────────────── */

static void set_nonblock(int fd) {
    int f = fcntl(fd, F_GETFL, 0);
    if (f >= 0) fcntl(fd, F_SETFL, f | O_NONBLOCK);
}

static int b64_encode(const uint8_t* in, size_t inlen, char* out, size_t out_cap) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* mem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, mem);
    BIO_write(b64, in, inlen);
    (void)BIO_flush(b64);
    BUF_MEM* bp; BIO_get_mem_ptr(b64, &bp);
    int n = (int)bp->length;
    if ((size_t)n + 1 > out_cap) n = out_cap - 1;
    memcpy(out, bp->data, n);
    out[n] = '\0';
    BIO_free_all(b64);
    return n;
}

static char* hdr_lookup(const char* req, const char* name) {
    /* Find header line case-insensitively. Returns malloc'd value. */
    const char* p = req;
    size_t nlen = strlen(name);
    while ((p = strstr(p, "\r\n")) != NULL) {
        p += 2;
        if (!*p) break;
        if (strncasecmp(p, name, nlen) == 0 && p[nlen] == ':') {
            const char* val = p + nlen + 1;
            while (*val == ' ' || *val == '\t') val++;
            const char* end = strstr(val, "\r\n");
            if (!end) end = val + strlen(val);
            size_t len = end - val;
            char* out = malloc(len + 1);
            if (!out) return NULL;
            memcpy(out, val, len); out[len] = '\0';
            return out;
        }
    }
    return NULL;
}

static int do_handshake(WsClient* c, const WsServerCfg* cfg) {
    /* Look for "\r\n\r\n" terminator in c->rx */
    if (c->rx_len < 4) return 0;
    char* end = memmem(c->rx, c->rx_len, "\r\n\r\n", 4);
    if (!end) return 0;
    *end = '\0';
    char* req = (char*)c->rx;

    /* Validate path */
    /* Format: "GET <path> HTTP/1.1\r\n..." */
    char* sp1 = strchr(req, ' ');
    char* sp2 = sp1 ? strchr(sp1 + 1, ' ') : NULL;
    if (!sp1 || !sp2) return -1;
    *sp2 = '\0';
    int path_ok = (strcmp(sp1 + 1, cfg->path) == 0);
    *sp2 = ' ';     /* restore so the header parser can find "\r\n" */
    if (!path_ok) {
        LOGW(TAG, "wrong path (expected %s)", cfg->path);
        return -1;
    }

    char* key = hdr_lookup(req, "Sec-WebSocket-Key");
    char* upg = hdr_lookup(req, "Upgrade");
    if (!key || !upg || strcasecmp(upg, "websocket") != 0) {
        free(key); free(upg);
        return -1;
    }

    char concat[256];
    int  cn = snprintf(concat, sizeof(concat), "%s%s", key, WS_GUID);
    free(key); free(upg);
    if (cn < 0 || (size_t)cn >= sizeof(concat)) return -1;

    uint8_t sha[20];
    SHA1((const unsigned char*)concat, cn, sha);
    char accept[64];
    b64_encode(sha, 20, accept, sizeof(accept));

    char resp[512];
    int rn = snprintf(resp, sizeof(resp),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "Sec-WebSocket-Protocol: %s\r\n"
        "\r\n",
        accept, cfg->subprotocol);
    if (rn < 0 || (size_t)rn >= sizeof(resp)) return -1;

    ssize_t w = send(c->fd, resp, rn, MSG_NOSIGNAL);
    if (w != rn) return -1;
    c->handshaken = true;
    c->rx_len = 0;
    LOGI(TAG, "client %d handshake OK", c->fd);
    return 1;
}

static int build_text_frame(const char* text, size_t tlen,
                            uint8_t* out, size_t out_cap) {
    /* Server-to-client TEXT frame (FIN=1, no mask). */
    if (out_cap < tlen + 14) return -1;
    out[0] = 0x81;            /* FIN + TEXT */
    int hdr = 2;
    if (tlen < 126) {
        out[1] = (uint8_t)tlen;
    } else if (tlen < 65536) {
        out[1] = 126;
        out[2] = (uint8_t)((tlen >> 8) & 0xFF);
        out[3] = (uint8_t)(tlen & 0xFF);
        hdr = 4;
    } else {
        out[1] = 127;
        for (int i = 0; i < 8; ++i)
            out[2 + i] = (uint8_t)((tlen >> ((7 - i) * 8)) & 0xFF);
        hdr = 10;
    }
    memcpy(out + hdr, text, tlen);
    return (int)(hdr + tlen);
}

static int build_pong_frame(const uint8_t* payload, size_t plen,
                            uint8_t* out, size_t out_cap) {
    if (out_cap < plen + 2 || plen > 125) return -1;
    out[0] = 0x8A;        /* FIN + PONG */
    out[1] = (uint8_t)plen;
    if (plen) memcpy(out + 2, payload, plen);
    return (int)(2 + plen);
}

/* Reserved for future use (graceful close handshake). */
__attribute__((unused))
static int build_close_frame(uint16_t code, uint8_t* out, size_t out_cap) {
    if (out_cap < 4) return -1;
    out[0] = 0x88;
    out[1] = 2;
    out[2] = (uint8_t)((code >> 8) & 0xFF);
    out[3] = (uint8_t)(code & 0xFF);
    return 4;
}

/** Dispatch JSON-RPC text frame. Replies on the same client. */
static void dispatch_rpc(WsServer* ws, WsClient* c,
                         const char* payload, size_t plen) {
    if (!ws->rpc_cb) return;

    /* very lightweight: extract "method":"..." + "id":N */
    char method[64] = {0};
    char id[32] = {0};
    char params[256] = {0};
    /* parse method */
    const char* mk = strstr(payload, "\"method\"");
    if (!mk) return;
    const char* mq1 = strchr(mk, ':');
    if (!mq1) return;
    while (*mq1 == ':' || *mq1 == ' ' || *mq1 == '\"') mq1++;
    const char* mq2 = strchr(mq1, '\"');
    if (!mq2) return;
    size_t mlen = (size_t)(mq2 - mq1);
    if (mlen >= sizeof(method)) mlen = sizeof(method) - 1;
    memcpy(method, mq1, mlen);
    method[mlen] = '\0';

    /* parse id (number) */
    const char* ik = strstr(payload, "\"id\"");
    if (ik) {
        const char* iq1 = strchr(ik, ':');
        if (iq1) {
            iq1++;
            while (*iq1 == ' ') iq1++;
            const char* iq2 = iq1;
            while (*iq2 && (*iq2 == '-' || (*iq2 >= '0' && *iq2 <= '9'))) iq2++;
            size_t ilen = (size_t)(iq2 - iq1);
            if (ilen && ilen < sizeof(id)) {
                memcpy(id, iq1, ilen); id[ilen] = '\0';
            }
        }
    }
    if (!id[0]) strcpy(id, "0");

    /* parse params (raw text up to closing brace before id or end) */
    const char* pk = strstr(payload, "\"params\"");
    if (pk) {
        const char* pq = strchr(pk, ':');
        if (pq) {
            pq++; while (*pq == ' ') pq++;
            const char* pe = pq;
            int depth = 0;
            while (*pe) {
                if (*pe == '{') depth++;
                else if (*pe == '}') { if (--depth <= 0) { pe++; break; } }
                else if (*pe == ',' && depth == 0) break;
                pe++;
            }
            size_t plen2 = (size_t)(pe - pq);
            if (plen2 < sizeof(params) - 1) {
                memcpy(params, pq, plen2); params[plen2] = '\0';
            }
        }
    }

    char result[512] = {0};
    char err[128] = {0};
    bool ok = ws->rpc_cb(method, params, result, sizeof(result),
                          err, sizeof(err), ws->rpc_user);

    char reply[1024];
    int rn;
    if (ok) {
        rn = snprintf(reply, sizeof(reply),
            "{\"jsonrpc\":\"2.0\",\"result\":%s,\"id\":%s}",
            result[0] ? result : "{\"ok\":true}", id);
    } else {
        rn = snprintf(reply, sizeof(reply),
            "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32601,"
            "\"message\":\"%s\"},\"id\":%s}",
            err[0] ? err : "method not found", id);
    }
    if (rn <= 0) return;

    uint8_t frame[2048];
    int fn = build_text_frame(reply, (size_t)rn, frame, sizeof(frame));
    if (fn < 0) return;
    send(c->fd, frame, fn, MSG_NOSIGNAL);
    LOGI(TAG, "RPC reply: %s", reply);
}

/** Read available bytes; assemble + parse one frame at a time. */
static void read_pending(WsServer* ws, WsClient* c) {
    uint8_t buf[8192];
    while (true) {
        ssize_t r = recv(c->fd, buf, sizeof(buf), 0);
        if (r > 0) {
            if (!c->handshaken) {
                if (c->rx_len + r > sizeof(c->rx)) { c->closing = true; return; }
                memcpy(c->rx + c->rx_len, buf, r);
                c->rx_len += r;
                int s = do_handshake(c, &ws->cfg);
                if (s < 0) { c->closing = true; return; }
                continue;
            }
            /* Parse one or more frames. */
            size_t off = 0;
            while (off + 2 <= (size_t)r) {
                uint8_t b0 = buf[off], b1 = buf[off + 1];
                uint8_t opcode = b0 & 0x0F;
                bool masked = (b1 & 0x80) != 0;
                size_t plen = b1 & 0x7F;
                size_t hdr = 2;
                if (plen == 126) {
                    if (off + 4 > (size_t)r) break;
                    plen = ((size_t)buf[off + 2] << 8) | buf[off + 3];
                    hdr = 4;
                } else if (plen == 127) {
                    if (off + 10 > (size_t)r) break;
                    plen = 0;
                    for (int i = 0; i < 8; ++i)
                        plen = (plen << 8) | buf[off + 2 + i];
                    hdr = 10;
                }
                if (masked) hdr += 4;
                if (off + hdr + plen > (size_t)r) break;
                uint8_t* payload = buf + off + hdr;
                if (masked) {
                    uint8_t* mask = buf + off + hdr - 4;
                    for (size_t i = 0; i < plen; ++i) payload[i] ^= mask[i & 3];
                }

                switch (opcode) {
                    case 0x1: /* TEXT */
                        if (plen > 0) {
                            char tmp[2048];
                            size_t cl = plen < sizeof(tmp) - 1 ? plen : sizeof(tmp) - 1;
                            memcpy(tmp, payload, cl); tmp[cl] = '\0';
                            LOGD(TAG, "RPC recv: %s", tmp);
                            dispatch_rpc(ws, c, tmp, cl);
                        }
                        break;
                    case 0x9: /* PING */
                    {
                        uint8_t pong[128];
                        int pn = build_pong_frame(payload, plen, pong, sizeof(pong));
                        if (pn > 0) send(c->fd, pong, pn, MSG_NOSIGNAL);
                        break;
                    }
                    case 0x8: /* CLOSE */
                        c->closing = true;
                        break;
                    default: break;
                }
                off += hdr + plen;
            }
            return;
        }
        if (r == 0) { c->closing = true; return; }
        if (errno == EAGAIN || errno == EWOULDBLOCK) return;
        c->closing = true;
        return;
    }
}

static void close_client(WsClient* c) {
    if (c->fd >= 0) close(c->fd);
    free(c->tx_buf);
    memset(c, 0, sizeof(*c));
    c->fd = -1;
}

/* ── IO thread ──────────────────────────────────────────────── */

static void* io_thread_fn(void* arg) {
    WsServer* ws = (WsServer*)arg;
    LOGI(TAG, "IO thread started");
    while (atomic_load(&ws->running)) {
        struct pollfd pfds[16];
        int nfds = 0;
        pfds[nfds].fd = ws->listen_fd;
        pfds[nfds].events = POLLIN;
        nfds++;
        pthread_mutex_lock(&ws->mtx);
        for (int i = 0; i < ws->clients_cap; ++i) {
            if (ws->clients[i].fd >= 0) {
                pfds[nfds].fd = ws->clients[i].fd;
                pfds[nfds].events = POLLIN;
                if (ws->clients[i].tx_buf) pfds[nfds].events |= POLLOUT;
                nfds++;
            }
        }
        pthread_mutex_unlock(&ws->mtx);
        int rc = poll(pfds, nfds, 100);
        if (rc < 0) {
            if (errno == EINTR) continue;
            LOGE(TAG, "poll: %s", strerror(errno)); break;
        }
        if (rc == 0) continue;

        /* New connection? */
        if (pfds[0].revents & POLLIN) {
            int afd = accept(ws->listen_fd, NULL, NULL);
            if (afd >= 0) {
                set_nonblock(afd);
                int yes = 1;
                setsockopt(afd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
                pthread_mutex_lock(&ws->mtx);
                int slot = -1;
                for (int i = 0; i < ws->clients_cap; ++i) {
                    if (ws->clients[i].fd < 0) { slot = i; break; }
                }
                if (slot < 0) {
                    LOGW(TAG, "max clients reached, rejecting fd=%d", afd);
                    close(afd);
                } else {
                    memset(&ws->clients[slot], 0, sizeof(WsClient));
                    ws->clients[slot].fd = afd;
                    LOGI(TAG, "accept fd=%d slot=%d", afd, slot);
                }
                pthread_mutex_unlock(&ws->mtx);
            }
        }

        /* Per-client */
        pthread_mutex_lock(&ws->mtx);
        for (int i = 0; i < ws->clients_cap; ++i) {
            WsClient* c = &ws->clients[i];
            if (c->fd < 0) continue;
            for (int j = 1; j < nfds; ++j) {
                if (pfds[j].fd != c->fd) continue;
                if (pfds[j].revents & POLLIN)  read_pending(ws, c);
                if (pfds[j].revents & POLLOUT && c->tx_buf) {
                    ssize_t w = send(c->fd, c->tx_buf + c->tx_off,
                                     c->tx_len - c->tx_off, MSG_NOSIGNAL);
                    if (w < 0) {
                        if (errno != EAGAIN && errno != EWOULDBLOCK)
                            c->closing = true;
                    } else {
                        c->tx_off += w;
                        if (c->tx_off >= c->tx_len) {
                            free(c->tx_buf); c->tx_buf = NULL;
                            c->tx_len = c->tx_off = 0;
                        }
                    }
                }
            }
            if (c->closing) {
                LOGI(TAG, "closing fd=%d", c->fd);
                close_client(c);
            }
        }
        pthread_mutex_unlock(&ws->mtx);
    }
    LOGI(TAG, "IO thread exit");
    return NULL;
}

/* ── Tick thread (30Hz broadcast) ───────────────────────────── */

static void enqueue_text(WsServer* ws, WsClient* c, const char* text, size_t tlen) {
    if (c->tx_buf) {
        /* Leaky drop: replace pending with newer telemetry frame. */
        free(c->tx_buf); c->tx_buf = NULL;
        c->tx_len = c->tx_off = 0;
    }
    uint8_t* frame = malloc(tlen + 14);
    if (!frame) return;
    int fn = build_text_frame(text, tlen, frame, tlen + 14);
    if (fn < 0) { free(frame); return; }
    c->tx_buf = frame;
    c->tx_len = (size_t)fn;
    c->tx_off = 0;
}

static void* tick_thread_fn(void* arg) {
    WsServer* ws = (WsServer*)arg;
    LOGI(TAG, "tick thread started (30 Hz)");
    char json[1280];
    while (atomic_load(&ws->running)) {
        struct timespec ts = {0, 33 * 1000 * 1000};   /* 33 ms */
        nanosleep(&ts, NULL);
        if (!ws->gen) continue;
        size_t n = telem_gen_render_json(ws->gen, json, sizeof(json));
        if (n == 0) continue;
        pthread_mutex_lock(&ws->mtx);
        for (int i = 0; i < ws->clients_cap; ++i) {
            WsClient* c = &ws->clients[i];
            if (c->fd < 0 || !c->handshaken) continue;
            enqueue_text(ws, c, json, n);
        }
        pthread_mutex_unlock(&ws->mtx);
    }
    LOGI(TAG, "tick thread exit");
    return NULL;
}

/* ── Public API ─────────────────────────────────────────────── */

WsServer* ws_server_create(const WsServerCfg* cfg, struct TelemetryGen* gen) {
    WsServer* ws = calloc(1, sizeof(*ws));
    if (!ws) return NULL;
    ws->cfg = *cfg;
    ws->clients_cap = cfg->max_clients;
    ws->clients = calloc(ws->clients_cap, sizeof(WsClient));
    for (int i = 0; i < ws->clients_cap; ++i) ws->clients[i].fd = -1;
    pthread_mutex_init(&ws->mtx, NULL);
    ws->listen_fd = -1;
    ws->gen = gen;
    return ws;
}

bool ws_server_start(WsServer* ws) {
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) { LOGE(TAG, "socket: %s", strerror(errno)); return false; }
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)ws->cfg.port);
    sa.sin_addr.s_addr = inet_addr(ws->cfg.bind_addr);
    if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        LOGE(TAG, "bind: %s", strerror(errno));
        close(fd); return false;
    }
    if (listen(fd, 8) < 0) {
        LOGE(TAG, "listen: %s", strerror(errno));
        close(fd); return false;
    }
    ws->listen_fd = fd;
    atomic_store(&ws->running, 1);
    pthread_create(&ws->io_thread, NULL, io_thread_fn, ws);
    pthread_create(&ws->tick_thread, NULL, tick_thread_fn, ws);
    LOGI(TAG, "WebSocket listening on %s:%d%s",
         ws->cfg.bind_addr, ws->cfg.port, ws->cfg.path);
    return true;
}

void ws_server_stop(WsServer* ws) {
    if (!ws || !atomic_load(&ws->running)) return;
    atomic_store(&ws->running, 0);
    if (ws->listen_fd >= 0) { close(ws->listen_fd); ws->listen_fd = -1; }
    pthread_join(ws->io_thread, NULL);
    pthread_join(ws->tick_thread, NULL);
    pthread_mutex_lock(&ws->mtx);
    for (int i = 0; i < ws->clients_cap; ++i) {
        if (ws->clients[i].fd >= 0) close_client(&ws->clients[i]);
    }
    pthread_mutex_unlock(&ws->mtx);
}

void ws_server_destroy(WsServer* ws) {
    if (!ws) return;
    ws_server_stop(ws);
    pthread_mutex_destroy(&ws->mtx);
    free(ws->clients);
    free(ws);
}

void ws_server_set_rpc_handler(WsServer* ws, WsRpcHandler cb, void* user) {
    if (!ws) return;
    ws->rpc_cb = cb;
    ws->rpc_user = user;
}

int ws_server_client_count(WsServer* ws) {
    if (!ws) return 0;
    int n = 0;
    pthread_mutex_lock(&ws->mtx);
    for (int i = 0; i < ws->clients_cap; ++i)
        if (ws->clients[i].fd >= 0 && ws->clients[i].handshaken) n++;
    pthread_mutex_unlock(&ws->mtx);
    return n;
}
