#include <drivers/vga.h>
#include <gui/font.h>
#include <gui/pixelart.h>
#include <math.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <string.h>

using namespace os;
using namespace os::gui;
using namespace os::common;
using namespace os::drivers;
using namespace os::math;

extern "C" {
    // JavaScript functions for canvas rendering
    EMSCRIPTEN_KEEPALIVE void js_put_pixel(int x, int y, int r, int g, int b);
    EMSCRIPTEN_KEEPALIVE void js_clear_screen();
    EMSCRIPTEN_KEEPALIVE void js_flush_canvas();
}

uint16_t strlen(char*);
uint8_t Web2EGA(uint32_t webColor);

// EGA color palette (256 colors)
static uint8_t ega_palette[256][3] = {0};

VideoGraphicsArray::VideoGraphicsArray() :
    miscPort(0x3c2),
    crtcIndexPort(0x3d4),
    crtcDataPort(0x3d5),
    sequencerIndexPort(0x3c4),
    sequencerDataPort(0x3c5),
    colorPaletteMask(0x3c6),
    colorRegisterRead(0x3c7),
    colorRegisterWrite(0x3c8),
    colorDataPort(0x3c9),
    graphicsControllerIndexPort(0x3ce),
    graphicsControllerDataPort(0x3cf),
    attributeControllerIndexPort(0x3c0),
    attributeControllerReadPort(0x3c1),
    attributeControllerWritePort(0x3c0),
    attributeControllerResetPort(0x3da) {
    
    FrameBufferSegment = pixels; // Use pixels array as framebuffer
    memset(pixels, 0, sizeof(pixels));
    
    // Initialize default EGA palette - use the same calculation as the original VGA driver
    // VGA writes 6-bit values (0-63) to palette registers, which need to be scaled to 8-bit (0-255)
    // Scale factor: 255/63 ≈ 4.048, but we use 4 for exact match with QEMU
    for (uint16_t color = 0; color < 256; color++) {
        uint8_t r6 = 0, g6 = 0, b6 = 0; // 6-bit values (0-63)
        uint8_t r = 0, g = 0, b = 0;    // 8-bit values (0-255)
        
        switch (color / 64) {
            case 0:
                r6 = (color & 0x20 ? 0x15 : 0) | (color & 0x04 ? 0x2a : 0);
                g6 = (color & 0x10 ? 0x15 : 0) | (color & 0x02 ? 0x2a : 0);
                b6 = (color & 0x08 ? 0x15 : 0) | (color & 0x01 ? 0x2a : 0);
                // Scale 6-bit to 8-bit: multiply by 4
                r = r6 * 4;
                g = g6 * 4;
                b = b6 * 4;
                break;
            case 1:
                r6 = ((color & 0x20 ? 0x15 : 0) | (color & 0x04 ? 0x2a : 0)) >> 3;
                g6 = ((color & 0x10 ? 0x15 : 0) | (color & 0x02 ? 0x2a : 0)) >> 3;
                b6 = ((color & 0x08 ? 0x15 : 0) | (color & 0x01 ? 0x2a : 0)) >> 3;
                r = r6 * 4;
                g = g6 * 4;
                b = b6 * 4;
                break;
            case 2:
                r6 = ((color & 0x20 ? 0x15 : 0) | (color & 0x04 ? 0x2a : 0)) << 3;
                g6 = ((color & 0x10 ? 0x15 : 0) | (color & 0x02 ? 0x2a : 0)) << 3;
                b6 = ((color & 0x08 ? 0x15 : 0) | (color & 0x01 ? 0x2a : 0)) << 3;
                // Clamp to 63 (6-bit max) before scaling
                if (r6 > 63) r6 = 63;
                if (g6 > 63) g6 = 63;
                if (b6 > 63) b6 = 63;
                r = r6 * 4;
                g = g6 * 4;
                b = b6 * 4;
                break;
            default:
                r = g = b = 0;
                break;
        }
        
        ega_palette[color][0] = r;
        ega_palette[color][1] = g;
        ega_palette[color][2] = b;
    }
}

VideoGraphicsArray::~VideoGraphicsArray() {
}

void VideoGraphicsArray::WriteRegisters(uint8_t* registers) {
    // Stub for web - do nothing
    (void)registers;
}

bool VideoGraphicsArray::SupportsMode(uint32_t width, uint32_t height, uint32_t colordepth) {
    return width == WIDTH_13H && height == HEIGHT_13H && colordepth == 8;
}

