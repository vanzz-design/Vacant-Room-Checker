
#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// ESP32 GPIO Pins for RC522
#define SS_PIN  5
#define RST_PIN 2

MFRC522 rfid(SS_PIN, RST_PIN);

// OLED Definitions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1 // Share the ESP32 reset pin

Adafruit_SH1106G display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Network & Database Credentials
const char* ssid = "MJHAY";             // Your router's Wi-Fi name
const char* password = "123456789";     // Your router's Wi-Fi password

const char* supabase_url = "https://bltyimpbtazngmfjprxy.supabase.co/rest/v1/rfid_logs";

const char* supabase_key = "sb_publishable_t9g0Pb40MrHgWYtrx2kC_Q_ufByRXtL"; // Insert your sb_publishable_... key here

// Room details and toggle state
String roomId = "CS-101";

bool isOccupied = false; // Default starting state

// Helper function to show the standby screen
void showReadyScreen() {

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 10);

  display.println("Scanner Ready");
  display.println("Room: " + roomId);
  display.println("");
  display.println("Tap Card to toggle");

  display.display();
}

void setup() {

  Serial.begin(115200);

  // Initialize OLED
  if (!display.begin(0x3C, true)) {

    Serial.println(F("SH1106 allocation failed"));

    for (;;) ;
  }

  // Show Boot Screen
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 20);

  display.println("System Booting...");

  display.display();

  // Initialize SPI bus and RFID reader
  SPI.begin();

  rfid.PCD_Init();

  rfid.PCD_DumpVersionToSerial();

  Serial.println("RFID Reader Ready. Tap a card.");

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");

  display.clearDisplay();

  display.setCursor(0, 20);

  display.println("Connecting to Wi-Fi...");

  display.display();

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);

    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");

  // Display the default ready screen
  showReadyScreen();
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

  // Update OLED to show processing
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 10);

  display.println("Processing...");
  display.println("UID: " + cardUID);

  display.display();

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

    http.addHeader(
      "Authorization",
      String("Bearer ") + String(supabase_key)
    );

    http.addHeader("Prefer", "return=minimal");

    // Construct the JSON payload
    String jsonPayload =
      "{\"card_uid\":\"" + cardUID +
      "\", \"room_id\":\"" + roomId +
      "\", \"status\":\"" + status + "\"}";

    // Execute POST request
    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode == 201) {

      Serial.println("Success! Database updated.");

      // OLED Success Screen
      display.clearDisplay();

      display.setTextSize(2);
      display.setCursor(0, 10);

      display.println("SUCCESS!");

      display.setTextSize(1);

      display.println("");
      display.println("Set to: " + status);

      display.display();

    } else {

      Serial.println(
        "Error Code: " + String(httpResponseCode)
      );

      Serial.println(
        "Response Body: " + http.getString()
      );

      // OLED Error Screen
      display.clearDisplay();

      display.setTextSize(2);
      display.setCursor(0, 10);

      display.println("ERROR");

      display.setTextSize(1);

      display.println(
        "Code: " + String(httpResponseCode)
      );

      display.display();
    }

    http.end();

  } else {

    Serial.println("Wi-Fi disconnected. Update failed.");

    // OLED Offline Screen
    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(0, 10);

    display.println("OFFLINE");

    display.setTextSize(1);

    display.println("Check Wi-Fi");

    display.display();
  }

  // 3-second cooldown before allowing the next scan
  delay(3000);

  // Return to the standby screen
  showReadyScreen();
}
