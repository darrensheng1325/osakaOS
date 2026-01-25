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
    // Use JavaScript setTimeout for web version
    EM_ASM({
        var start = Date.now();
        var end = start + $0;
        while (Date.now() < end) {
            // Busy wait (not ideal but works for web)
        }
    }, ms);
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

