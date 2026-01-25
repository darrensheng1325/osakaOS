# osakaOS Web Edition

This is the Emscripten port of osakaOS, allowing it to run in a web browser.

## Building

### Prerequisites

1. Install Emscripten SDK:
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   source ./emsdk_env.sh
   ```

2. Make sure you have the required dependencies (same as the original osakaOS build).

### Compilation

```bash
make -f Makefile.emscripten
```

This will generate:
- `osakaOS.js` - The main JavaScript file
- `osakaOS.wasm` - The WebAssembly binary

### Running

1. Start a local web server (required for loading WASM files):
   ```bash
   python3 -m http.server 8000
   # or
   npx serve
   ```

2. Open `http://localhost:8000` in your web browser.

## Differences from Native Version

### Hardware Abstraction

- **VGA Driver**: Uses HTML5 Canvas instead of direct hardware access
- **Keyboard/Mouse**: Uses DOM events instead of hardware interrupts
- **Timer (PIT)**: Uses JavaScript `setInterval` instead of hardware timer
- **Speaker**: Uses Web Audio API instead of PC speaker
- **Storage**: ATA driver is stubbed (no persistent storage)
- **Network**: Network drivers are stubbed (no network functionality)
- **PCI/CMOS**: Hardware detection is stubbed

### Graphics

- Graphics mode (320x200) is rendered to a Canvas element
- Text mode (80x25) is rendered to a separate Canvas element
- EGA palette is preserved and converted to RGB for web display

### Input

- Keyboard input is captured via DOM `keydown`/`keyup` events
- Mouse input is captured via DOM `mousedown`/`mouseup`/`mousemove` events
- Key mappings are converted from JavaScript key codes to PS/2 scan codes

### Limitations

- No persistent file system (uses in-memory storage only)
- No network functionality
- Some hardware-specific features may not work
- Performance may be slower than native version

## Controls

- **Keyboard**: Standard keyboard input works as in the native version
- **Mouse**: Click and drag on the canvas
- **Windows/Command Key**: Enter GUI mode (same as native)

## Troubleshooting

### Canvas not displaying

- Make sure the canvas elements exist in the HTML
- Check browser console for errors
- Verify WebAssembly is supported in your browser

### Input not working

- Click on the canvas to focus it
- Check browser console for JavaScript errors
- Verify event handlers are properly set up

### Performance issues

- Try reducing the canvas size
- Check browser performance tools
- Some features may be slower in web version

## Notes

This is a port of the original osakaOS to run in web browsers. Some features may not work exactly as in the native version due to browser limitations and security restrictions.

