// DNS-over-HTTP bridge for the WASM build.
//
// On bare-metal osakaOS, the browser app (Shinosaka) sends UDP/53 DNS
// queries through the NIC. The web port cannot send UDP from the
// browser, so we expose a pair of functions that JavaScript implements
// against a DNS server (typically the one in dns/ which speaks DoH on
// http://DNS_SERVER_IP:DNS_SERVER_PORT/dns-query).

#include <emscripten.h>
#include <common/types.h>

#ifndef DNS_SERVER_IP_STR
#define DNS_SERVER_IP_STR "127.0.0.1"
#endif
#ifndef DNS_SERVER_PORT
#define DNS_SERVER_PORT 8053
#endif

using namespace os::common;

// A 32-bit IPv4 in network byte order, populated by JS via
// dnsResolveCallback once a hostname has been resolved. 0 means
// "no result yet"; the browser app polls this between yields.
static volatile uint32_t g_lastResolvedIP = 0;
static volatile uint16_t g_lastDnsId = 0;

extern "C" {

EMSCRIPTEN_KEEPALIVE
const char* dnsServerHost() { return DNS_SERVER_IP_STR; }

EMSCRIPTEN_KEEPALIVE
int dnsServerPort() { return DNS_SERVER_PORT; }

// Called from C/C++ to kick off an async resolution via JS.
EMSCRIPTEN_KEEPALIVE
void dnsResolveAsync(const char* hostname, uint16_t dnsId) {
    g_lastResolvedIP = 0;
    g_lastDnsId = dnsId;
    EM_ASM_({
        if (Module.dnsResolveOverHttp) {
            var name = UTF8ToString($0);
            Module.dnsResolveOverHttp(name, $1, UTF8ToString($2), $3);
        }
    }, hostname, (int)dnsId, DNS_SERVER_IP_STR, (int)DNS_SERVER_PORT);
}

// Called by JS once the DoH response is in.
// ip is in network byte order (little-endian quad packed big-endian).
EMSCRIPTEN_KEEPALIVE
void dnsResolveCallback(uint16_t dnsId, uint32_t ipBE) {
    if (dnsId == g_lastDnsId) {
        g_lastResolvedIP = ipBE;
    }
}

EMSCRIPTEN_KEEPALIVE
uint32_t dnsLastResolvedIP() { return g_lastResolvedIP; }

EMSCRIPTEN_KEEPALIVE
uint16_t dnsLastResolvedId() { return g_lastDnsId; }

} // extern "C"
