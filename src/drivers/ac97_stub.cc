#include <drivers/ac97.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

AC97::AC97(PeripheralComponentInterconnectDeviceDescriptor* dev,
           InterruptManager* interrupts, uint32_t nabm_offset)
: Driver(),
  InterruptHandler(dev->interrupt + interrupts->HardwareInterruptOffset(), interrupts),
  resetPort(dev->portBase + 0x00),
  masterVolumePort(dev->portBase + 0x02),
  auxVolumePort(dev->portBase + 0x04),
  micVolumePort(dev->portBase + 0x0e),
  pcmVolumePort(dev->portBase + 0x18),
  inputDevicePort(dev->portBase + 0x1a),
  inputGainPort(dev->portBase + 0x1c),
  micGainPort(dev->portBase + 0x1e),
  extCapabilitiesPort(dev->portBase + 0x28),
  controlExtCapPort(dev->portBase + 0x2a),
  ratePcmFrontDacPort(dev->portBase + 0x2c),
  ratePcmSurrDacPort(dev->portBase + 0x2e),
  ratePcmLfeDacPort(dev->portBase + 0x30),
  ratePcmLeftRightPort(dev->portBase + 0x32),
  buffDescrAddressIN(dev->portBase + nabm_offset + 0x00),
  procDescrEntryNumIN(dev->portBase + nabm_offset + 0x04),
  allDescrEntryNumIN(dev->portBase + nabm_offset + 0x05),
  dataTransferStatusIN(dev->portBase + nabm_offset + 0x06),
  sampleTransferNumIN(dev->portBase + nabm_offset + 0x08),
  buffNextEntryNumIN(dev->portBase + nabm_offset + 0x0a),
  controlTransferIN(dev->portBase + nabm_offset + 0x0b),
  buffDescrAddressOUT(dev->portBase + nabm_offset + 0x10),
  procDescrEntryNumOUT(dev->portBase + nabm_offset + 0x14),
  allDescrEntryNumOUT(dev->portBase + nabm_offset + 0x15),
  dataTransferStatusOUT(dev->portBase + nabm_offset + 0x16),
  sampleTransferNumOUT(dev->portBase + nabm_offset + 0x18),
  buffNextEntryNumOUT(dev->portBase + nabm_offset + 0x1a),
  controlTransferOUT(dev->portBase + nabm_offset + 0x1b),
  globalControlRegister(dev->portBase + nabm_offset + 0x2c),
  globalStatusRegister(dev->portBase + nabm_offset + 0x30) {

    this->driverType = 2;
    bufferEntryNum = 0;
    bufferPtr = nullptr;
}

AC97::~AC97() {}

void AC97::Activate() {}

void AC97::PlaySound(AC97Buffer* buffer) { (void)buffer; }

uint32_t AC97::HandleInterrupt(uint32_t esp) { return esp; }
