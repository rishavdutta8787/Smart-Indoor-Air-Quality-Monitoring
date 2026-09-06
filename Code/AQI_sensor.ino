/*
 * Indoor Air-Quality & Environmental Data Logger
 * ESP8266 NodeMCU + DHT11 + MQ-135 + LDR module
 * Sends data to Blynk (real-time) AND Google Sheets (long-term storage)
 */

// ======================================================
// BLYNK CREDENTIALS
// ======================================================
#define BLYNK_TEMPLATE_ID   "TMPL3syiQHI1Z"
#define BLYNK_TEMPLATE_NAME "AirQualityLogger"
#define BLYNK_AUTH_TOKEN    "qejLJUjqsfT0W8hMFTdM0M-UHbPWC44n"

// ======================================================
// LIBRARIES
// ======================================================
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <BlynkSimpleEsp8266.h>
#include <DHT.h>

// ======================================================
// WIFI CREDENTIALS
// ======================================================
char ssid[] = "ECEDEPT";
char pass[] = "gcug@123";

// ======================================================
// GOOGLE APPS SCRIPT WEB APP URL
// Replace YOUR_DEPLOYMENT_ID with your actual deployment ID
// ======================================================
String googleScriptURL =
  "https://script.google.com/macros/s/AKfycbzUrls0RMKWILuqbtTW8h39_rJCWEPTcmwSQWmxZ4tb3Nd8o1BN9_qqBn7tHoYxbgPJ8A/exec";

// ======================================================
// PIN SETUP
// ======================================================

// DHT11
#define DHTPIN D4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// MQ-135 Analog Output
#define MQ135PIN A0

// LDR Module Digital Output
#define LDRPIN D5

// ======================================================
// TIMING
// ======================================================

BlynkTimer timer;

// Send/read data every 5 seconds
unsigned long sampleIntervalMs = 10000;

// ======================================================
// THRESHOLDS
// Tune these values after collecting real sensor data
// ======================================================

int gasAnomalyThreshold = 600;

// ======================================================
// GOOGLE SHEETS LOGGING FUNCTION
// ======================================================

void logToGoogleSheets(
  float t,
  float h,
  int mq,
  int light,
  String cls,
  int anomaly,
  String occupancy
) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Skipping Google Sheets.");
    return;
  }

  WiFiClientSecure client;

  // Disable certificate verification
  client.setInsecure();

  // Increase timeout for Google servers
  client.setTimeout(15000);

  HTTPClient https;

  // URL encode spaces in text values
  cls.replace(" ", "%20");
  occupancy.replace(" ", "%20");

  // Construct URL
  String url = googleScriptURL +
               "?t=" + String(t, 2) +
               "&h=" + String(h, 2) +
               "&mq=" + String(mq) +
               "&light=" + String(light) +
               "&cls=" + cls +
               "&anomaly=" + String(anomaly) +
               "&occupancy=" + occupancy;

  Serial.println();
  Serial.println("Sending data to Google Sheets...");
  Serial.print("Light: ");
  Serial.println(light);
  Serial.print("Occupancy: ");
  Serial.println(occupancy);

  // Follow Google redirects
  https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  // Increase HTTP timeout
  https.setTimeout(15000);

  if (https.begin(client, url)) {

    int httpCode = https.GET();

    Serial.print("Google Sheets HTTP status: ");
    Serial.println(httpCode);

    if (httpCode > 0) {

      String payload = https.getString();

      Serial.print("Google Sheets response: ");
      Serial.println(payload);

    } else {

      Serial.print("Google Sheets request failed: ");
      Serial.println(https.errorToString(httpCode));

    }

    https.end();

  } else {

    Serial.println("Unable to establish HTTPS connection.");

  }

  // Give ESP8266 WiFi stack a moment
  yield();
}
// ======================================================
// SENSOR READING + DATA SENDING FUNCTION
// ======================================================

