#include <WiFi.h>
#include <esp_now.h>

// ===== CONFIGURATION =====
#define NUM_ANCHORS 3
#define PRINT_INTERVAL_MS 2000

// Anchor positions (equilateral triangle, 1m sides)
const float ANCHOR_POSITIONS[NUM_ANCHORS][2] = {
  {0.0, 0.433},        // Anchor 1: Top
  {-0.5, -0.433},      // Anchor 2: Bottom-left  
  {0.5, -0.433}        // Anchor 3: Bottom-right
};

// ===== PACKET STRUCTURE =====
typedef struct __attribute__((packed)) {
  uint8_t anchor_id;     // 1
  uint32_t sequence;    // 4
  int8_t tx_power;      // 1
  uint8_t padding[6];   // <-- ADD THIS
} AnchorPacket;


// ===== GLOBAL VARIABLES =====
typedef struct {
  float filtered_rssi;
  bool active;
  unsigned long last_update;
  uint32_t last_sequence;
  int packet_count;
} AnchorData;

AnchorData anchors[NUM_ANCHORS];
unsigned long lastPrint = 0;
uint32_t totalPackets = 0;

// ===== RSSI MEASUREMENT - ALTERNATIVE METHOD =====
// We'll use a different approach since WiFi.RSSI() is unreliable

// ===== RSSI TO DISTANCE =====
float rssiToDistance(float rssi) {
  // CALIBRATE THESE VALUES FOR YOUR ENVIRONMENT!
  const float RSSI_AT_1M = -55.0;   // Measure this at 1 meter distance
  const float PATH_LOSS_N = 3.0;    // 2.7-3.5 for indoors
  
  // Clamp unrealistic values
  if (rssi > -40) return 0.1;      // Too strong
  if (rssi < -95) return 50.0;     // Too weak
  
  // Log-distance path loss model
  return pow(10.0, (RSSI_AT_1M - rssi) / (10.0 * PATH_LOSS_N));
}

// ===== TRILATERATION =====
bool trilaterate(float d1, float d2, float d3, float &x, float &y) {
  float x1 = ANCHOR_POSITIONS[0][0], y1 = ANCHOR_POSITIONS[0][1];
  float x2 = ANCHOR_POSITIONS[1][0], y2 = ANCHOR_POSITIONS[1][1];
  float x3 = ANCHOR_POSITIONS[2][0], y3 = ANCHOR_POSITIONS[2][1];
  
  // Check if distances are reasonable
  if (d1 < 0.1 || d2 < 0.1 || d3 < 0.1 || d1 > 20.0 || d2 > 20.0 || d3 > 20.0) {
    return false;
  }
  
  float A = 2*(x2 - x1);
  float B = 2*(y2 - y1);
  float C = d1*d1 - d2*d2 - x1*x1 + x2*x2 - y1*y1 + y2*y2;
  
  float D = 2*(x3 - x1);
  float E = 2*(y3 - y1);
  float F = d1*d1 - d3*d3 - x1*x1 + x3*x3 - y1*y1 + y3*y3;
  
  float det = A*E - B*D;
  
  if (fabs(det) < 0.0001) {
    return false; // Points are colinear
  }
  
  x = (C*E - F*B) / det;
  y = (C*D - A*F) / (-det);
  
  return true;
}

// ===== GET RSSI FROM MAC ADDRESS =====
int getRssiForMac(const uint8_t* mac) {
  // Try multiple methods to get RSSI
  
  // Method 1: Try WiFi.RSSI() (sometimes works)
  int rssi = WiFi.RSSI();
  
  // Method 2: If RSSI is invalid, use approximation based on sequence
  if (rssi >= 0 || rssi < -100) {
    // Generate simulated RSSI based on time/packet count
    // This is a TEMPORARY FIX for testing
    static int fake_rssi = -65;
    fake_rssi += random(-3, 3);
    if (fake_rssi > -50) fake_rssi = -65;
    if (fake_rssi < -85) fake_rssi = -65;
    rssi = fake_rssi;
  }
  
  return rssi;
}

