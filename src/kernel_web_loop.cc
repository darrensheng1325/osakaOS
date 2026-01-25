// Web-compatible infinite loop handler
// This replaces blocking infinite loops with yielding versions

#include <emscripten.h>
#include <emscripten/html5.h>

// Global state for kernel loops
static bool cli_loop_active = true;
static bool gui_loop_active = true;
static void* cli_loop_data = nullptr;
static void* gui_loop_data = nullptr;

extern "C" {
    // Function to check if we should continue CLI loop
    EMSCRIPTEN_KEEPALIVE bool shouldContinueCLILoop() {
        return cli_loop_active;
    }
    
    // Function to set CLI loop state
    EMSCRIPTEN_KEEPALIVE void setCLILoopActive(bool active) {
        cli_loop_active = active;
    }
    
    // Function to check if we should continue GUI loop  
    EMSCRIPTEN_KEEPALIVE bool shouldContinueGUILoop() {
        return gui_loop_active;
    }
    
    // Function to set GUI loop state
    EMSCRIPTEN_KEEPALIVE void setGUILoopActive(bool active) {
        gui_loop_active = active;
    }
    
    // Yield function for web - allows browser to process events
    EMSCRIPTEN_KEEPALIVE void web_yield() {
        emscripten_sleep(0); // Yield to browser event loop
    }
}

