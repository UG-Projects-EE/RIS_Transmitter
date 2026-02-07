#include <Arduino_RouterBridge.h>

extern "C" void matrixWrite(const uint32_t* buf);
extern "C" void matrixBegin();

volatile int ris_cols[13] = {0};

int t_step = 0;
unsigned long last_render = 0;
unsigned long last_fetch = 0;

void setup() {
  delay(3000); // Safety delay for Linux boot
  matrixBegin();
  
  // Flash White
  uint32_t all[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
  matrixWrite(all);
  delay(500);
  uint32_t clr[4] = {0};
  matrixWrite(clr);

  Bridge.begin();
}

void loop() {
  unsigned long now = millis();

  // 1. DATA FETCH LOOP (Every 1.5 Seconds)
  // This controls the speed of the QPSK Symbol Cycle
  if (now - last_fetch > 4000) {
    last_fetch = now;
    
    String dataString;
    // Ask Python for the next symbol
    bool ok = Bridge.call("get_ris_frame").result(dataString);
    
    if (ok && dataString.length() > 0) {
       parseCSV(dataString);
    }
  }

  // 2. RENDER LOOP (Every 100ms)
  // This controls the flicker speed (Time Coding)
  if (now - last_render > 1000) {
    last_render = now;
    renderFrame(t_step);
    t_step = (t_step + 1) % 4; 
  }
}

void parseCSV(String data) {
  int col_idx = 0;
  int start = 0;
  for (int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == ',') {
      if (col_idx < 13) {
        ris_cols[col_idx] = data.substring(start, i).toInt();
        col_idx++;
      }
      start = i + 1;
    }
  }
  if (col_idx < 13) {
     ris_cols[col_idx] = data.substring(start).toInt();
  }
}

void setPixel(uint32_t* frame, int r, int c) {
  int idx = r * 13 + c;
  if (idx < 104) frame[idx / 32] |= (1UL << (idx % 32));
}

void renderFrame(int t) {
  uint32_t frame[4] = {0};

  for (int c = 0; c < 13; c++) {
    int pat = ris_cols[c];
    if ((pat >> t) & 1) {
      // Draw Full Height Bar
      for(int r=0; r<8; r++) setPixel(frame, r, c);
    }
  }
  matrixWrite(frame);
}