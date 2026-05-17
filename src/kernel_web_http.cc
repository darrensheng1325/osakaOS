// HTTP fetch bridge for the WASM build.
//
// Browsers can't emit raw TCP, so the kernel's full TCP stack from
// upstream — which builds SYN/ACK ethernet frames in
// `amd_am79c973::Send` — has nowhere to send them under emscripten.
// Instead, the Shinosaka browser app calls `httpFetchAsync` directly;
// JavaScript runs a `fetch()` against the URL and pushes the raw HTTP
// response bytes (status line, headers, blank line, body — same shape
// the kernel would have received over the wire) into the active TCP
// socket's handleBuffer via `httpFetchCallback`. The browser app then
// processes it through its existing `HandleResponseHTTP` /
// `AllocateMemoryHTML` / `RenderHTML` path — same as bare metal.

#include <emscripten.h>
#include <common/types.h>
#include <net/tcp.h>
#include <net/network.h>

using namespace os::common;
using namespace os::net;

// The TCP socket that should receive the next fetch response. Set by
// `httpFetchAsync` immediately before kicking off the JS fetch. Reset
// to null once the callback delivers data, so a stale callback (e.g.
// after socket destruction) can't write into freed memory.
static TransmissionControlProtocolSocket* g_active_http_socket = nullptr;
static uint32_t g_pending_request_id = 0;

extern "C" {

// Called from the C++ browser app (Shinosaka::SendRequestHTTP) to
// initiate an HTTP GET via JS. `socket` is the TCP socket whose
// handleBuffer we should populate with the response. `url` should be
// the full URL (scheme included) the kernel wants to fetch.
EMSCRIPTEN_KEEPALIVE
void httpFetchAsync(TransmissionControlProtocolSocket* socket, const char* url) {
    g_active_http_socket = socket;
    g_pending_request_id++;
    if (socket) {
        socket->handleType = HANDLE_FLAG_EMPTY;
        socket->bufferIndex = 0;
    }
    EM_ASM_({
        if (Module.httpFetchOverWeb) {
            Module.httpFetchOverWeb(UTF8ToString($0), $1);
        }
    }, url, (int)g_pending_request_id);
}

// Called by JS once the fetch resolves. `data` points to a wasm-heap
// buffer (allocated via _malloc on the JS side) containing the full
// HTTP response. We copy into the active socket's handleBuffer so the
// browser app's ComputeAppState picks it up exactly as it would on
// bare metal.
EMSCRIPTEN_KEEPALIVE
void httpFetchCallback(uint32_t requestId, uint8_t* data, uint32_t length) {
    if (requestId != g_pending_request_id) return;
    TransmissionControlProtocolSocket* s = g_active_http_socket;
    if (!s || !data || length == 0) return;
    if (length > HANDLE_BUF_SIZE_TCP) length = HANDLE_BUF_SIZE_TCP;
    for (uint32_t i = 0; i < length; i++) {
        s->handleBuffer[i] = data[i];
    }
    s->bufferIndex = length;
    s->handleType = HANDLE_FLAG_TCP;
    g_active_http_socket = nullptr;
}

// Surface "connection failed" back into the socket so the browser app
// renders its standard error path.
EMSCRIPTEN_KEEPALIVE
void httpFetchFail(uint32_t requestId) {
    if (requestId != g_pending_request_id) return;
    TransmissionControlProtocolSocket* s = g_active_http_socket;
    if (s) s->connectionFail = true;
    g_active_http_socket = nullptr;
}

} // extern "C"