bool VideoGraphicsArray::SetMode(uint32_t width, uint32_t height, uint32_t colordepth) {
    if (!SupportsMode(width, height, colordepth)) {
        return false;
    }
    
    FrameBufferSegment = pixels;
    memset(pixels, 0, sizeof(pixels));
    
    // Initialize palette (already done in constructor, but ensure it's set)
    return true;
}

void VideoGraphicsArray::PaletteUpdate(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 256) {
        ega_palette[index][0] = r;
        ega_palette[index][1] = g;
        ega_palette[index][2] = b;
    }
}

uint8_t* VideoGraphicsArray::GetFrameBufferSegment() {
    return pixels;
}

void VideoGraphicsArray::PutPixel(int32_t x, int32_t y, uint8_t color) {
    if (x >= 0 && WIDTH_13H > x && y >= 0 && HEIGHT_13H > y) {
        pixels[(y<<8)+(y<<6)+x] = color;
    }
}

void VideoGraphicsArray::PutPixelRaw(int32_t x, int32_t y, uint8_t colorIndex) {
    PutPixel(x, y, colorIndex);
}

void VideoGraphicsArray::DarkenPixel(int32_t x, int32_t y) {
    if (x >= 0 && WIDTH_13H > x && y >= 0 && HEIGHT_13H > y) {
        pixels[(y<<8)+(y<<6)+x] = light2dark[pixels[(y<<8)+(y<<6)+x]];
    }
}

uint8_t VideoGraphicsArray::ReadPixel(int32_t x, int32_t y) {
    if (x < 0 || WIDTH_13H <= x || y < 0 || HEIGHT_13H <= y) { return 0; }
    return pixels[WIDTH_13H*y+x];
}

void VideoGraphicsArray::PutText(char* str, int32_t x, int32_t y, uint8_t color) {
    uint16_t length = strlen(str);
    
    if ((WIDTH_13H - x) < (length * 5)) { return; }
    
    uint8_t* charArr = charset[0];
    
    for (int i = 0; str[i] != '\0'; i++) {
        charArr = charset[(uint8_t)(str[i])-32];
        
        for (uint8_t w = 0; w < font_width; w++) {
            for (uint8_t h = 0; h < font_height; h++) {
                if (charArr[w] && ((charArr[w] >> h) & 1)) {
                    this->PutPixel(x+w, y+h, color);
                }
            }
        }
        x += font_width;
    }
}

void VideoGraphicsArray::FillBufferFull(int32_t x, int32_t y, 
                    int32_t w, int32_t h, uint8_t* buf) {
    for (int32_t Y = y; Y < y+h; Y++) {
        for (int32_t X = x; X < x+w; X++) {
            this->PutPixel(X, Y, buf[WIDTH_13H*(Y-y)+(X-x)]);
        }
    }
}

void VideoGraphicsArray::FillBuffer(int16_t x, int16_t y, 
                int16_t w, int16_t h, uint8_t* buf, bool mirror) {
    uint8_t pixelColor = 0;
    uint8_t scrollVert = 0;
    bool scroll = false;
    
    if (y < 0) {
        if (y+h < 0) { return; }
        scrollVert = (y * -1);
        h -= (y * -1);
        y = 0;
        scroll = true;
    }
    
    for (int16_t Y = y; Y < y+h; Y++) {
        for (int16_t X = x; X < x+w; X++) {
            if (mirror) { 
                pixelColor = buf[w*(Y-y+scrollVert)+(x+w-X-1)];
            } else { 
                pixelColor = buf[w*(Y-y+scrollVert)+(X-x)]; 
            }
            
            if (pixelColor) {
                this->PutPixel(X, Y, pixelColor);
            }
        }
    }
}

void VideoGraphicsArray::FillRectangle(int32_t x, int32_t y, 
        int32_t w, int32_t h, uint8_t color) {
    for (int32_t Y = y; Y < y+h; Y++) {
        for (int32_t X = x; X < x+w; X++) {
            this->PutPixel(X, Y, color);
        }
    }
}

void VideoGraphicsArray::DrawRectangle(int32_t x, int32_t y, 
        int32_t w, int32_t h, uint8_t color) {
    for (int32_t X = x; X < x+w; X++) {
        this->PutPixel(X, y,     color);
        this->PutPixel(X, y+h-1, color);
    }
    
    for (int32_t Y = y; Y < y+h; Y++) {
        this->PutPixel(x,     Y, color);
        this->PutPixel(x+w-1, Y, color);
    }
}

