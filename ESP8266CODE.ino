#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <FirebaseESP8266.h>


#define FIREBASE_HOST ""  // Replace with your Firebase Database URL
#define FIREBASE_AUTH ""           // Replace with your Firebase Database Secret

// Wi-Fi credentials
#define WIFI_SSID ""         // Replace with your Wi-Fi SSID
#define WIFI_PASSWORD "" // Replace with your Wi-Fi password
#define DO_PIN A0

#define VREF 3300    // VREF (mv)
#define ADC_RES 1024 // ADC Resolution


// Single-point calibration Mode=0
// Two-point calibration Mode=1
#define TWO_POINT_CALIBRATION 1 // Enable two-point calibration

// Single point calibration needs to be filled CAL1_V and CAL1_T
#define CAL1_V (609) // mv
#define CAL1_T (38)   // ℃
// Two-point calibration needs to be filled CAL2_V and CAL2_T
#define CAL2_V (341) // mv
#define CAL2_T (11)   // ℃

// DS18B20 setup
#define ONE_WIRE_BUS D4 // Pin for DS18B20
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

FirebaseData firebaseData;
FirebaseAuth auth;
FirebaseConfig config;

// DO table for calibration
const uint16_t DO_Table[41] = {
    14460, 14220, 13820, 13440, 13090, 12740, 12420, 12110, 11810, 11530,
    11260, 11010, 10770, 10530, 10300, 10080, 9860, 9660, 9460, 9270,
    9080, 8900, 8730, 8570, 8410, 8250, 8110, 7960, 7820, 7690,
    7560, 7430, 7300, 7180, 7070, 6950, 6840, 6730, 6630, 6530, 6410};

uint16_t ADC_Raw;
uint16_t ADC_Voltage;

float readDO(uint32_t voltage_mv, uint8_t temperature_c) {
#if TWO_POINT_CALIBRATION == 0
    uint16_t V_saturation = (uint32_t)CAL1_V + (uint32_t)35 * temperature_c - (uint32_t)CAL1_T * 35;
    return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#else
    uint16_t V_saturation = (int16_t)((int8_t)temperature_c - CAL2_T) * ((uint16_t)CAL1_V - CAL2_V) / ((uint8_t)CAL1_T - CAL2_T) + CAL2_V;
    return (voltage_mv * DO_Table[temperature_c] / V_saturation);
#endif
}
unsigned long previousMillis = 0; // Store the last time a reading was taken
const long readInterval = 1000; // Interval to read temperature every second
const long outputInterval = 30000; // Output interval for every 30 seconds

unsigned long lastReadMillis = 0; // Store the last time a reading was made
float lastCelsius = 0; // Store the last Celsius temperature
float lastFahrenheit = 0;

float lastDO_mg_per_L=0;
float lastADC_Raw = 0;
float lastADC_Voltage = 0;
float pH = 0;
float Turbidity = 0;
void setup() {
    Serial.begin(115200);
    Serial.println("Starting...");

    // Initialize the DS18B20 sensor
    sensors.begin();
    // Connect to Wi-Fi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    Serial.println("Connected to Wi-Fi");

    // Configure Firebase
    config.host = FIREBASE_HOST;
    config.signer.tokens.legacy_token = FIREBASE_AUTH;

    // Initialize Firebase
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
}

void loop() {
    unsigned long currentMillis = millis();

    // Check if it's time to read the temperature (every second)
    if (currentMillis - lastReadMillis >= readInterval) {
        lastReadMillis = currentMillis;
        // Request temperature readings from the DS18B20 sensor
        sensors.requestTemperatures();
        
        // Get the temperature in Celsius
        lastCelsius = sensors.getTempCByIndex(0);
        lastFahrenheit = sensors.toFahrenheit(lastCelsius);
        uint8_t Temperaturet = (uint8_t)lastCelsius; // Cast to uint8_t for DO calculation

        // Read raw voltage from the DO sensor
        lastADC_Raw = analogRead(DO_PIN);
        lastADC_Voltage = uint32_t(VREF) * lastADC_Raw / ADC_RES;

        // Calculate DO and format to 2 decimal places
        lastDO_mg_per_L = readDO(lastADC_Voltage, Temperaturet) / 1000.0; // Convert to mg/L
        if (Serial.available() > 0) {
        String data = Serial.readStringUntil('\n');

        // Parse pH and turbidity values from string
        int commaIndex = data.indexOf(',');
        if (commaIndex > 0) {
          String phString = data.substring(0, commaIndex);
          String turbidityString = data.substring(commaIndex + 1);
          
          pH = phString.toFloat();
          Turbidity = turbidityString.toFloat(); 
        }
      }
    }

    // Check if it's time to output the readings (every 30 seconds)
    if (currentMillis - previousMillis >= outputInterval) {
        previousMillis = currentMillis;
        Serial.print("Temperature in Celsius: " + String(lastCelsius) + "\t");
        Serial.print("Temperature in Fahrenheit: " + String(lastFahrenheit) + "\t");
        Serial.print("ADC RAW: " + String(lastADC_Raw) + "\t");
        Serial.print("ADC Voltage: " + String(lastADC_Voltage) + "\t");
        Serial.print("pH: " + String(pH) + "\t");
        Serial.print("Turbidity: " + String(Turbidity) + "\t");
        Serial.println("DO: " + String(lastDO_mg_per_L, 2) + "\t"); // Format to 2 decimal places
        // Create a unified JSON object for all parameters
        FirebaseJson dataJson;
        dataJson.set("timestamp/.sv", "timestamp"); // Common timestamp

        // Add each parameter to the unified JSON object
        dataJson.set("Temperature", lastCelsius);
        dataJson.set("Temperaturefar", lastFahrenheit);
        dataJson.set("DissolvedOxygen", lastDO_mg_per_L);
        dataJson.set("DOadc", lastADC_Raw);
        dataJson.set("DOvolt", lastADC_Voltage);
        dataJson.set("pH", pH);
        dataJson.set("Turbidity", Turbidity);

        // Send the unified data JSON to Firebase
        if (Firebase.pushJSON(firebaseData, "/SensorData", dataJson)) {
            Serial.println("SensorData sent to Firebase successfully");
        } else {
            Serial.print("Failed to send data to Firebase: ");
            Serial.println(firebaseData.errorReason());
        }

        // Optionally update the current values under individual nodes
        if (Firebase.setJSON(firebaseData, "/CurrentData", dataJson)) {
            Serial.println("CurrentData updated in Firebase successfully");
        } else {
            Serial.print("Failed to update CurrentData in Firebase: ");
            Serial.println(firebaseData.errorReason());
        }
    }
}
