#include <drivers/mouse.h>
#include <emscripten.h>
#include <emscripten/html5.h>

extern "C" {
    void printf(char* strChr);
}

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
    
    // Export handler pointer for JavaScript
    EM_ASM_({
        Module._g_mouseHandler = $0;
    }, (uintptr_t)handler);
    
    // Setup web mouse events for both canvases
    EM_ASM({
        // Function to setup mouse events on a canvas
        function setupMouseEvents(canvas, isTextCanvas) {
            if (!canvas) return;
            
            canvas.addEventListener('mousedown', function(e) {
                console.log('[JS] Mouse down event on canvas, handler:', Module._g_mouseHandler);
                if (!Module._g_mouseHandler) {
                    console.error('[JS] Mouse handler not available!');
                    return;
                }
                
                var button = 1; // Left button
                if (e.button === 1) button = 2; // Middle button
                if (e.button === 2) button = 3; // Right button
                
                var rect = canvas.getBoundingClientRect();
                var x = e.clientX - rect.left;
                var y = e.clientY - rect.top;
                
                // Scale coordinates
                if (isTextCanvas) {
                    // Text canvas is 640x400, scale to 320x200
                    x = Math.floor(x * 320 / rect.width);
                    y = Math.floor(y * 200 / rect.height);
                } else {
                    // Graphics canvas is 320x200
                    x = Math.floor(x * 320 / rect.width);
                    y = Math.floor(y * 200 / rect.height);
                }
                
                console.log('[JS] Calling handleMouseDown with button:', button, 'x:', x, 'y:', y);
                Module.ccall('handleMouseDown', null, ['number', 'number', 'number'], [button, x, y]);
                console.log('[JS] handleMouseDown called');
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
                
                // Scale coordinates
                if (isTextCanvas) {
                    x = Math.floor(x * 320 / rect.width);
                    y = Math.floor(y * 200 / rect.height);
                } else {
                    x = Math.floor(x * 320 / rect.width);
                    y = Math.floor(y * 200 / rect.height);
                }
                
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
        }
        
        // Setup events for both canvases
        var graphicsCanvas = document.getElementById('osaka-canvas');
        var textCanvas = document.getElementById('osaka-text-canvas');
        
        setupMouseEvents(graphicsCanvas, false);
        setupMouseEvents(textCanvas, true);
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
        // Use printf for logging (single argument version)
        printf("[C] handleMouseDown called\n");
        if (g_mouseHandler) {
            g_mouseHandler->OnMouseDown(button);
            printf("[C] OnMouseDown called on handler\n");
        } else {
            printf("[C] ERROR: g_mouseHandler is null!\n");
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

