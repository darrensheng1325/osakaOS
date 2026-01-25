#include <drivers/speaker.h>
#include <emscripten.h>

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

void sleep(uint32_t ms);

Speaker::Speaker() 
: PIT2(0x42),
  PITcommand(0x43),
  speakerPort(0x61) {
    
    // Initialize Web Audio API
    EM_ASM({
        if (!Module.audioContext) {
            Module.audioContext = new (window.AudioContext || window.webkitAudioContext)();
        }
    });
}

Speaker::~Speaker() {
}

void Speaker::PlaySound(uint32_t freq) {
    // Use Web Audio API for web version
    EM_ASM({
        if (!Module.audioContext) return;
        
        var frequency = $0;
        var oscillator = Module.audioContext.createOscillator();
        var gainNode = Module.audioContext.createGain();
        
        oscillator.connect(gainNode);
        gainNode.connect(Module.audioContext.destination);
        
        oscillator.frequency.value = frequency;
        oscillator.type = 'square'; // PC speaker uses square wave
        
        gainNode.gain.value = 0.1; // Volume
        
        oscillator.start();
        
        // Store reference to stop later
        if (!Module.currentOscillator) {
            Module.currentOscillator = [];
        }
        Module.currentOscillator.push(oscillator);
    }, freq);
}

void Speaker::NoSound() {
    // Stop all oscillators
    EM_ASM({
        if (Module.currentOscillator) {
            Module.currentOscillator.forEach(function(osc) {
                try {
                    osc.stop();
                    osc.disconnect();
                } catch(e) {}
            });
            Module.currentOscillator = [];
        }
    });
}

void Speaker::Speak(uint32_t freq) {
    this->PlaySound(freq);
    sleep(40);
    this->NoSound();
}

