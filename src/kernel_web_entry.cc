// Web entry point for osakaOS
#include <emscripten.h>
#include <emscripten/html5.h>

extern "C" void kernelMain(void* multiboot_structure, uint32_t magicnumber);

// Web version entry point - does NOT start kernel automatically
// Kernel will be started manually from JavaScript via onRuntimeInitialized
extern "C" int main() {
    // Just return - kernel will be started manually from JavaScript
    // This prevents any blocking during page load
    return 0;
}

