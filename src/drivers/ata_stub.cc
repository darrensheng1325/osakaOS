#include <drivers/ata.h>
#include <string.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

AdvancedTechnologyAttachment::AdvancedTechnologyAttachment(uint16_t portBase, bool master)
: dataPort(portBase),
  errorPort(portBase + 0x01),
  sectorCountPort(portBase + 0x02),
  lbaLowPort(portBase + 0x03),
  lbaMidPort(portBase + 0x04),
  lbaHiPort(portBase + 0x05),
  devicePort(portBase + 0x06),
  commandPort(portBase + 0x07),
  controlPort(portBase + 0x206) {
    
    bytesPerSector = 512;
    this->master = master;
}

AdvancedTechnologyAttachment::~AdvancedTechnologyAttachment() {
}

bool AdvancedTechnologyAttachment::Identify() {
    // Stub for web - return false (no disk)
    return false;
}

void AdvancedTechnologyAttachment::Read28(uint32_t sector, uint8_t* data, int count, int offset) {
    // Stub for web - zero out data
    (void)sector;
    (void)offset;
    if (data && count > 0) {
        memset(data, 0, count);
    }
}

void AdvancedTechnologyAttachment::Write28(uint32_t sector, uint8_t* data, int count, int offset) {
    // Stub for web - do nothing
    (void)sector;
    (void)data;
    (void)count;
    (void)offset;
}

void AdvancedTechnologyAttachment::Flush() {
    // Stub for web - do nothing
}

