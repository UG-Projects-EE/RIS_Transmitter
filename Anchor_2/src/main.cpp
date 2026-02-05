#include <ESP8266WiFi.h>
#include <espnow.h>

// ===== CONFIGURATION =====
#define ANCHOR_ID 2  // Set to 1, 2, or 3 for each anchor

// REPLACE WITH YOUR ESP32 RECEIVER MAC ADDRESS
uint8_t receiverMAC[] = { 0x80, 0xF3, 0xDA, 0x5D, 0xDB, 0x64 };; // ← CHANGE THIS!

// ===== PACKET STRUCTURE (MUST MATCH RECEIVER) =====
typedef struct {
  uint8_t anchor_id;
  uint32_t sequence;
  int8_t battery_level;
} AnchorPacket;

AnchorPacket packet;
uint32_t packetCounter = 0;
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL_MS = 100; // 10 packets/second

// ===== BATTERY MONITORING (OPTIONAL) =====
int8_t readBatteryLevel() {
  // For ESP8266, you can use ADC to read battery if connected
  // For now, return -1 (not available) or simulate
  return -1; // -1 means battery monitoring not available
}

// ===== SEND CALLBACK =====
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  if (sendStatus != 0) {
    Serial.print("Send failed, status: ");
    Serial.println(sendStatus);
  }
}

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=================================");
  Serial.println("     ANCHOR TRANSMITTER");
  Serial.println("=================================");
  
  // Set WiFi mode
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  
  // Display MAC address
  Serial.print("Anchor ");
  Serial.print(ANCHOR_ID);
  Serial.print(" MAC: ");
  Serial.println(WiFi.macAddress());
  
  // Initialize ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("ESP-NOW initialization failed!");
    delay(3000);
    ESP.restart();
  }
  
  // Set ESP-NOW role
  esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
  
  // Add receiver as peer
  if (esp_now_add_peer(receiverMAC, ESP_NOW_ROLE_SLAVE, 1, NULL, 0) != 0) {
    Serial.println("Failed to add peer!");
    delay(3000);
    ESP.restart();
  }
  
  // Register send callback
  esp_now_register_send_cb(OnDataSent);
  
  // Initialize packet
  packet.anchor_id = ANCHOR_ID;
  packet.battery_level = readBatteryLevel();
  
  Serial.print("Target MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.print(receiverMAC[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
  Serial.println("Transmitter ready!");
  Serial.println("=================================\n");
}

// ===== MAIN LOOP =====
void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = currentTime;
    
    // Update packet
    packet.sequence = packetCounter++;
    packet.battery_level = readBatteryLevel();
    
    // Send packet
    esp_now_send(receiverMAC, (uint8_t*)&packet, sizeof(packet));
    
    // Print status every 100 packets
    if (packetCounter % 100 == 0) {
      Serial.print("Anchor ");
      Serial.print(ANCHOR_ID);
      Serial.print(" packets sent: ");
      Serial.println(packetCounter);
    }
  }
  
  // Small delay to prevent watchdog
  delay(1);
}