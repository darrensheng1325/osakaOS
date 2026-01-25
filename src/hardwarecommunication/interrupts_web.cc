#include <hardwarecommunication/interrupts.h>
#include <emscripten.h>

using namespace os;
using namespace os::common;
using namespace os::hardwarecommunication;

void printf(char* str);
void printfHex(uint8_t);

InterruptHandler::InterruptHandler(uint8_t interruptNumber, InterruptManager* interruptManager) {
    this->interruptNumber = interruptNumber;
    this->interruptManager = interruptManager;
    
    if (interruptManager) {
        interruptManager->handlers[interruptNumber] = this;
    }
}

InterruptHandler::~InterruptHandler() {
    if (interruptManager && interruptManager->handlers[interruptNumber] == this) {
        interruptManager->handlers[interruptNumber] = 0;
    }
}

uint32_t InterruptHandler::HandleInterrupt(uint32_t esp) { 
    return esp; 
}

InterruptManager::GateDescriptor InterruptManager::interruptDescriptorTable[256];
InterruptManager* InterruptManager::ActiveInterruptManager = 0;

void InterruptManager::SetInterruptDescriptorTableEntry(uint8_t interruptNumber, uint16_t codeSegmentSelectorOffset, 
        void (*handler)(), uint8_t DescriptorPrivilegeLevel, uint8_t DescriptorType) {
    // Stub for web - do nothing
    (void)interruptNumber;
    (void)codeSegmentSelectorOffset;
    (void)handler;
    (void)DescriptorPrivilegeLevel;
    (void)DescriptorType;
}

void InterruptManager::IgnoreInterruptRequest() {
    // Stub for web
}

// Stub interrupt handlers
void InterruptManager::HandleInterruptRequest0x00() {}
void InterruptManager::HandleInterruptRequest0x01() {}
void InterruptManager::HandleInterruptRequest0x02() {}
void InterruptManager::HandleInterruptRequest0x03() {}
void InterruptManager::HandleInterruptRequest0x04() {}
void InterruptManager::HandleInterruptRequest0x05() {}
void InterruptManager::HandleInterruptRequest0x06() {}
void InterruptManager::HandleInterruptRequest0x07() {}
void InterruptManager::HandleInterruptRequest0x08() {}
void InterruptManager::HandleInterruptRequest0x09() {}
void InterruptManager::HandleInterruptRequest0x0A() {}
void InterruptManager::HandleInterruptRequest0x0B() {}
void InterruptManager::HandleInterruptRequest0x0C() {}
void InterruptManager::HandleInterruptRequest0x0D() {}
void InterruptManager::HandleInterruptRequest0x0E() {}
void InterruptManager::HandleInterruptRequest0x0F() {}
void InterruptManager::HandleInterruptRequest0x31() {}
void InterruptManager::HandleInterruptRequest0x80() {}

void InterruptManager::HandleException0x00() {}
void InterruptManager::HandleException0x01() {}
void InterruptManager::HandleException0x02() {}
void InterruptManager::HandleException0x03() {}
void InterruptManager::HandleException0x04() {}
void InterruptManager::HandleException0x05() {}
void InterruptManager::HandleException0x06() {}
void InterruptManager::HandleException0x07() {}
void InterruptManager::HandleException0x08() {}
void InterruptManager::HandleException0x09() {}
void InterruptManager::HandleException0x0A() {}
void InterruptManager::HandleException0x0B() {}
void InterruptManager::HandleException0x0C() {}
void InterruptManager::HandleException0x0D() {}
void InterruptManager::HandleException0x0E() {}
void InterruptManager::HandleException0x0F() {}
void InterruptManager::HandleException0x10() {}
void InterruptManager::HandleException0x11() {}
void InterruptManager::HandleException0x12() {}
void InterruptManager::HandleException0x13() {}

InterruptManager::InterruptManager(uint16_t hardwareInterruptOffset, GlobalDescriptorTable* gdt, TaskManager* taskManager) 
: picMasterCommand(0x20),
  picMasterData(0x21), 
  picSlaveCommand(0xA0), 
  picSlaveData(0xA1) {
    
    // Yield early to prevent blocking
    #ifdef __EMSCRIPTEN__
    emscripten_sleep(0);
    #endif
    
    this->taskManager = taskManager;
    this->hardwareInterruptOffset = hardwareInterruptOffset;
    this->interruptCount = 1;
    this->boot = false;
    
    // Initialize handlers array
    // Split into smaller chunks to avoid long blocking
    for (int i = 0; i < 128; i++) {
        handlers[i] = 0;
    }
    
    #ifdef __EMSCRIPTEN__
    emscripten_sleep(0);
    #endif
    
    for (int i = 128; i < 256; i++) {
        handlers[i] = 0;
    }
    
    #ifdef __EMSCRIPTEN__
    emscripten_sleep(0);
    #endif
}

InterruptManager::~InterruptManager() { 
    Deactivate(); 
}

uint16_t InterruptManager::HardwareInterruptOffset() {
    return hardwareInterruptOffset;
}

void InterruptManager::Activate() {
    if (ActiveInterruptManager != 0) {
        ActiveInterruptManager->Deactivate();
    }
    
    ActiveInterruptManager = this;
    // Web version - no assembly needed
}

void InterruptManager::Deactivate() {
    if (ActiveInterruptManager == this) {
        ActiveInterruptManager = 0;
    }
}

uint32_t InterruptManager::handleInterrupt(uint8_t interruptNumber, uint32_t esp) {
    if (ActiveInterruptManager != 0) {
        return ActiveInterruptManager->DoHandleInterrupt(interruptNumber, esp);
    }
    return esp;
}

uint32_t InterruptManager::DoHandleInterrupt(uint8_t interruptNumber, uint32_t esp) {
    if (handlers[interruptNumber] != 0) {
        esp = handlers[interruptNumber]->HandleInterrupt(esp);
    } else if (interruptNumber != hardwareInterruptOffset && interruptNumber != 0x2e) {
        // Unhandled interrupt - log it
        // printf("UNHANDLED INTERRUPT ");
        // printfHex(interruptNumber);
        // printf("\n");
    }
    
    // Compute tasks (timer interrupt)
    if (interruptNumber == hardwareInterruptOffset) {
        if (taskManager) {
            esp = (uint32_t)taskManager->Schedule((CPUState*)esp);
        }
    }
    
    return esp;
}

