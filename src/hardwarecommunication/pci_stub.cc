#include <hardwarecommunication/pci.h>
#include <drivers/driver.h>
#include <memorymanagement.h>

using namespace os;
using namespace os::common;
using namespace os::hardwarecommunication;
using namespace os::drivers;

PeripheralComponentInterconnectController::PeripheralComponentInterconnectController(MemoryManager* memoryManager)
: dataport(0xCFC),
  commandport(0xCF8) {
    this->memoryManager = memoryManager;
}

PeripheralComponentInterconnectController::~PeripheralComponentInterconnectController() {
}

uint32_t PeripheralComponentInterconnectController::Read(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset) {
    // Stub for web - return 0
    (void)bus;
    (void)device;
    (void)function;
    (void)registeroffset;
    return 0;
}

void PeripheralComponentInterconnectController::Write(uint16_t bus, uint16_t device, uint16_t function, uint32_t registeroffset, uint32_t value) {
    // Stub for web - do nothing
    (void)bus;
    (void)device;
    (void)function;
    (void)registeroffset;
    (void)value;
}

bool PeripheralComponentInterconnectController::DeviceHasFunctions(uint16_t bus, uint16_t device) {
    // Stub for web - return false
    (void)bus;
    (void)device;
    return false;
}

void PeripheralComponentInterconnectController::SelectDrivers(DriverManager* drvManager, InterruptManager* interrupts) {
    // Stub for web - do nothing
    (void)drvManager;
    (void)interrupts;
}

BaseAddressRegister PeripheralComponentInterconnectController::GetBaseAddressRegister(uint16_t bus, uint16_t device, uint16_t function, uint16_t bar) {
    // Stub for web - return empty BAR
    (void)bus;
    (void)device;
    (void)function;
    (void)bar;
    BaseAddressRegister result;
    result.type = InputOutput;
    result.address = 0;
    result.prefetchable = false;
    return result;
}

Driver* PeripheralComponentInterconnectController::GetDriver(PeripheralComponentInterconnectDeviceDescriptor dev, InterruptManager* interrupts) {
    // Stub for web - return null
    (void)dev;
    (void)interrupts;
    return nullptr;
}

PeripheralComponentInterconnectDeviceDescriptor PeripheralComponentInterconnectController::GetDeviceDescriptor(uint16_t bus, uint16_t device, uint16_t function) {
    // Stub for web - return empty descriptor
    (void)bus;
    (void)device;
    (void)function;
    PeripheralComponentInterconnectDeviceDescriptor result;
    return result;
}