void VideoGraphicsArray::DrawLineFlat(int32_t x0, int32_t y0, 
                int32_t x1, int32_t y1,
                uint8_t color,
                bool x) {
    if (x) {
        for (int32_t X = x0; X < x1; X++) {
            this->PutPixel(X, y0, color);
        }
    } else {
        for (int32_t Y = y0; Y < y1; Y++) {
            this->PutPixel(x0, Y, color);
        }
    }
}

void VideoGraphicsArray::DrawLineLow(int32_t x0, int32_t y0, 
                int32_t x1, int32_t y1,
                uint8_t color) {
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t yi = 1;
    
    if (dy < 0) {
        yi = -1;
        dy = -dy;
    }
    int16_t D = (2*dy) - dx;
    int16_t y = y0;
    
    for (int x = x0; x < x1; x++) {
        this->PutPixel(x, y, color);
        
        if (D > 0) {
            y += yi;
            D += (2*(dy-dx));
        } else {
            D += 2*dy;
        }
    }
}

void VideoGraphicsArray::DrawLineHigh(int32_t x0, int32_t y0, 
                int32_t x1, int32_t y1,
                uint8_t color) {
    int16_t dx = x1 - x0;
    int16_t dy = y1 - y0;
    int16_t xi = 1;
    
    if (dx < 0) {
        xi = -1;
        dx = -dx;
    }
    int16_t D = (2*dx) - dy;
    int16_t x = x0;
    
    for (int y = y0; y < y1; y++) {
        this->PutPixel(x, y, color);
        
        if (D > 0) {
            x += xi;
            D += (2*(dx-dy));
        } else {
            D += 2*dx;
        }
    }
}

void VideoGraphicsArray::DrawLine(int32_t x0, int32_t y0, 
                int32_t x1, int32_t y1,
                uint8_t color) {
    if (::abs(y1 - y0) < ::abs(x1 - x0)) {
        if (x0 > x1) {  
            DrawLineLow(x1, y1, x0, y0, color);
        } else {
            DrawLineLow(x0, y0, x1, y1, color); 
        }
    } else {
        if (y0 > y1) {  
            DrawLineHigh(x1, y1, x0, y0, color);
        } else {
            DrawLineHigh(x0, y0, x1, y1, color); 
        }
    }
}

void VideoGraphicsArray::FillPolygon(uint16_t x[], uint16_t y[], 
                     uint8_t edgeNum, 
                     uint8_t color) {
    int16_t i, j, temp = 0;
    uint16_t xmin = WIDTH_13H; 
    uint16_t xmax = 0;
    
    for (i = 0; i < edgeNum; i++) {
        if (x[i] < xmin) {
            xmin = x[i];
        }
        if (x[i] > xmax) {
            xmax = x[i];
        }
    }
    
    for (i = xmin; i <= xmax; i++) {
        uint16_t interPoints[edgeNum];
        uint16_t count = 0;
        
        for (j = 0; j < edgeNum; j++) {
            uint16_t next = (j + 1) % edgeNum;
            
            if ((y[j] > i && y[next] <= i) || (y[next] > i && y[j] <= i)) {
                interPoints[count++] = x[j] + (i - y[j]) 
                    * (x[next] - x[j]) / (y[next] - y[j]);
            }
        }
        
        for (j = 0; j < count-1; j++) {
            for (int k = 0; k < count-j-1; k++) {
                if (interPoints[k] > interPoints[k+1]) {
                    temp = interPoints[k];
                    interPoints[k] = interPoints[k+1];
                    interPoints[k+1] = temp;
                }
            }
        }
        
        for (j = 0; j < count; j += 2) {
            this->DrawLine(interPoints[j], i, interPoints[j+1], i, color);
        }
    }
}

void VideoGraphicsArray::MakeDark(uint8_t darkness) {
    if (darkness > 0) {
        for (uint8_t y = 0; y < HEIGHT_13H; y++) {
            for (uint16_t x = 0; x < WIDTH_13H; x++) {
                for (uint8_t i = 0; i < darkness; i++) {
                    pixels[(y<<8)+(y<<6)+x] = light2dark[pixels[(y<<8)+(y<<6)+x]];
                }
            }
        }
    }
}