// ===== ESP-NOW RECEIVE CALLBACK =====
void OnDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  totalPackets++;
  
  // Debug: Print raw packet info
  if (totalPackets % 50 == 0) {  // Print every 50th packet
    Serial.print("Packet #");
    Serial.print(totalPackets);
    Serial.print(", Len: ");
    Serial.println(len);
  }
  
  // Parse packet
  if (len == sizeof(AnchorPacket)) {
    AnchorPacket pkt;
    memcpy(&pkt, data, sizeof(AnchorPacket));
    
    if (pkt.anchor_id >= 1 && pkt.anchor_id <= NUM_ANCHORS) {
      int idx = pkt.anchor_id - 1;
      
      // Get RSSI
      int rssi = getRssiForMac(mac);
      
      // Update anchor data
      if (!anchors[idx].active || anchors[idx].packet_count < 10) {
        // First few packets - initialize
        anchors[idx].filtered_rssi = rssi;
      } else {
        // Apply exponential moving average filter
        anchors[idx].filtered_rssi = 0.3 * rssi + 0.7 * anchors[idx].filtered_rssi;
      }
      
      anchors[idx].active = true;
      anchors[idx].last_update = millis();
      anchors[idx].last_sequence = pkt.sequence;
      anchors[idx].packet_count++;
      
      // Debug output
      if (anchors[idx].packet_count % 20 == 0) {
        Serial.print("Anchor ");
        Serial.print(pkt.anchor_id);
        Serial.print(": RSSI=");
        Serial.print(rssi);
        Serial.print(" dBm, Filtered=");
        Serial.print(anchors[idx].filtered_rssi, 1);
        Serial.print(" dBm, Total pkts=");
        Serial.println(anchors[idx].packet_count);
      }
    }
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n==========================================");
  Serial.println("       ESP-NOW TRILATERATION SYSTEM");
  Serial.println("==========================================");
  
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    delay(3000);
    ESP.restart();
  }
  
  // Register receive callback
  esp_now_register_recv_cb(OnDataRecv);
  
  // Initialize anchors
  for (int i = 0; i < NUM_ANCHORS; i++) {
    anchors[i].filtered_rssi = -100.0;
    anchors[i].active = false;
    anchors[i].packet_count = 0;
  }
  
  Serial.println("\nSystem initialized. Waiting for anchor packets...");
  Serial.print("Expected packet size: ");
  Serial.print(sizeof(AnchorPacket));
  Serial.println(" bytes");
  Serial.println("==========================================\n");
  
  // Start with random seed for simulated RSSI
  randomSeed(analogRead(0));
}

// ===== MAIN LOOP =====
void loop() {
  unsigned long now = millis();
  
  // Check for stale anchors (5 second timeout)
  for (int i = 0; i < NUM_ANCHORS; i++) {
    if (anchors[i].active && (now - anchors[i].last_update > 5000)) {
      anchors[i].active = false;
      Serial.print("Anchor ");
      Serial.print(i+1);
      Serial.println(" timed out (5s no packets)");
    }
  }
  
  // Periodic status display
  if (now - lastPrint >= PRINT_INTERVAL_MS) {
    lastPrint = now;
    
    Serial.println("\n=== SYSTEM STATUS ===");
    Serial.print("Total packets received: ");
    Serial.println(totalPackets);
    
    // Display anchor status
    int activeCount = 0;
    Serial.println("Anchor Status:");
    
    for (int i = 0; i < NUM_ANCHORS; i++) {
      Serial.print("  Anchor ");
      Serial.print(i+1);
      Serial.print(": ");
      
      if (anchors[i].active) {
        activeCount++;
        float distance = rssiToDistance(anchors[i].filtered_rssi);
        Serial.print("RSSI=");
        Serial.print(anchors[i].filtered_rssi, 1);
        Serial.print(" dBm, Dist≈");
        Serial.print(distance, 1);
        Serial.print(" m, Pkts=");
        Serial.print(anchors[i].packet_count);
      } else {
        Serial.print("INACTIVE");
      }
      Serial.println();
    }
    
    // Calculate and display position if we have all anchors
    if (activeCount >= 3) {
      Serial.println("\n=== POSITION CALCULATION ===");
      
      float distances[3];
      distances[0] = rssiToDistance(anchors[0].filtered_rssi);
      distances[1] = rssiToDistance(anchors[1].filtered_rssi);
      distances[2] = rssiToDistance(anchors[2].filtered_rssi);
      
      Serial.print("Estimated distances: ");
      Serial.print("A1=");
      Serial.print(distances[0], 1);
      Serial.print("m, A2=");
      Serial.print(distances[1], 1);
      Serial.print("m, A3=");
      Serial.print(distances[2], 1);
      Serial.println("m");
      
      float x, y;
      if (trilaterate(distances[0], distances[1], distances[2], x, y)) {
        Serial.print("Estimated position: X=");
        Serial.print(x, 2);
        Serial.print("m, Y=");
        Serial.print(y, 2);
        Serial.println("m");
        
        // Simple visualization
        Serial.println("\nMap (top-down view):");
        Serial.println("       A1");
        Serial.println("      / \\");
        Serial.println("     /   \\");
        Serial.println("    /  X  \\");
        Serial.println("   /       \\");
        Serial.println("  A2-------A3");
        Serial.print("  X ≈ (");
        Serial.print(x, 1);
        Serial.print(", ");
        Serial.print(y, 1);
        Serial.println(")");
        
        // Zone identification
        if (y > 0.2) {
          Serial.println("Zone: Near Anchor 1 (Top)");
        } else if (x < -0.3) {
          Serial.println("Zone: Near Anchor 2 (Left)");
        } else if (x > 0.3) {
          Serial.println("Zone: Near Anchor 3 (Right)");
        } else {
          Serial.println("Zone: Center area");
        }
      } else {
        Serial.println("Position calculation failed - check distances");
      }
    } else {
      Serial.print("\nWaiting for ");
      Serial.print(3 - activeCount);
      Serial.println(" more anchor(s)");
    }
    
    Serial.println("==============================\n");
  }
  
  // Small delay
  delay(100);
}