#include <hardwarecommunication/port.h>

using namespace os::hardwarecommunication;

Port::Port(os::common::uint16_t portnumber) {
    this->portnumber = portnumber;
}

Port::~Port() {
}

Port8Bit::Port8Bit(os::common::uint16_t portnumber)
: Port(portnumber) {
}

Port8Bit::~Port8Bit() {
}

os::common::uint8_t Port8Bit::Read() {
    // Stub for web - return 0
    return 0;
}

void Port8Bit::Write(os::common::uint8_t data) {
    // Stub for web - do nothing
    (void)data;
}

Port8BitSlow::Port8BitSlow(os::common::uint16_t portnumber)
: Port8Bit(portnumber) {
}

Port8BitSlow::~Port8BitSlow() {
}

void Port8BitSlow::Write(os::common::uint8_t data) {
    // Stub for web - do nothing
    (void)data;
}

Port16Bit::Port16Bit(os::common::uint16_t portnumber)
: Port(portnumber) {
}

Port16Bit::~Port16Bit() {
}

os::common::uint16_t Port16Bit::Read() {
    // Stub for web - return 0
    return 0;
}

void Port16Bit::Write(os::common::uint16_t data) {
    // Stub for web - do nothing
    (void)data;
}

Port32Bit::Port32Bit(os::common::uint16_t portnumber)
: Port(portnumber) {
}

Port32Bit::~Port32Bit() {
}

os::common::uint32_t Port32Bit::Read() {
    // Stub for web - return 0
    return 0;
}

void Port32Bit::Write(os::common::uint32_t data) {
    // Stub for web - do nothing
    (void)data;
}

