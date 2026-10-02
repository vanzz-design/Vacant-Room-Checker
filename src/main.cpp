#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>

// ESP32 GPIO Pins for RC522
#define SS_PIN  5
#define RST_PIN 2

MFRC522 rfid(SS_PIN, RST_PIN);

// Network & Database Credentials
const char* ssid = "sixseven";             // Your router's Wi-Fi name
const char* password = "SIXSEVEN";     // Your router's Wi-Fi password
const char* supabase_url = "https://bltyimpbtazngmfjprxy.supabase.co/rest/v1/rfid_logs";
const char* supabase_key = "sb_publishable_t9g0Pb40MrHgWYtrx2kC_Q_ufByRXtL"; // Insert your sb_publishable_... key here

// Room details and toggle state
String roomId = "CS-101";
bool isOccupied = false; // Default starting state

void setup() {
  Serial.begin(115200);
  
  // Initialize SPI bus and RFID reader
  SPI.begin();
  rfid.PCD_Init();

  rfid.PCD_DumpVersionToSerial();

  Serial.println("RFID Reader Ready. Tap a card.");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi Connected!");
}

void loop() {
  // Check if a new card is present and can be read
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  // Extract the UID from the card
  String cardUID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    cardUID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
    cardUID += String(rfid.uid.uidByte[i], HEX);
  }
  cardUID.toUpperCase();
  
  // Stop reading this specific tap to prevent rapid-fire triggering
  rfid.PICC_HaltA();
  
  // -----------------------------------------------------
  // THE TOGGLE LOGIC: Flip the state on every successful tap
  // -----------------------------------------------------
  isOccupied = !isOccupied; 
  String status = isOccupied ? "Occupied" : "Vacant";
  
  Serial.println("-------------------------");
  Serial.println("Card UID: " + cardUID);
  Serial.println("Room Status toggled to: " + status);

  // Transmit the new status to Supabase
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(supabase_url);
    
    // Set REST API Headers
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", supabase_key);
    http.addHeader("Authorization", String("Bearer ") + String(supabase_key));
    http.addHeader("Prefer", "return=minimal");

    // Construct the JSON payload
    String jsonPayload = "{\"card_uid\":\"" + cardUID + "\", \"room_id\":\"" + roomId + "\", \"status\":\"" + status + "\"}";
    
    // Execute POST request
    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode == 201) {
      Serial.println("Success! Database updated.");
    } else {
      Serial.println("Error Code: " + String(httpResponseCode));
      Serial.println("Response Body: " + http.getString());
    }
    http.end();
  } else {
    Serial.println("Wi-Fi disconnected. Update failed.");
  }

  // 3-second cooldown before allowing the next scan
  delay(3000); 
}