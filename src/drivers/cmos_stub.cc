#include <drivers/cmos.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

CMOS::CMOS()
: WriteCMOS(0x70), ReadCMOS(0x71) {
    pit = nullptr;
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
    // Stub for web - return 0
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
    // Stub for web - do nothing
}

