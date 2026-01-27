#include <drivers/mouse.h>
#include <gui/desktop.h>
#include <emscripten.h>
#include <emscripten/html5.h>

extern "C" {
    void printf(char* strChr);
}

using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;
using namespace os::gui;

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
            
            var isMouseDown = false;
            
            var isPointerLocked = false;
            
            // Request pointer lock on click for graphics canvas
            if (!isTextCanvas) {
                // Handle pointer lock change
                var pointerLockChangeHandler = function() {
                    isPointerLocked = document.pointerLockElement === canvas;
                    console.log('[JS] Pointer lock:', isPointerLocked ? 'locked' : 'unlocked');
                };
                document.addEventListener('pointerlockchange', pointerLockChangeHandler);
                
                document.addEventListener('pointerlockerror', function() {
                    console.error('[JS] Pointer lock error');
                    isPointerLocked = false;
                });
            }
            
            canvas.addEventListener('mousedown', function(e) {
                e.preventDefault(); // Prevent default to allow dragging
                isMouseDown = true;
                if (!Module._g_mouseHandler) {
                    console.error('[JS] Mouse handler not available!');
                    return;
                }
                
                var button = 1; // Left button
                if (e.button === 1) button = 2; // Middle button
                if (e.button === 2) button = 3; // Right button
                
                var x, y;
                
                // When pointer lock is active, e.clientX/Y might reflect the center position
                // where the browser moved the cursor, not the actual click position.
                // In this case, use the current tracked position instead.
                if (isPointerLocked && !isTextCanvas) {
                    // Use current tracked position when pointer locked
                    // The click happened at the current mouse position
                    x = Module._lastMouseX || 160;
                    y = Module._lastMouseY || 100;
                    console.log('[JS] Pointer locked - using tracked position:', x, y);
                } else {
                    // Use actual click coordinates when not pointer locked
                    var rect = canvas.getBoundingClientRect();
                    x = e.clientX - rect.left;
                    y = e.clientY - rect.top;
                    
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
                    
                    // Update tracked position with actual click coordinates
                    Module._lastMouseX = x;
                    Module._lastMouseY = y;
                }
                
                // Request pointer lock after setting position (only for graphics canvas)
                // Only request if not already locked
                if (!isTextCanvas && !isPointerLocked && canvas.requestPointerLock) {
                    canvas.requestPointerLock();
                }
                
                Module.ccall('handleMouseDown', null, ['number', 'number', 'number'], [button, x, y]);
            });
            
            canvas.addEventListener('mouseup', function(e) {
                e.preventDefault();
                isMouseDown = false;
                if (!Module._g_mouseHandler) return;
                
                var button = 1;
                if (e.button === 1) button = 2;
                if (e.button === 2) button = 3;
                
                Module.ccall('handleMouseUp', null, ['number'], [button]);
            });
            
            // Handle mouse leave to cancel drag
            canvas.addEventListener('mouseleave', function(e) {
                if (isMouseDown) {
                    isMouseDown = false;
                    if (Module._g_mouseHandler) {
                        // Release all buttons on mouse leave
                        Module.ccall('handleMouseUp', null, ['number'], [1]);
                    }
                }
            });
            
            canvas.addEventListener('mousemove', function(e) {
                if (!Module._g_mouseHandler) return;
                
                var dx = 0, dy = 0;
                
                if (isPointerLocked && !isTextCanvas) {
                    // Use movementX/Y for pointer locked mode (relative movement)
                    dx = e.movementX || 0;
                    dy = e.movementY || 0;
                    
                    // Update absolute position
                    var x = (Module._lastMouseX || 160) + dx;
                    var y = (Module._lastMouseY || 100) + dy;
                    // Clamp to canvas bounds
                    x = Math.max(0, Math.min(319, x));
                    y = Math.max(0, Math.min(199, y));
                    Module._lastMouseX = x;
                    Module._lastMouseY = y;
                } else {
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
                    dx = x - lastX;
                    dy = y - lastY;
                    
                    Module._lastMouseX = x;
                    Module._lastMouseY = y;
                }
                
                // Only send movement if there's actual movement
                if (dx !== 0 || dy !== 0) {
                    Module.ccall('handleMouseMove', null, ['number', 'number'], [dx, dy]);
                }
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
        if (g_mouseHandler) {
            // Cast to Desktop to access MouseX and MouseY
            // Desktop inherits from MouseEventHandler, so this is safe
            Desktop* desktop = dynamic_cast<Desktop*>(g_mouseHandler);
            if (desktop) {
                // Clamp coordinates to valid range
                if (x < 0) x = 0;
                if (x >= 320) x = 319;
                if (y < 0) y = 0;
                if (y >= 200) y = 199;
                
                // Directly set the mouse position to the click coordinates
                // This ensures the mouse is at the correct position when clicking
                // Don't use relative movement - just set the absolute position
                desktop->MouseX = x;
                desktop->MouseY = y;
            } else {
                // If not Desktop, try to move to position (for other handlers)
                // This assumes current position is at center (160, 100) for 320x200
                int dx = x - 160;
                int dy = y - 100;
                g_mouseHandler->OnMouseMove(dx, dy);
            }
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

