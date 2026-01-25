#include <drivers/keyboard.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <string.h>

using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

// Global keyboard handler for web events
static KeyboardEventHandler* g_keyboardHandler = nullptr;

// Key code mapping from JavaScript key codes to PS/2 scan codes
static uint8_t jsKeyToScanCode(const char* key) {
    // Map common keys - this is a simplified mapping
    if (key && strcmp(key, "Enter") == 0) return 0x1c;
    if (key && strcmp(key, "Backspace") == 0) return 0x0e;
    if (key && strcmp(key, "Tab") == 0) return 0x0f;
    if (key && strcmp(key, "Escape") == 0) return 0x01;
    if (key && strcmp(key, "Space") == 0) return 0x39;
    if (key && strcmp(key, "ArrowLeft") == 0) return 0x4b;
    if (key && strcmp(key, "ArrowUp") == 0) return 0x48;
    if (key && strcmp(key, "ArrowDown") == 0) return 0x50;
    if (key && strcmp(key, "ArrowRight") == 0) return 0x4d;
    if (key && strcmp(key, "Meta") == 0) return 0x5b; // Windows/Command key
    
    // Letters
    if (key && strlen(key) == 1 && key[0] >= 'a' && key[0] <= 'z') {
        return 0x10 + (key[0] - 'a'); // Approximate mapping
    }
    if (key && strlen(key) == 1 && key[0] >= 'A' && key[0] <= 'Z') {
        return 0x10 + (key[0] - 'A');
    }
    
    // Numbers
    if (key && strlen(key) == 1 && key[0] >= '1' && key[0] <= '9') {
        return 0x02 + (key[0] - '1');
    }
    if (key && key[0] == '0') return 0x0b;
    
    return 0;
}

extern "C" {
    EMSCRIPTEN_KEEPALIVE void setKeyboardHandler(void* handler) {
        g_keyboardHandler = (KeyboardEventHandler*)handler;
    }
    
    EM_JS(void, setupKeyboardEvents, (), {
        var handler = null;
        
        function keyToScanCode(key, shift, caps) {
            var code = 0;
            var isLetter = key.length === 1 && key >= 'a' && key <= 'z';
            var isUpper = key.length === 1 && key >= 'A' && key <= 'Z';
            
            if (isLetter || isUpper) {
                var base = key.toLowerCase().charCodeAt(0) - 'a'.charCodeAt(0);
                code = 0x10 + base;
            } else if (key >= '0' && key <= '9') {
                code = 0x02 + (key === '0' ? 9 : parseInt(key) - 1);
            } else {
                switch(key) {
                    case 'Enter': code = 0x1c; break;
                    case 'Backspace': code = 0x0e; break;
                    case 'Tab': code = 0x0f; break;
                    case 'Escape': code = 0x01; break;
                    case ' ': code = 0x39; break;
                    case 'ArrowLeft': code = 0x4b; break;
                    case 'ArrowUp': code = 0x48; break;
                    case 'ArrowDown': code = 0x50; break;
                    case 'ArrowRight': code = 0x4d; break;
                    case 'Meta': code = 0x5b; break;
                    default: code = key.charCodeAt(0); break;
                }
            }
            return code;
        }
        
        document.addEventListener('keydown', function(e) {
            if (!Module._g_keyboardHandler) return;
            
            var key = e.key;
            var shift = e.shiftKey;
            var ctrl = e.ctrlKey;
            var alt = e.altKey;
            var caps = e.getModifierState('CapsLock');
            
            // Set modifier states
            Module.ccall('setKeyboardModifiers', null, ['number', 'number', 'number', 'number'], 
                [shift ? 1 : 0, ctrl ? 1 : 0, alt ? 1 : 0, caps ? 1 : 0]);
            
            // Convert key to character
            var char = key;
            if (key.length === 1) {
                if (shift && key >= 'a' && key <= 'z') {
                    char = key.toUpperCase();
                } else if (!shift && key >= 'A' && key <= 'Z') {
                    char = key.toLowerCase();
                }
            } else if (key === 'Enter') {
                char = '\n';
            } else if (key === 'Backspace') {
                char = '\b';
            } else if (key === 'Tab') {
                char = '\t';
            } else if (key === 'Escape') {
                char = '\x1b';
            } else if (key === ' ') {
                char = ' ';
            } else if (key === 'ArrowLeft') {
                char = '\xfc';
            } else if (key === 'ArrowUp') {
                char = '\xfd';
            } else if (key === 'ArrowDown') {
                char = '\xfe';
            } else if (key === 'ArrowRight') {
                char = '\xff';
            } else {
                return; // Unknown key
            }
            
            // Call handler
            Module.ccall('handleKeyDown', null, ['number'], [char.charCodeAt(0)]);
        });
        
        document.addEventListener('keyup', function(e) {
            if (!Module._g_keyboardHandler) return;
            
            var key = e.key;
            var shift = e.shiftKey;
            var ctrl = e.ctrlKey;
            var alt = e.altKey;
            
            Module.ccall('setKeyboardModifiers', null, ['number', 'number', 'number', 'number'], 
                [shift ? 1 : 0, ctrl ? 1 : 0, alt ? 1 : 0, 0]);
            
            var char = key;
            if (key.length === 1) {
                char = key.toLowerCase();
            } else if (key === ' ') {
                char = ' ';
            }
            
            if (char.length === 1) {
                Module.ccall('handleKeyUp', null, ['number'], [char.charCodeAt(0)]);
            }
        });
    });
}