void sendSensorData() {

  // --------------------------------------------------
  // Read DHT11
  // --------------------------------------------------

  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {

    Serial.println("DHT11 read failed. Skipping this cycle.");

    return;
  }

  // --------------------------------------------------
  // Read MQ-135
  // --------------------------------------------------

  int mq135Raw = analogRead(MQ135PIN);

  // --------------------------------------------------
  // Read LDR Digital Output
  // --------------------------------------------------

  int lightState = digitalRead(LDRPIN);

  // --------------------------------------------------
  // Air Quality Classification
  // IMPORTANT:
  // These are experimental RAW ADC thresholds.
  // They do NOT represent calibrated PPM or AQI.
  // --------------------------------------------------

  String airQualityClass;

  if (mq135Raw < 300) {

    airQualityClass = "Good";

  } else if (mq135Raw < 600) {

    airQualityClass = "Moderate";

  } else {

    airQualityClass = "Poor";

  }

  // --------------------------------------------------
  // Gas Anomaly Detection
  // --------------------------------------------------

  int gasAnomaly;

  if (mq135Raw > gasAnomalyThreshold) {

    gasAnomaly = 1;

  } else {

    gasAnomaly = 0;

  }

  // --------------------------------------------------
  // Simple Occupancy Estimation
  //
  // This is ONLY an estimated state based on light.
  // LDR cannot reliably detect actual human occupancy.
  //
  // Change HIGH/LOW if your LDR module behaves opposite.
  // --------------------------------------------------

  String OccupancyStatus;

  if (lightState == HIGH) {

    OccupancyStatus = "Unoccupied";

  } else {

    OccupancyStatus = "PossiblyOccupied";

  }

  // ==================================================
  // SERIAL MONITOR OUTPUT
  // ==================================================

  Serial.println("--------------------------------");

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" C");

  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("MQ135 Raw: ");
  Serial.println(mq135Raw);

  Serial.print("Light State: ");
  Serial.println(lightState);

  Serial.print("Air Quality: ");
  Serial.println(airQualityClass);

  Serial.print("Gas Anomaly: ");
  Serial.println(gasAnomaly);

  Serial.print("Occupancy Status: ");
  Serial.println(OccupancyStatus);

  Serial.println("--------------------------------");

  // ==================================================
  // SEND DATA TO BLYNK
  // ==================================================

  if (Blynk.connected()) {

    Blynk.virtualWrite(V0, temperature);
    Blynk.virtualWrite(V1, humidity);
    Blynk.virtualWrite(V2, mq135Raw);
    Blynk.virtualWrite(V3, lightState);
    Blynk.virtualWrite(V4, airQualityClass);
    Blynk.virtualWrite(V5, gasAnomaly);
    Blynk.virtualWrite(V6, OccupancyStatus);

  } else {

    Serial.println("Blynk not connected.");

  }

  // ==================================================
  // SEND DATA TO GOOGLE SHEETS
  // ==================================================

  logToGoogleSheets(
    temperature,
    humidity,
    mq135Raw,
    lightState,
    airQualityClass,
    gasAnomaly,
    OccupancyStatus
  );
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("Indoor Air Quality Data Logger");
  Serial.println("Starting system...");

  // Start DHT11
  dht.begin();

  // Configure LDR pin
  pinMode(LDRPIN, INPUT);

  // ==================================================
  // CONNECT TO WIFI
  // ==================================================

WiFi.mode(WIFI_STA);
WiFi.disconnect();
delay(1000);

Serial.println("Starting WiFi connection...");
Serial.print("SSID: ");
Serial.println(ssid);

WiFi.begin(ssid, pass);

int attempts = 0;

while (WiFi.status() != WL_CONNECTED && attempts < 30) {
  delay(500);

  Serial.print("Attempt ");
  Serial.print(attempts);
  Serial.print(" | WiFi status: ");
  Serial.println(WiFi.status());

  attempts++;
}

if (WiFi.status() == WL_CONNECTED) {

  Serial.println();
  Serial.println("WiFi connected!");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

} else {

  Serial.println();
  Serial.println("WiFi connection FAILED!");

  Serial.print("Final WiFi status code: ");
  Serial.println(WiFi.status());
}

  // ==================================================
  // CONNECT TO BLYNK
  // ==================================================

  Blynk.config(BLYNK_AUTH_TOKEN);

  Serial.println("Connecting to Blynk...");

  if (Blynk.connect(10000)) {

    Serial.println("Blynk connected!");

  } else {

    Serial.println("Blynk connection failed.");
  }

  // ==================================================
  // START SENSOR TIMER
  // ==================================================

  timer.setInterval(sampleIntervalMs, sendSensorData);

  Serial.println("System ready.");
}

// ======================================================
// MAIN LOOP
// ======================================================

void loop() {

  // Run Blynk only when connected
  if (Blynk.connected()) {

    Blynk.run();

  } else {

    // Attempt reconnection
    Blynk.connect(1000);
  }

  // Run sensor timer
  timer.run();
}