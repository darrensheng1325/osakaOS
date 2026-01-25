#include <drivers/mouse.h>
#include <emscripten.h>
#include <emscripten/html5.h>

using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

// Global mouse handler for web events
static MouseEventHandler* g_mouseHandler = nullptr;

MouseEventHandler::MouseEventHandler() {
}

void MouseEventHandler::OnActivate() {
    // Virtual - implemented by derived classes
}

void MouseEventHandler::OnMouseDown(uint8_t button) {
    // Virtual - implemented by derived classes
}

void MouseEventHandler::OnMouseUp(uint8_t button) {
    // Virtual - implemented by derived classes
}

void MouseEventHandler::OnMouseMove(int x, int y) {
    // Virtual - implemented by derived classes
}

MouseDriver::MouseDriver(InterruptManager* manager, MouseEventHandler* handler)
: InterruptHandler(0x2C, manager),
  dataport(0x60),
  commandport(0x64) {
    this->handler = handler;
    g_mouseHandler = handler;
    offset = 0;
    buttons = 0;
    pressed = false;
    
    // Setup web mouse events
    EM_ASM({
        var canvas = document.getElementById('osaka-canvas');
        if (!canvas) return;
        
        var handler = null;
        
        canvas.addEventListener('mousedown', function(e) {
            if (!Module._g_mouseHandler) return;
            
            var button = 1; // Left button
            if (e.button === 1) button = 2; // Middle button
            if (e.button === 2) button = 3; // Right button
            
            var rect = canvas.getBoundingClientRect();
            var x = e.clientX - rect.left;
            var y = e.clientY - rect.top;
            
            // Scale to 320x200
            x = Math.floor(x * 320 / rect.width);
            y = Math.floor(y * 200 / rect.height);
            
            Module.ccall('handleMouseDown', null, ['number', 'number', 'number'], [button, x, y]);
        });
        
        canvas.addEventListener('mouseup', function(e) {
            if (!Module._g_mouseHandler) return;
            
            var button = 1;
            if (e.button === 1) button = 2;
            if (e.button === 2) button = 3;
            
            Module.ccall('handleMouseUp', null, ['number'], [button]);
        });
        
        canvas.addEventListener('mousemove', function(e) {
            if (!Module._g_mouseHandler) return;
            
            var rect = canvas.getBoundingClientRect();
            var x = e.clientX - rect.left;
            var y = e.clientY - rect.top;
            
            // Scale to 320x200
            x = Math.floor(x * 320 / rect.width);
            y = Math.floor(y * 200 / rect.height);
            
            // Calculate relative movement
            var lastX = Module._lastMouseX || 0;
            var lastY = Module._lastMouseY || 0;
            var dx = x - lastX;
            var dy = y - lastY;
            
            Module._lastMouseX = x;
            Module._lastMouseY = y;
            
            Module.ccall('handleMouseMove', null, ['number', 'number'], [dx, -dy]);
        });
        
        canvas.addEventListener('contextmenu', function(e) {
            e.preventDefault(); // Prevent right-click menu
        });
    });
}

MouseDriver::~MouseDriver() {
    if (g_mouseHandler == handler) {
        g_mouseHandler = nullptr;
    }
}

void MouseDriver::Activate() {
    // Web mouse is always active via DOM events
}

// Web version - called from JavaScript
extern "C" {
    EMSCRIPTEN_KEEPALIVE void handleMouseDown(uint8_t button, int x, int y) {
        if (g_mouseHandler) {
            g_mouseHandler->OnMouseDown(button);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE void handleMouseUp(uint8_t button) {
        if (g_mouseHandler) {
            g_mouseHandler->OnMouseUp(button);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE void handleMouseMove(int dx, int dy) {
        if (g_mouseHandler) {
            g_mouseHandler->OnMouseMove(dx, dy);
        }
    }
}

uint32_t MouseDriver::HandleInterrupt(uint32_t esp) {
    // Web version - interrupts are handled via JavaScript events
    return esp;
}

