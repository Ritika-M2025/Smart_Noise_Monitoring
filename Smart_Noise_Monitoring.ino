#define BLYNK_TEMPLATE_ID   "Template ID"
#define BLYNK_TEMPLATE_NAME "Smart Noise Pollution Monitor"
#define BLYNK_AUTH_TOKEN    "Token"

#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// Wi-Fi credentials
char ssid[] = "user_name";
char pass[] = "wifipass";

// Hardware connections
const int SOUND_PIN  = 34;
const int BUZZER_PIN = 26;
const int RELAY_PIN  = 27;

// Change to false if your relay is active HIGH.
const bool RELAY_ACTIVE_LOW = true;

// Experimental relative sound thresholds.
// These are NOT calibrated decibel levels.
const int NOISE_THRESHOLD = 1800;
const int RESET_THRESHOLD = 1500;

const int REQUIRED_HIGH_SAMPLES = 2;

BlynkTimer timer;

bool alarmActive = false;
int consecutiveHighSamples = 0;

int latestSoundLevel = 0;
bool sensorReadingValid = false;

// --------------------------------------------------
// Relay control
// --------------------------------------------------
void controlRelay(bool on) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(RELAY_PIN, on ? LOW : HIGH);
  } else {
    digitalWrite(RELAY_PIN, on ? HIGH : LOW);
  }
}

// --------------------------------------------------
// Read analog sound sensor
// Returns peak-to-peak variation over 100 ms.
// Returns -1 if ADC reaches a measurement limit.
// --------------------------------------------------
int readSoundLevel() {
  int minimum = 4095;
  int maximum = 0;

  unsigned long startTime = millis();

  while (millis() - startTime < 100) {
    int value = analogRead(SOUND_PIN);

    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;

    delayMicroseconds(200);
  }

  Serial.print("Minimum: ");
  Serial.print(minimum);
  Serial.print(" | Maximum: ");
  Serial.print(maximum);

  if (minimum == 0 || maximum >= 4095) {
    Serial.println(" | ADC LIMIT: Check sensor signal and wiring");
    return -1;
  }

  int level = maximum - minimum;

  Serial.print(" | Sound level: ");
  Serial.println(level);

  return level;
}

// --------------------------------------------------
// Publish dashboard values
// --------------------------------------------------
void publishStatus() {
  if (!Blynk.connected()) return;

  if (sensorReadingValid) {
    Blynk.virtualWrite(V0, latestSoundLevel);
  }

  Blynk.virtualWrite(V1, alarmActive ? 1 : 0);
  Blynk.virtualWrite(V2, alarmActive ? "NOISE ALERT" : "NORMAL");
}

// --------------------------------------------------
// Change alarm state
// Notification is requested only on OFF -> ON.
// --------------------------------------------------
void setAlarm(bool on) {
  if (alarmActive == on) return;

  alarmActive = on;

  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
  controlRelay(on);

  publishStatus();

  if (on) {
    Serial.println("***** NOISE ALERT *****");

    if (Blynk.connected()) {
      Blynk.logEvent(
        "noise_alert",
        "Noise threshold exceeded. Please check the area."
      );

      Serial.println("Blynk noise notification event requested.");
    } else {
      Serial.println("Blynk disconnected: notification not requested.");
    }
  } else {
    Serial.println("Noise returned below reset threshold.");
  }
}

// --------------------------------------------------
// Monitor sound
// --------------------------------------------------
void monitorNoise() {
  int level = readSoundLevel();

  // Ignore ADC-limit readings rather than treating them
  // as valid high-noise measurements.
  if (level < 0) {
    sensorReadingValid = false;
    consecutiveHighSamples = 0;
    return;
  }

  sensorReadingValid = true;
  latestSoundLevel = level;

  if (Blynk.connected()) {
    Blynk.virtualWrite(V0, level);
  }

  if (level >= NOISE_THRESHOLD) {
    consecutiveHighSamples++;

    if (!alarmActive &&
        consecutiveHighSamples >= REQUIRED_HIGH_SAMPLES) {
      setAlarm(true);
    }
  } else {
    consecutiveHighSamples = 0;
  }

  if (alarmActive && level < RESET_THRESHOLD) {
    setAlarm(false);
  }

  Serial.print("Alarm status: ");
  Serial.println(alarmActive ? "ON" : "OFF");
}

// --------------------------------------------------
// Restore dashboard values after Blynk reconnects
// --------------------------------------------------
BLYNK_CONNECTED() {
  Serial.println("Blynk connected!");

  publishStatus();
}

// --------------------------------------------------
// Setup
// --------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("SMART NOISE POLLUTION MONITOR");

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(BUZZER_PIN, LOW);
  controlRelay(false);

  analogReadResolution(12);
  analogSetPinAttenuation(SOUND_PIN, ADC_11db);

  Serial.println("Connecting to Wi-Fi and Blynk...");

  Blynk.begin(
    BLYNK_AUTH_TOKEN,
    ssid,
    pass
  );

  Serial.println("Blynk connection established.");

  timer.setInterval(500L, monitorNoise);
  timer.setInterval(2000L, publishStatus);
}

// --------------------------------------------------
// Main loop
// --------------------------------------------------
void loop() {
  Blynk.run();
  timer.run();
}
