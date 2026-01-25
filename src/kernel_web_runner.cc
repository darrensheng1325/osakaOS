// Web-compatible kernel runner using Emscripten main loop
#include <emscripten.h>
#include <emscripten/html5.h>

extern "C" void kernelMain(void* multiboot_structure, uint32_t magicnumber);

// Global state for kernel execution
static bool kernel_initialized = false;
static bool kernel_running = false;
static uint32_t* fake_multiboot = nullptr;

// Kernel state machine
enum KernelState {
    KERNEL_INIT,
    KERNEL_CLI_LOOP,
    KERNEL_GUI_LOOP,
    KERNEL_DONE
};

static KernelState kernel_state = KERNEL_INIT;

// Forward declarations for kernel functions we need to call
extern "C" {
    // These will be called from the main loop
    void kernelMain(void* multiboot_structure, uint32_t magicnumber);
}

// Main loop function that runs the kernel in chunks
static void kernel_main_loop() {
    if (!kernel_initialized) {
        // Initialize kernel
        if (fake_multiboot) {
            kernelMain(fake_multiboot, 0x2BADB002);
            kernel_initialized = true;
            kernel_running = true;
        }
    }
    // The kernel's infinite loops will handle themselves
    // but they now yield via emscripten_sleep
}

extern "C" int main() {
    // Create a fake multiboot structure for web
    static uint32_t mb[16] = {0};
    mb[1] = 0x100000; // Memory upper bound (1MB)
    fake_multiboot = mb;
    
    // Set up main loop to run kernel
    // Use 0 FPS (infinite) but simulate with requestAnimationFrame
    emscripten_set_main_loop(kernel_main_loop, 0, 1);
    
    // Actually, let's run kernel initialization immediately but async
    emscripten_async_call([](void* arg) {
        kernelMain((uint32_t*)arg, 0x2BADB002);
        kernel_initialized = true;
        kernel_running = true;
    }, fake_multiboot, 0);
    
    return 0;
}

