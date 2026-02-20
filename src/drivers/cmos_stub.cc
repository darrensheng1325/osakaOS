#include <drivers/cmos.h>
#include <emscripten.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

CMOS::CMOS()
: WriteCMOS(0x70), ReadCMOS(0x71) {
    pit = nullptr;
    // Initialize timeData with current time
    ReadRTC();
}

CMOS::~CMOS() {
}

void CMOS::CMOS_OUT(uint8_t val, uint8_t reg) {
    // Stub for web - do nothing
    (void)val;
    (void)reg;
}

uint8_t CMOS::CMOS_IN(uint8_t reg) {
    // Stub for web - return 0
    (void)reg;
    return 0;
}

int32_t CMOS::GetUpdate() {
    // Stub for web - return 0 (no update in progress)
    return 0;
}

uint8_t CMOS::GetRegisterRTC(int32_t reg) {
    // Stub for web - return 0
    (void)reg;
    return 0;
}

void CMOS::DumpRTC() {
    // Stub for web - do nothing
}

void CMOS::ReadRTC() {
    // Use JavaScript Date API to get current time
    // Get values from JavaScript and write them directly in C++
    uint8_t second = EM_ASM_INT({
        return new Date().getSeconds();
    });
    uint8_t minute = EM_ASM_INT({
        return new Date().getMinutes();
    });
    uint8_t hour = EM_ASM_INT({
        return new Date().getHours();
    });
    uint8_t day = EM_ASM_INT({
        return new Date().getDate();
    });
    uint8_t month = EM_ASM_INT({
        return new Date().getMonth() + 1; // JavaScript months are 0-based
    });
    int32_t year = EM_ASM_INT({
        return new Date().getFullYear() % 100; // Last 2 digits of year
    });
    
    // Write directly to the timeData structure
    timeData.second = second;
    timeData.minute = minute;
    timeData.hour = hour;
    timeData.day = day;
    timeData.month = month;
    timeData.year = year;
}

