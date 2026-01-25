#include <drivers/amd_am79c973.h>
#include <string.h>
#include <emscripten.h>
#include <memorymanagement.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

// RawDataHandler implementation
RawDataHandler::RawDataHandler(amd_am79c973* backend) {
    this->backend = backend;
    backend->SetHandler(this);
}

RawDataHandler::~RawDataHandler() {
    if (backend) {
        backend->SetHandler(0);
    }
}

bool RawDataHandler::OnRawDataReceived(uint8_t* buffer, uint32_t size) {
    (void)buffer;
    (void)size;
    return false;
}

void RawDataHandler::Send(uint8_t* buffer, uint32_t size) {
    if (backend) {
        backend->Send(buffer, size);
    }
}

amd_am79c973::amd_am79c973(PeripheralComponentInterconnectDeviceDescriptor* dev, InterruptManager* interrupts)
: Driver(),
  InterruptHandler(dev->interrupt + interrupts->HardwareInterruptOffset(), interrupts),
  MACAddress0Port(dev->portBase),
  MACAddress2Port(dev->portBase + 0x02),
  MACAddress4Port(dev->portBase + 0x04),
  registerDataPort(dev->portBase + 0x10),
  registerAddressPort(dev->portBase + 0x12),
  resetPort(dev->portBase + 0x14),
  busControlRegisterDataPort(dev->portBase + 0x16) {
    
    this->handler = 0;
    this->currentSendBuffer = 0;
    this->currentRecvBuffer = 0;
    memset(&initBlock, 0, sizeof(initBlock));
    memset(sendBufferDescrMemory, 0, sizeof(sendBufferDescrMemory));
    memset(recvBufferDescrMemory, 0, sizeof(recvBufferDescrMemory));
    sendBufferDescr = 0;
    recvBufferDescr = 0;
}

amd_am79c973::~amd_am79c973() {
}

void amd_am79c973::Activate() {
    // Stub for web - do nothing
}

int amd_am79c973::Reset() {
    // Stub for web - return 0
    return 0;
}

void amd_am79c973::SetIPAddress(uint32_t ip) {
    // Store IP address for web networking
    initBlock.logicalAddress = ip;
    
    // Notify JavaScript of IP address change
    EM_ASM_({
        if (Module.setNetworkIP) {
            // Convert from little-endian to dotted decimal
            var ip = $0;
            var ip1 = ip & 0xff;
            var ip2 = (ip >> 8) & 0xff;
            var ip3 = (ip >> 16) & 0xff;
            var ip4 = (ip >> 24) & 0xff;
            Module.setNetworkIP(ip4, ip3, ip2, ip1);
        }
    }, ip);
}

uint32_t amd_am79c973::GetIPAddress() {
    return initBlock.logicalAddress;
}

uint64_t amd_am79c973::GetMACAddress() {
    // Generate a fake MAC address for web (or use one from JavaScript)
    // Format: 02:XX:XX:XX:XX:XX (locally administered)
    uint64_t mac = 0x020000000000ULL;
    
    // Try to get MAC from JavaScript if available
    uint64_t jsMac = EM_ASM_INT({
        if (Module.getNetworkMAC) {
            return Module.getNetworkMAC();
        }
        return 0;
    });
    
    if (jsMac != 0) {
        return jsMac;
    }
    
    // Generate MAC based on IP address if available
    if (initBlock.logicalAddress != 0) {
        uint32_t ip = initBlock.logicalAddress;
        mac |= ((uint64_t)(ip & 0xff) << 32);
        mac |= ((uint64_t)((ip >> 8) & 0xff) << 24);
        mac |= ((uint64_t)((ip >> 16) & 0xff) << 16);
        mac |= ((uint64_t)((ip >> 24) & 0xff) << 8);
    }
    
    return mac;
}

void amd_am79c973::SetHandler(RawDataHandler* handler) {
    this->handler = handler;
}

void amd_am79c973::Send(uint8_t* buffer, int size) {
    // Send packet via JavaScript networking
    if (!buffer || size <= 0) return;
    
    // Copy buffer to heap-allocated memory that JavaScript can access
    uint8_t* heapBuffer = (uint8_t*)MemoryManager::activeMemoryManager->malloc(size);
    if (!heapBuffer) return;
    
    for (int i = 0; i < size; i++) {
        heapBuffer[i] = buffer[i];
    }
    
    // Send to JavaScript
    EM_ASM_({
        var ptr = $0;
        var size = $1;
        if (Module.sendNetworkPacket && ptr && size > 0) {
            try {
                var heapSize = HEAPU8.length;
                if (ptr >= 0 && ptr + size <= heapSize) {
                    var packet = new Uint8Array(HEAPU8.buffer, ptr, size);
                    Module.sendNetworkPacket(packet);
                }
            } catch (e) {
                console.error('[C] Error sending network packet:', e);
            }
        }
    }, heapBuffer, size);
    
    // Free the temporary buffer
    MemoryManager::activeMemoryManager->free(heapBuffer);
}

void amd_am79c973::Receive() {
    // Check for received packets from JavaScript
    if (!handler) return;
    
    // Poll for packets from JavaScript queue
    int packetPtr = EM_ASM_INT({
        if (Module.pollNetworkPackets) {
            Module.pollNetworkPackets();
        }
        // Check if there's a packet waiting
        if (Module.networkPackets && Module.networkPackets.length > 0) {
            var packet = Module.networkPackets[0];
            return packet.ptr;
        }
        return 0;
    });
    
    if (packetPtr != 0) {
        // Get packet size
        int packetSize = EM_ASM_INT({
            if (Module.networkPackets && Module.networkPackets.length > 0) {
                return Module.networkPackets[0].size;
            }
            return 0;
        });
        
        if (packetSize > 0 && handler) {
            // Notify handler of received packet
            uint8_t* buffer = (uint8_t*)packetPtr;
            handler->OnRawDataReceived(buffer, packetSize);
            
            // Remove packet from queue and free memory
            EM_ASM_({
                if (Module.networkPackets && Module.networkPackets.length > 0) {
                    var packet = Module.networkPackets.shift();
                    // Free the memory
                    if (Module._free && packet.ptr) {
                        Module._free(packet.ptr);
                    }
                }
            });
        }
    }
}

uint32_t amd_am79c973::HandleInterrupt(uint32_t esp) {
    // Stub for web
    return esp;
}

