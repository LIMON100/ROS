/**
 * wifi_ctrl.c — bring up / tear down hostapd + dnsmasq.
 */
#define _GNU_SOURCE
#include "wifi_ctrl.h"
#include "common.h"

#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define TAG "wifi_ctrl"

struct WifiCtrl {
    AirysConfig cfg;
    pid_t       hostapd_pid;
    pid_t       dnsmasq_pid;
    atomic_int  is_up;
};

WifiCtrl* wifi_ctrl_create(const AirysConfig* cfg) {
    WifiCtrl* w = calloc(1, sizeof(*w));
    if (!w) return NULL;
    w->cfg = *cfg;
    w->hostapd_pid = -1;
    w->dnsmasq_pid = -1;
    return w;
}

void wifi_ctrl_destroy(WifiCtrl* w) {
    if (!w) return;
    wifi_ctrl_down(w);
    free(w);
}

/* ── helpers ─────────────────────────────────────────────────── */

static int run_cmd(const char* cmd) {
    LOGD(TAG, "run: %s", cmd);
    int rc = system(cmd);
    if (rc != 0) LOGW(TAG, "exit code %d for: %s", rc, cmd);
    return rc;
}

static void emit(WifiProgressCb cb, void* user, int pct, const char* phase) {
    LOGI(TAG, "phase '%s' (%d%%)", phase, pct);
    if (cb) cb(pct, phase, user);
    /* Phase delay so AIRYS-APP UI can show progress. */
    struct timespec ts = {1, 200 * 1000 * 1000};   /* 1.2s */
    nanosleep(&ts, NULL);
}

static bool write_hostapd_conf(const AirysConfig* cfg, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) { LOGE(TAG, "fopen %s: %s", path, strerror(errno)); return false; }
    fprintf(f,
        "interface=%s\n"
        "driver=nl80211\n"
        "ssid=%s\n"
        "country_code=KR\n"
        "hw_mode=g\n"
        "channel=6\n"
        "ieee80211n=1\n"
        "wpa=2\n"
        "wpa_passphrase=%s\n"
        "wpa_key_mgmt=WPA-PSK\n"
        "wpa_pairwise=CCMP\n"
        "rsn_pairwise=CCMP\n"
        "ctrl_interface=/var/run/hostapd\n",
        cfg->wifi_iface, cfg->wifi_ssid, cfg->wifi_psk);
    fclose(f);
    return true;
}

static bool write_dnsmasq_conf(const AirysConfig* cfg, const char* path) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    /* Derive subnet from configured IP (assumes /24). */
    char base[32]; strncpy(base, cfg->wifi_ip, sizeof(base) - 1);
    base[sizeof(base) - 1] = '\0';
    char* last = strrchr(base, '.');
    if (last) *last = '\0';
    fprintf(f,
        "interface=%s\n"
        "bind-interfaces\n"
        "dhcp-range=%s.50,%s.150,255.255.255.0,12h\n"
        "dhcp-option=3,%s\n"      /* router */
        "dhcp-option=6,%s\n"      /* dns    */
        "log-dhcp\n",
        cfg->wifi_iface, base, base, cfg->wifi_ip, cfg->wifi_ip);
    fclose(f);
    return true;
}

/* fork+exec returning the child pid (or -1) */
static pid_t spawn_daemon(char* const argv[]) {
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        /* New session so killpg works in tear-down. */
        setsid();
        execvp(argv[0], argv);
        _exit(127);
    }
    return pid;
}

static bool stub_mode(const AirysConfig* cfg) {
    /* Use stub (no real wlan0 manipulation) when wifi disabled or non-root. */
    if (!cfg->wifi_enabled) return true;
    if (geteuid() != 0) return true;
    return false;
}

/* ── Public API ────────────────────────────────────────────── */