void VideoGraphicsArray::MakeWave(uint8_t waveLength) {
    if (!waveLength) { return; }
    
    uint16_t offset = waveLength;
    bool incOrDec = true;
    
    for (uint8_t y = 0; y < HEIGHT_13H; y++) {
        for (uint16_t x = offset; x < WIDTH_13H; x++) {
            pixels[(y<<8)+(y<<6)+(x-offset)] = pixels[(y<<8)+(y<<6)+x];
        }
        
        if (offset > waveLength) { incOrDec = false; }
        if (offset == 0) { incOrDec = true; }
        
        if (incOrDec) { offset++;
        } else { offset--; }
    }
}

// Draw to canvas via JavaScript
void VideoGraphicsArray::DrawToScreen() {
    // Call JavaScript function defined in HTML
    // Cast pixels pointer to unsigned int to avoid validation
    unsigned int pixelsPtr = reinterpret_cast<unsigned int>(pixels);
    EM_ASM_INT({
        var ptr = $0;
        console.log('[VGA] DrawToScreen called, pixels pointer:', ptr);
        if (Module.renderPixels) {
            Module.renderPixels(ptr);
            console.log('[VGA] renderPixels called successfully');
        } else {
            console.error('[VGA] Module.renderPixels not found!');
        }
        return 0;
    }, pixelsPtr);
}

void VideoGraphicsArray::FSdither(uint32_t* buf, uint16_t w, uint16_t h) {
    uint8_t oldPixel = 0;
    uint8_t newPixel = 0;
    uint8_t quantError = 0;
    
    for (uint16_t y = 0; y < h; y++) {
        for (uint16_t x = 0; x < w; x++) {
            oldPixel = buf[w*y+x];
            newPixel = Web2EGA(oldPixel);
            buf[w*y+x] = newPixel;
            quantError = oldPixel - newPixel;
            buf[w*(y)+(x+1)]   += quantError*7 / 16;
            buf[w*(y+1)+(x-1)] += quantError*3 / 16;
            buf[w*(y+1)+(x)]   += quantError*5 / 16;
            buf[w*(y+1)+(x+1)] += quantError / 16;
        }
    }
}

void VideoGraphicsArray::ErrorScreen() {
    this->FillRectangle(0, 0, WIDTH_13H, HEIGHT_13H, 0x09);
    this->FillRectangle(0, 29, 300, 101, 0x3f);
    
    this->PutText("Sorry, osakaOS experienced a critical error. :(", 1, 11, 0x40);
    this->PutText("Sorry, osakaOS experienced a critical error. :(", 0, 10, 0x3f);
    
    this->PutText("ERROR CODE: ", 0, 30, 0x09);
    this->PutText("0x61 0x72 0x65 0x20 0x79 0x6F 0x75 0x20 0x66 0x75 0x63 0x6B 0x69", 0, 40, 0x09);
    this->PutText("0x6E 0x67 0x20 0x72 0x65 0x74 0x61 0x72 0x74 0x65 0x64 0x3F 0x20", 0, 50, 0x09);
    this->PutText("0x64 0x6F 0x20 0x79 0x6F 0x75 0x20 0x73 0x65 0x72 0x69 0x6F 0x75", 0, 60, 0x09);
    this->PutText("0x73 0x6C 0x79 0x20 0x74 0x68 0x69 0x6E 0x6B 0x20 0x64 0x6F 0x69", 0, 70, 0x09);
    this->PutText("0x6E 0x67 0x20 0x74 0x68 0x61 0x74 0x20 0x73 0x68 0x69 0x74 0x20", 0, 80, 0x09);
    this->PutText("0x74 0x6F 0x20 0x79 0x6F 0x75 0x72 0x20 0x63 0x6F 0x6D 0x70 0x75", 0, 90, 0x09);
    this->PutText("0x74 0x65 0x72 0x20 0x77 0x61 0x73 0x20 0x61 0x20 0x67 0x6F 0x6F", 0, 100, 0x09);
    this->PutText("0x64 0x20 0x69 0x64 0x65 0x61 0x3F 0x20 0x6B 0x69 0x6C 0x6C 0x20", 0, 110, 0x09);
    this->PutText("0x79 0x6F 0x75 0x72 0x73 0x65 0x6C 0x66", 0, 120, 0x09);
    
    this->PutText("Sending all your data to our remote servers...", 1, 151, 0x40);
    this->PutText("Sending all your data to our remote servers...", 0, 150, 0x3f);
    this->PutText("Don't turn off your kitchen lights.", 1, 161, 0x40);
    this->PutText("Don't turn off your kitchen lights.", 0, 160, 0x3f);
    
    for (int i = 20; i < 180; i += 20) {
        this->FillBuffer(304, i, 13, 20, cursorClickLeft, false);
    }
}

