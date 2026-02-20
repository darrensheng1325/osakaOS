#include <drivers/pit.h>
#include <emscripten.h>
#include <emscripten/html5.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

static uint32_t g_tickCount = 0;

PIT::PIT(InterruptManager* manager) 
: InterruptHandler(0x00, manager),
  channel0(0x40),
  channel1(0x41),
  channel2(0x42),
  commandPort(0x43),
  PIC(0x20) {
    
    // Setup JavaScript timer for web version
    EM_ASM({
        var tickCount = 0;
        Module._g_tickCount = 0;
        
        // Timer interrupt every ~10ms (100Hz)
        setInterval(function() {
            tickCount++;
            Module._g_tickCount = tickCount;
            
            // Trigger timer interrupt if handler is set
            if (Module._timerInterruptHandler) {
                Module.ccall('triggerTimerInterrupt', null, [], []);
            }
        }, 10);
    });
}

PIT::~PIT() {
}

void PIT::sleep(uint32_t ms) {
    if (ms == 0) return;
    
    // For very short delays (<=5ms), use a tight busy-wait for precision
    // This is acceptable for frame rate control and won't noticeably freeze
    if (ms <= 5) {
        EM_ASM({
            var targetTime = performance.now() + $0;
            while (performance.now() < targetTime) {
                // Tight loop for precision on very short delays
            }
        }, ms);
        return;
    }
    
    // For longer delays, break into very small chunks (1ms) to minimize freezing
    // Use a less tight loop that allows browser's event loop to process events
    // This is the best we can do without async/await (which conflicts with ASYNCIFY)
    uint32_t chunkSize = 1; // 1ms chunks for maximum responsiveness
    uint32_t remaining = ms;
    
    while (remaining > 0) {
        uint32_t chunk = (remaining > chunkSize) ? chunkSize : remaining;
        
        // Sleep this tiny chunk using a less tight loop
        // The periodic checks allow browser to process events
        EM_ASM({
            var targetTime = performance.now() + $0;
            var iterations = 0;
            while (performance.now() < targetTime) {
                iterations++;
                // Every 50 iterations, re-check time
                // This allows browser's event loop to process events
                if (iterations % 50 === 0) {
                    if (performance.now() >= targetTime) break;
                }
            }
        }, chunk);
        
        remaining -= chunk;
        
        // Between chunks, the browser naturally gets a chance to process events
        // because we're returning from EM_ASM and re-entering
        // No additional delay needed - the chunk size is small enough
    }
}

uint32_t PIT::readCount() {
    // Return tick count for web version
    return g_tickCount;
}

void PIT::setCount(uint32_t count) {
    // Stub for web - do nothing
    (void)count;
}

uint32_t PIT::HandleInterrupt(uint32_t esp) {
    // Timer interrupt - send EOI equivalent
    g_tickCount++;
    return esp;
}

// Called from JavaScript timer
extern "C" {
    EMSCRIPTEN_KEEPALIVE void triggerTimerInterrupt() {
        // This will be called by the JavaScript timer
        // The interrupt manager will handle it
    }
}