bool wifi_ctrl_up(WifiCtrl* w, WifiProgressCb cb, void* user) {
    if (!w) return false;
    if (atomic_load(&w->is_up)) {
        LOGW(TAG, "already up — no-op");
        return true;
    }
    LOGI(TAG, "bringing AP up: ssid='%s' iface=%s ip=%s%s",
         w->cfg.wifi_ssid, w->cfg.wifi_iface, w->cfg.wifi_ip,
         stub_mode(&w->cfg) ? "  [STUB MODE]" : "");

    emit(cb, user, 25, "wireless module check");

    if (!stub_mode(&w->cfg)) {
        /* Step 1: bring iface up. (rfkill unblock is best-effort.) */
        run_cmd("rfkill unblock wifi 2>/dev/null");
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "ip link set %s down 2>/dev/null", w->cfg.wifi_iface);
        run_cmd(buf);
        snprintf(buf, sizeof(buf),
                 "ip addr flush dev %s 2>/dev/null", w->cfg.wifi_iface);
        run_cmd(buf);
        snprintf(buf, sizeof(buf),
                 "ip link set %s up", w->cfg.wifi_iface);
        if (run_cmd(buf) != 0) {
            LOGE(TAG, "failed to bring iface up");
            if (cb) { cb(-1, "iface_up_fail", user); } return false;
        }
        snprintf(buf, sizeof(buf),
                 "ip addr add %s/24 dev %s", w->cfg.wifi_ip, w->cfg.wifi_iface);
        run_cmd(buf);
    }

    emit(cb, user, 50, "hostapd start");

    if (!stub_mode(&w->cfg)) {
        const char* hp = "/tmp/airys_hostapd.conf";
        if (!write_hostapd_conf(&w->cfg, hp)) {
            if (cb) { cb(-1, "hostapd_conf_fail", user); } return false;
        }
        char* hostapd_argv[] = { (char*)"hostapd", (char*)hp, NULL };
        w->hostapd_pid = spawn_daemon(hostapd_argv);
        if (w->hostapd_pid < 0) {
            LOGE(TAG, "spawn hostapd failed");
            if (cb) { cb(-1, "hostapd_spawn_fail", user); } return false;
        }
        /* Allow hostapd to settle. */
        sleep(2);
        /* Detect early death. */
        int wstat;
        pid_t r = waitpid(w->hostapd_pid, &wstat, WNOHANG);
        if (r == w->hostapd_pid) {
            LOGE(TAG, "hostapd died early");
            w->hostapd_pid = -1;
            if (cb) { cb(-1, "hostapd_died", user); } return false;
        }
    }

    emit(cb, user, 75, "dnsmasq start");

    if (!stub_mode(&w->cfg)) {
        const char* dp = "/tmp/airys_dnsmasq.conf";
        if (!write_dnsmasq_conf(&w->cfg, dp)) {
            if (cb) { cb(-1, "dnsmasq_conf_fail", user); } return false;
        }
        char* dnsmasq_argv[] = {
            (char*)"dnsmasq", (char*)"--keep-in-foreground",
            (char*)"-C", (char*)dp, NULL
        };
        w->dnsmasq_pid = spawn_daemon(dnsmasq_argv);
        if (w->dnsmasq_pid < 0) {
            LOGE(TAG, "spawn dnsmasq failed");
            if (cb) { cb(-1, "dnsmasq_spawn_fail", user); } return false;
        }
        sleep(1);
        int wstat;
        pid_t r = waitpid(w->dnsmasq_pid, &wstat, WNOHANG);
        if (r == w->dnsmasq_pid) {
            LOGE(TAG, "dnsmasq died early");
            w->dnsmasq_pid = -1;
            if (cb) { cb(-1, "dnsmasq_died", user); } return false;
        }
    }

    emit(cb, user, 100, "ready");
    atomic_store(&w->is_up, 1);
    return true;
}

bool wifi_ctrl_down(WifiCtrl* w) {
    if (!w || !atomic_load(&w->is_up)) return true;
    LOGI(TAG, "tearing AP down");

    if (w->dnsmasq_pid > 0) {
        kill(-w->dnsmasq_pid, SIGTERM);
        int wstat;
        for (int i = 0; i < 10; ++i) {
            if (waitpid(w->dnsmasq_pid, &wstat, WNOHANG) == w->dnsmasq_pid) break;
            usleep(100 * 1000);
        }
        kill(-w->dnsmasq_pid, SIGKILL);
        waitpid(w->dnsmasq_pid, &wstat, 0);
        w->dnsmasq_pid = -1;
    }
    if (w->hostapd_pid > 0) {
        kill(-w->hostapd_pid, SIGTERM);
        int wstat;
        for (int i = 0; i < 10; ++i) {
            if (waitpid(w->hostapd_pid, &wstat, WNOHANG) == w->hostapd_pid) break;
            usleep(100 * 1000);
        }
        kill(-w->hostapd_pid, SIGKILL);
        waitpid(w->hostapd_pid, &wstat, 0);
        w->hostapd_pid = -1;
    }
    if (!stub_mode(&w->cfg)) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "ip link set %s down 2>/dev/null", w->cfg.wifi_iface);
        run_cmd(buf);
    }
    atomic_store(&w->is_up, 0);
    return true;
}

bool wifi_ctrl_is_up(const WifiCtrl* w) {
    return w && atomic_load((atomic_int*)&w->is_up);
}

size_t wifi_ctrl_render_creds_json(const WifiCtrl* w, int video_port,
                                   int ws_port, const char* transport,
                                   char* out, size_t out_cap) {
    if (!w || !out) return 0;
    int n = snprintf(out, out_cap,
        "{\"ssid\":\"%s\","
        "\"psk\":\"%s\","
        "\"ip\":\"%s\","
        "\"video_port\":%d,"
        "\"ws_port\":%d,"
        "\"stream_id\":\"airys-ch2\","
        "\"transport\":\"%s\","
        "\"ttl_s\":3600}",
        w->cfg.wifi_ssid, w->cfg.wifi_psk, w->cfg.wifi_ip,
        video_port, ws_port,
        transport ? transport : "udp");
    return (n > 0 && (size_t)n < out_cap) ? (size_t)n : 0;
}