KeyboardEventHandler::KeyboardEventHandler() {
    numCode = 0;
    ctrl = false;
    alt = false;
    shift = false;
    caps = false;
    fkey = 0;
    cli = false;
    keyValue = 0;
}

void KeyboardEventHandler::OnKeyDown(char ch) {
    // Virtual - implemented by derived classes
}

void KeyboardEventHandler::OnKeyUp(char ch) {
    // Virtual - implemented by derived classes
}

void KeyboardEventHandler::resetMode() {
    // Virtual - implemented by derived classes
}

void KeyboardEventHandler::modeSet(uint8_t) {
    // Virtual - implemented by derived classes
}

KeyboardDriver::KeyboardDriver(InterruptManager* manager, KeyboardEventHandler *handler)
: InterruptHandler(0x21, manager),
  dataport(0x60),
  commandport(0x64) {
    this->handler = handler;
    g_keyboardHandler = handler;
    
    // Setup web keyboard events
    EM_ASM({
        setupKeyboardEvents();
    });
}

KeyboardDriver::~KeyboardDriver() {
    if (g_keyboardHandler == handler) {
        g_keyboardHandler = nullptr;
    }
}

void KeyboardDriver::Activate() {
    // Web keyboard is always active via DOM events
}

// Web version - called from JavaScript
extern "C" {
    EMSCRIPTEN_KEEPALIVE void handleKeyDown(uint8_t keyCode) {
        if (g_keyboardHandler) {
            char ch = (char)keyCode;
            g_keyboardHandler->OnKeyDown(ch);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE void handleKeyUp(uint8_t keyCode) {
        if (g_keyboardHandler) {
            char ch = (char)keyCode;
            g_keyboardHandler->OnKeyUp(ch);
        }
    }
    
    EMSCRIPTEN_KEEPALIVE void setKeyboardModifiers(uint8_t shift, uint8_t ctrl, uint8_t alt, uint8_t caps) {
        if (g_keyboardHandler) {
            g_keyboardHandler->shift = shift != 0;
            g_keyboardHandler->ctrl = ctrl != 0;
            g_keyboardHandler->alt = alt != 0;
            g_keyboardHandler->caps = caps != 0;
        }
    }
}

uint32_t KeyboardDriver::HandleInterrupt(uint32_t esp) {
    // Web version - interrupts are handled via JavaScript events
    // This is called from the interrupt manager's event loop
    return esp;
}

