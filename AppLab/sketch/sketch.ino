#include <Arduino_RouterBridge.h> 
#include <Arduino_LED_Matrix.h>
#include "ris_codebook.h"        

ArduinoLEDMatrix matrix;
uint32_t frame[] = { 0, 0, 0 }; 

// The RPC Function Python calls
void set_ris_beam(int idx) {
    if (idx < 0 || idx > 3) return;

    // Flash all LEDs to prove data arrived
    frame[0] = 0xFFFFFFFF; frame[1] = 0xFFFFFFFF; frame[2] = 0xFFFFFFFF;
    matrix.loadFrame(frame);
    delay(100); 

    // Clear and apply pattern
    frame[0] = 0; frame[1] = 0; frame[2] = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 4; c++) {
            if (RIS_LOOKUP[idx][r][c] == 1) {
                int bit_pos = (r * 4) + c;
                if (bit_pos < 32) frame[0] |= (1UL << (31 - bit_pos));
            }
        }
    }
    matrix.loadFrame(frame);
}

void setup() {
    // 1. Start LEDs FIRST (So we see if it's alive)
    matrix.begin();
    
    // 2. Show "I am awake" dot (Bottom-Right)
    frame[2] = 0x1; 
    matrix.loadFrame(frame);
    
    // 3. Start Bridge LAST (If this hangs, the dot stays on)
    Bridge.begin();
}

void loop() {
    // HEARTBEAT: This pixel will blink every 500ms
    // If it stops blinking, the MCU has crashed.
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 500) {
        frame[2] ^= 0x1; 
        matrix.loadFrame(frame);
        lastBlink = millis();
    }
}