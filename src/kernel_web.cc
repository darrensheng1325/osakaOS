// Web-compatible text mode rendering
// This replaces direct memory access to 0xb8000

#include <emscripten.h>
#include <emscripten/html5.h>
#include <string.h>
#include <common/types.h>

// Forward declaration for UTF8ToString
extern "C" {
    char* UTF8ToString(uint32_t ptr);
}

// Text mode buffer (80x25 characters)
static uint16_t textBuffer[80 * 25] = {0};
static uint8_t cursorX = 0;
static uint8_t cursorY = 0;

extern "C" {
    // Text mode rendering functions
    EMSCRIPTEN_KEEPALIVE void putcharTUI(unsigned char ch, unsigned char forecolor, 
            unsigned char backcolor, uint8_t x, uint8_t y) {
        if (x >= 80 || y >= 25) return;
        
        uint16_t attrib = (backcolor << 4) | (forecolor & 0x0f);
        textBuffer[80 * y + x] = ch | (attrib << 8);
        
        // Update canvas via JavaScript function
        EM_ASM_INT({
            if (Module.putCharTUI) {
                Module.putCharTUI($0, $1, $2, $3, $4);
            }
            return 0;
        }, x, y, ch, forecolor, backcolor);
    }
    
    // Main printf function implementation (shared by C and C++ versions)
    void printf_impl(char* strChr) {
        if (!strChr) {
            return; // Null pointer, nothing to print
        }
        
        // Use EM_ASM_INT to pass pointer as integer to JavaScript helper function
        // This avoids Emscripten pointer validation
        unsigned int ptr = reinterpret_cast<unsigned int>(strChr);
        
        // Call JavaScript helper function that safely reads and renders the string
        EM_ASM_INT({
            var ptr = $0;
            console.log('[C] printf called with pointer:', ptr);
            if (Module.safeReadAndPrintString) {
                Module.safeReadAndPrintString(ptr);
                console.log('[C] safeReadAndPrintString called');
            } else {
                console.error('[C] Module.safeReadAndPrintString not found!');
            }
            return 0;
        }, ptr);
        
        // Note: The original C++ string iteration is disabled because string literals
        // might be in read-only memory sections that aren't accessible via direct pointer access.
        // The JavaScript code above handles the string reading and rendering safely.
    }
    
    // Export C version of printf for JavaScript
    EMSCRIPTEN_KEEPALIVE void printf(char* strChr) {
        printf_impl(strChr);
    }
}

// C++ code should declare printf as extern "C" to use the C version above
// We provide C++ versions of printfLine and printfHex

void printfLine(const char* str, uint8_t line) {
    if (!str) return;
    
    // Use EM_ASM_INT to pass pointer as integer, avoiding validation
    unsigned int ptr = reinterpret_cast<unsigned int>(str);
    
    // Check if pointer is valid first
    int isValid = EM_ASM_INT({
        var ptr = $0;
        if (!ptr) return 0;
        try {
            var heapSize = HEAPU8.length;
            if (ptr >= 0 && ptr < heapSize) return 1;
        } catch (e) {}
        return 0;
    }, ptr);
    
    if (!isValid) return;
    
    // Now safely read and render
    EM_ASM_INT({
        var ptr = $0;
        var lineNum = $1;
        if (!ptr) return 0;
        
        try {
            var heapSize = HEAPU8.length;
            if (ptr < 0 || ptr >= heapSize) return 0;
            
            var maxLen = Math.min(80, 256, heapSize - ptr);
            for (var i = 0; i < maxLen; i++) {
                if (ptr + i >= heapSize) break;
                var ch = HEAPU8[ptr + i];
                if (ch === 0) break; // Null terminator
                
                // Update text buffer and render
                if (Module.putCharTUI) {
                    Module.putCharTUI(i, lineNum, ch, 0x07, 0x00);
                }
            }
        } catch (e) {
            // Silently fail
        }
        return 0;
    }, ptr, line);
}

// C++ linkage version of printfHex (calls C printf)
void printfHex(uint8_t key) {
    // Use a local array instead of string literal to avoid read-only memory access
    char foo[4] = "00 ";
    const char* hex = "0123456789ABCDEF";

    foo[0] = hex[(key >> 4) & 0x0F];
    foo[1] = hex[key & 0x0F];
    foo[2] = ' ';
    foo[3] = '\0';
    // Call the C printf function (declared above)
    printf(foo);
}

