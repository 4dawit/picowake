#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "tusb.h"
#include "logo.h"
#include "secrets.h"

static volatile bool wake_requested = false;

static const char HEAD_OK[] =
    "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-store\r\nConnection: close\r\n\r\n";

static const char HEAD_SEE_OTHER[] =
    "HTTP/1.1 303 See Other\r\nLocation: /?sent\r\n"
    "Content-Length: 0\r\nConnection: close\r\n\r\n";

static const char HEAD_NOT_FOUND[] =
    "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";

static const char PAGE_TOP[] =
    "<!doctype html><html lang=en><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<meta name=theme-color content='#000'>"
    "<title>picowake</title>"
    "<style>"
    "*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}"
    "body{margin:0;height:100vh;height:100dvh;overflow:hidden;display:flex;"
    "align-items:center;justify-content:center;background:#000;color:#fff;"
    "font-family:system-ui,-apple-system,'Segoe UI',Roboto,sans-serif}"
    "header{position:fixed;top:0;left:0;right:0;padding:28px 16px 0;"
    "display:flex;flex-direction:column;align-items:center;gap:10px}"
    "header img{width:44px;height:56px}"
    "h1{margin:0;font-size:2.2rem}"
    "button{-webkit-appearance:none;appearance:none;width:210px;height:210px;"
    "border-radius:50%;border:0;cursor:pointer;color:#000;"
    "display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px;"
    "font:600 1.1rem system-ui,sans-serif;letter-spacing:.06em;"
    "background:linear-gradient(145deg,#fff,#b5b5b5);"
    "box-shadow:0 0 50px rgba(255,255,255,.12),inset 0 3px 8px rgba(255,255,255,.6);"
    "transition:transform .12s}"
    "button:active{transform:scale(.94)}"
    "button svg{width:68px;height:68px}"
    ".msg{position:fixed;left:50%;bottom:56px;transform:translateX(-50%);margin:0;"
    "padding:10px 20px;border-radius:999px;font-size:.95rem;white-space:nowrap;"
    "background:#161616;color:#fff;border:1px solid #333}"
    "footer{position:fixed;left:0;right:0;bottom:0;padding:14px 16px;"
    "text-align:center;font-size:.75rem;color:#707070}"
    "@media(max-height:520px){header img{display:none}button{width:150px;height:150px}"
    "button svg{width:48px;height:48px}}"
    "</style></head><body>"
    "<header><img src=/logo.png alt='' width=44 height=56>"
    "<h1>picowake</h1></header>"
    "<form method=post action=/wake>"
    "<button type=submit aria-label='Wake PC'>"
    "<svg viewBox='0 0 24 24' fill=none stroke=currentColor stroke-width=2 "
    "stroke-linecap=round stroke-linejoin=round><path d='M12 3v9'/>"
    "<path d='M6.3 6.3a8 8 0 1 0 11.4 0'/></svg>"
    "WAKE PC</button></form>";

static const char PAGE_MSG[] = "<p class=msg>Wake signal sent</p>";

static err_t tx(struct tcp_pcb *pcb, const void *data, size_t len) {
    return tcp_write(pcb, data, (u16_t)len, TCP_WRITE_FLAG_COPY);
}

static bool req_is(struct pbuf *p, const char *s) {
    size_t n = strlen(s);
    return p->tot_len >= n && pbuf_memcmp(p, 0, s, n) == 0;
}

static err_t on_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err) {
    (void)arg; (void)err;
    if (!p) { tcp_close(pcb); return ERR_OK; }

    bool wake = req_is(p, "POST /wake");
    bool logo = req_is(p, "GET /logo.png");
    bool page = req_is(p, "GET / ") || req_is(p, "GET /?");
    bool sent = req_is(p, "GET /?sent");
    tcp_recved(pcb, p->tot_len);
    pbuf_free(p);

    if (wake) {
        wake_requested = true;
        tx(pcb, HEAD_SEE_OTHER, sizeof HEAD_SEE_OTHER - 1);
    } else if (logo) {
        char head[160];
        int n = snprintf(head, sizeof head,
            "HTTP/1.1 200 OK\r\nContent-Type: image/png\r\n"
            "Cache-Control: public, max-age=86400\r\n"
            "Content-Length: %u\r\nConnection: close\r\n\r\n", logo_png_len);
        tx(pcb, head, n);
        tx(pcb, logo_png, logo_png_len);
    } else if (page) {
        tx(pcb, HEAD_OK, sizeof HEAD_OK - 1);
        tx(pcb, PAGE_TOP, sizeof PAGE_TOP - 1);
        if (sent) tx(pcb, PAGE_MSG, sizeof PAGE_MSG - 1);
    } else {
        tx(pcb, HEAD_NOT_FOUND, sizeof HEAD_NOT_FOUND - 1);
    }
    tcp_output(pcb);
    tcp_close(pcb);
    return ERR_OK;
}

static err_t on_accept(void *arg, struct tcp_pcb *newpcb, err_t err) {
    (void)arg;
    if (err != ERR_OK || !newpcb) return ERR_VAL;
    tcp_recv(newpcb, on_recv);
    return ERR_OK;
}

static bool start_server(void) {
    cyw43_arch_lwip_begin();
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_ANY);
    bool ok = false;
    if (pcb && tcp_bind(pcb, NULL, 80) == ERR_OK) {
        pcb = tcp_listen_with_backlog(pcb, 4);
        if (pcb) { tcp_accept(pcb, on_accept); ok = true; }
    }
    cyw43_arch_lwip_end();
    return ok;
}

static bool wifi_connect(void) {
    return cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS,
                                              CYW43_AUTH_WPA2_AES_PSK, 15000) == 0;
}

static void do_wake(void) {
    if (tud_suspended()) {
        tud_remote_wakeup();
    } else if (tud_hid_ready()) {
        // PC already awake: tap Shift (wakes display / screensaver)
        tud_hid_keyboard_report(0, KEYBOARD_MODIFIER_LEFTSHIFT, NULL);
        absolute_time_t t = make_timeout_time_ms(60);
        while (!time_reached(t)) tud_task();
        tud_hid_keyboard_report(0, 0, NULL);
    }
}

int main(void) {
    stdio_init_all();
    tusb_init();

    if (cyw43_arch_init()) return 1;
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_pm(&cyw43_state, CYW43_PERFORMANCE_PM);
    cyw43_arch_lwip_begin();
    netif_set_hostname(&cyw43_state.netif[CYW43_ITF_STA], "picowake");
    cyw43_arch_lwip_end();

    while (!wifi_connect()) tud_task();
    start_server();

    printf("IP: %s\n", ip4addr_ntoa(netif_ip4_addr(&cyw43_state.netif[CYW43_ITF_STA])));
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    absolute_time_t next_check = make_timeout_time_ms(10000);
    while (true) {
        tud_task();

        if (wake_requested) {
            wake_requested = false;
            do_wake();
        }

        if (time_reached(next_check)) {
            next_check = make_timeout_time_ms(10000);
            if (cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA) != CYW43_LINK_UP) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                if (wifi_connect()) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            }
        }
    }
}
