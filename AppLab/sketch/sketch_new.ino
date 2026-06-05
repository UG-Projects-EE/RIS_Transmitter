#include <Arduino_RouterBridge.h>

// The 16 Output Pins driving your RIS / LEDs
const int ris_pins[16] = {
  2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 
  A0, A1, A2, A3
};

volatile int ris_cols[16] = {0};

// Hardware Pointers and Pre-calculated Masks for Direct Port Manipulation
volatile uint32_t* pin_BSRR[16]; 
uint32_t hardware_commands[4][16]; // [time_slot][pin_index]

int t_step = 0;
unsigned long last_fetch = 0;

void setup() {
  delay(3000); // Safety delay for Linux boot

  for (int i = 0; i < 16; i++) {
    pinMode(ris_pins[i], OUTPUT);
    digitalWrite(ris_pins[i], LOW); 
    
    // ==========================================================
    // DEEP HARDWARE MAPPING (STM32 BSRR Lookup)
    // ==========================================================
    // 1. Get the Arduino Core Port and Bitmask for this specific pin
    uint32_t port = digitalPinToPort(ris_pins[i]);
    
    // 2. portOutputRegister points to the ODR (Output Data Register).
    // In STM32 architecture, the BSRR register is located exactly 
    // 4 bytes (one 32-bit word) after the ODR in memory.
    volatile uint32_t* odr_ptr = portOutputRegister(port);
    pin_BSRR[i] = odr_ptr + 1; 
  }

  Bridge.begin();
}

void loop() {
  unsigned long now = millis();

  // ========================================================
  // 1. DATA FETCH LOOP (Executes once every 4 seconds)
  // ========================================================
  if (now - last_fetch > 4000) {
    last_fetch = now;
    
    String dataString;
    bool ok = Bridge.call("get_ris_frame").result(dataString);
    
    if (ok && dataString.length() > 0) {
       parseCSV(dataString);
       compileHardwareCommands(); // Pre-calculate BSRR commands!
    }
  }

  // ========================================================
  // 2. BARE-METAL RF DRIVE SEQUENCE (Direct BSRR Memory Writes)
  // 16 sequential pointer dereferences = ~32 clock cycles.
  // At 160 MHz, this achieves a toggle rate of ~5 MHz!
  // ========================================================
  *pin_BSRR[0]  = hardware_commands[t_step][0];
  *pin_BSRR[1]  = hardware_commands[t_step][1];
  *pin_BSRR[2]  = hardware_commands[t_step][2];
  *pin_BSRR[3]  = hardware_commands[t_step][3];
  *pin_BSRR[4]  = hardware_commands[t_step][4];
  *pin_BSRR[5]  = hardware_commands[t_step][5];
  *pin_BSRR[6]  = hardware_commands[t_step][6];
  *pin_BSRR[7]  = hardware_commands[t_step][7];
  *pin_BSRR[8]  = hardware_commands[t_step][8];
  *pin_BSRR[9]  = hardware_commands[t_step][9];
  *pin_BSRR[10] = hardware_commands[t_step][10];
  *pin_BSRR[11] = hardware_commands[t_step][11];
  *pin_BSRR[12] = hardware_commands[t_step][12];
  *pin_BSRR[13] = hardware_commands[t_step][13];
  *pin_BSRR[14] = hardware_commands[t_step][14];
  *pin_BSRR[15] = hardware_commands[t_step][15];
  
  // Fast bitwise advance: 0, 1, 2, 3 -> loop
  t_step = (t_step + 1) & 3; 
}

// ========================================================
// COMPILER: Converts binary patterns into STM32 BSRR commands
// ========================================================
void compileHardwareCommands() {
  for (int t = 0; t < 4; t++) {
    for (int c = 0; c < 16; c++) {
      uint32_t mask = digitalPinToBitMask(ris_pins[c]);
      int state = (ris_cols[c] >> t) & 1;
      
      if (state == 1) {
        // To SET a pin, write the mask to the lower 16 bits of BSRR
        hardware_commands[t][c] = mask;
      } else {
        // To RESET a pin, write the mask to the upper 16 bits of BSRR
        hardware_commands[t][c] = (mask << 16);
      }
    }
  }
}

// ========================================================
// PARSER
// ========================================================
void parseCSV(String data) {
  int col_idx = 0;
  int start = 0;
  for (int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == ',') {
      if (col_idx < 16) {
        ris_cols[col_idx] = data.substring(start, i).toInt();
        col_idx++;
      }
      start = i + 1;
    }
  }
  if (col_idx < 16) {
     ris_cols[col_idx] = data.substring(start).toInt();
  }
}
