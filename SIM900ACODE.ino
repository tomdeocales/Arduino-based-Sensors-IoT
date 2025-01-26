#include <WiFi.h>
#include <FirebaseESP32.h>

// Firebase and WiFi credentials
#define FIREBASE_HOST ""
#define FIREBASE_AUTH ""
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// SIM900A Configuration
#define RX_PIN 16  // GPIO 16 for RX (SIM900A TX)
#define TX_PIN 17  // GPIO 17 for TX (SIM900A RX)
HardwareSerial mySerial(1);  // Use Serial1 for SIM900A communication

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// Threshold values
float temperatureMin = 25.0, temperatureMax = 31.0;
float pHMin = 6.5, pHMax = 9.0;
float dissolvedOxygenMin = 5.0, dissolvedOxygenMax = 1000.0;
float turbidityMax = 50.0;
float chlorophyllMax = 30.0;

// Variables to track the last timestamps
String lastCurrentDataTimestamp = "";
String lastCurrentPredictionTimestamp = "";

// SMS function
void sendSMS(String phoneNumber, String message) {
  mySerial.println("AT");
  delay(1000);
  mySerial.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  mySerial.print("AT+CMGS=\"");
  mySerial.print(phoneNumber);
  mySerial.println("\"");
  delay(1000);
  mySerial.println(message);
  mySerial.write(26);  // End-of-message character (Ctrl+Z)
  delay(3000);
}

// Function to check thresholds and send SMS
void checkThresholds(String key, float value, float min, float max, String unit) {
  if (value < min || value > max) {
    String alertMessage = key + " value " + String(value, 1) + " " + unit +
                          (value < min ? " is below the minimum threshold of " : " exceeds the maximum threshold of ") +
                          String(value < min ? min : max) + " " + unit + ".";
    sendSMS("+639770710102", alertMessage);  // Replace with recipient's phone number
  }
}

void setup() {
  Serial.begin(115200);

  // Initialize SIM900A
  mySerial.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(3000);  // Allow time for SIM900A to initialize

  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi");

  // Configure Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Ready for Firebase monitoring...");
}

void loop() {
  // Monitor CurrentData for changes
  if (Firebase.getJSON(firebaseData, "/CurrentData")) {
    FirebaseJson jsonData = firebaseData.jsonObject();
    FirebaseJsonData jsonDataValue;

    // Check timestamp
    if (jsonData.get(jsonDataValue, "timestamp")) {
      String currentDataTimestamp = jsonDataValue.stringValue;
      if (currentDataTimestamp != lastCurrentDataTimestamp) {
        lastCurrentDataTimestamp = currentDataTimestamp;

        // Check thresholds for parameters
        if (jsonData.get(jsonDataValue, "Temperature")) {
          float temperatureValue = atof(jsonDataValue.stringValue.c_str());
          checkThresholds("Temperature", temperatureValue, temperatureMin, temperatureMax, "Degree Celsius");
        }

        if (jsonData.get(jsonDataValue, "pH")) {
          float pHValue = atof(jsonDataValue.stringValue.c_str());
          checkThresholds("pH", pHValue, pHMin, pHMax, "pH");
        }

        if (jsonData.get(jsonDataValue, "DissolvedOxygen")) {
          float doValue = atof(jsonDataValue.stringValue.c_str());
          checkThresholds("Dissolved Oxygen", doValue, dissolvedOxygenMin, dissolvedOxygenMax, "mg/L");
        }

        if (jsonData.get(jsonDataValue, "Turbidity")) {
          float turbidityValue = atof(jsonDataValue.stringValue.c_str());
          checkThresholds("Turbidity", turbidityValue, 0, turbidityMax, "FNU");
        }
      }
    }
  } else {
    Serial.println("Failed to read CurrentData: " + firebaseData.errorReason());
  }

  // Monitor CurrentPrediction for changes
  if (Firebase.getJSON(firebaseData, "/CurrentPrediction")) {
    FirebaseJson jsonPrediction = firebaseData.jsonObject();
    FirebaseJsonData jsonPredictionValue;

    // Check timestamp
    if (jsonPrediction.get(jsonPredictionValue, "timestamp")) {
      String currentPredictionTimestamp = jsonPredictionValue.stringValue;
      if (currentPredictionTimestamp != lastCurrentPredictionTimestamp) {
        lastCurrentPredictionTimestamp = currentPredictionTimestamp;

        // Check thresholds for chlorophyll
        if (jsonPrediction.get(jsonPredictionValue, "Predicted_Chlorophyll")) {
          float chlorophyllValue = atof(jsonPredictionValue.stringValue.c_str());
          checkThresholds("Chlorophyll", chlorophyllValue, 0, chlorophyllMax, "ug/L");
        }
      }
    }
  } else {
    Serial.println("Failed to read CurrentPrediction: " + firebaseData.errorReason());
  }

  delay(1800000);
} 
