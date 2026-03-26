/******************************************************************************************
 *  Projekt: ESP32 Stromsäule mit MQTT & WS2812 LEDs
 *  Version: 5.1
 *  Autor:   B.P
 *  Datum:   07.10.2025
 *
 *  Funktionen:
 *    - WLAN + MQTT Verbindung (optional TLS)
 *    - Counter-Eingänge (4x) mit Software-Polling & Entprellung (statt Interrupts)
 *    - GPIO-Ausgänge (4x) mit optionaler Dauersteuerung (Duration)
 *    - WS2812 LED-Gruppen mit RGB, Blink & Duration
 *    - NTP Zeitabgleich und regelmäßige Localtime Veröffentlichung
 *    - Watchdog Schutz & Retained MQTT States
 *    - Initial-Publish von GPIOs, Counter und Status nach Boot
 *    - MQTT-MAC-Adresse Handling mit Möglichkeit zum „Türken“ (Spoofing)
 *    - Polling-Zyklus mit Delay(5) zur CPU-Entlastung
 *
 *  Befehle über MQTT:
 *    - SET_OUTPUT: gpio=<pin>;state=<0|1>;[duration=<ms>]
 *    - SET_LED: group=<0-4>;red=<0-255>;green=<0-255>;blue=<0-255>;duration=<ms>;blink=<0|1>
 *    - Reboot / Watchdog-Test / State-ACK
 *
 ******************************************************************************************/


#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <time.h>
#include "esp_task_wdt.h"   // 🐶 Watchdog
#include "esp_system.h"     // für esp_random()

// Optional: eigene "getürkte" MAC (Format "AA:BB:CC:DD:EE:FF")
// Leer lassen = echte MAC verwenden
String spoofedMac = ""; // Beispiel: "DE:AD:BE:EF:00:01"

// -------------------------
// Hilfsfunktion zum Formatieren der MAC für Topics
// -------------------------
String formatMacForTopic(String mac) {
  mac.toUpperCase();
  mac.replace(":", "-");
  return mac;
}

// -------------------------
// TLS optional
// -------------------------
#define USE_TLS   // einkommentieren = TLS an, auskommentieren = TLS aus
#define SOFTWARE_VERSION "BP_v.5.1_ST"

#ifdef USE_TLS
  #include <WiFiClientSecure.h>
  WiFiClientSecure espClient;
#else
  WiFiClient espClient;
#endif

PubSubClient mqttClient(espClient);

// -------------------------
// WLAN / MQTT Daten / Zeit
// -------------------------
const char* ssid     = "MDSI";
const char* password = "jF8Oq!wCDgcEp_%20";

//const char* ssid = "FRITZ!Box 7520 QM";
//const char* password = "28389585079405190635";

const char* mqtt_server = "dev-mqtt.pramux.de";
#ifdef USE_TLS
const int mqtt_port = 8883;
#else
const int mqtt_port = 1885;
#endif
const char* mqtt_user = "pramux";
const char* mqtt_pass = ";Wp{=(*_2a*H.p83/!b(CB8t";

// ⏰ Intervall für Localtime-Update (ms)
const unsigned long localtimeInterval = 60000;   

// -------------------------
// Topic-Prefix
// -------------------------
const char* topicPrefix = "Stromsäule";

// -------------------------
// Globals
// -------------------------
String baseTopic; 

// GPIOs (low-side switches)
const int gpio12 = 12;
const int gpio13 = 13;
const int gpio25 = 25;
const int gpio33 = 33;

// GPIO "last known state" (-1 = unbekannt)
int lastSentGpio12 = -1;
int lastSentGpio13 = -1;
int lastSentGpio25 = -1;
int lastSentGpio33 = -1;

// Struktur für GPIO-Zeitsteuerung
struct GpioState {
  int pin;
  bool active;
  unsigned long offTime;
};

GpioState gpioStates[4] = {
  { gpio12, false, 0 },
  { gpio13, false, 0 },
  { gpio25, false, 0 },
  { gpio33, false, 0 }
};

// Counter GPIOs (Eingänge)
const int counter0Pin = 36;
const int counter1Pin = 39;
const int counter2Pin = 34;
const int counter3Pin = 35;

// Counter-Werte
uint32_t counter0 = 0;
uint32_t counter1 = 0;
uint32_t counter2 = 0;
uint32_t counter3 = 0;

// Non-blocking MQTT-Reconnect
unsigned long lastMqttReconnectAttempt = 0;

/// Debounce Marker für Counter
unsigned long lastCounter0 = 0;
unsigned long lastCounter1 = 0;
unsigned long lastCounter2 = 0;
unsigned long lastCounter3 = 0;

// Publish-Marker (zuletzt gesendeter Wert pro Counter)
int lastSentCounter0 = 0;
int lastSentCounter1 = 0;
int lastSentCounter2 = 0;
int lastSentCounter3 = 0;



bool watchdogResetDetected = false;   // Watchdog-Flag

// Flag & Timer für verzögerten Initial-Publish
bool initialPublishDone = false;
unsigned long bootTime = 0;

#define LED_PIN 15
#define NUM_LEDS_GROUP0 3
#define NUM_LEDS_GROUP1 3
#define NUM_LEDS_GROUP2 3
#define NUM_LEDS_GROUP3 3
#define NUM_LEDS_GROUP4 3

#define NUM_LEDS_TOTAL (NUM_LEDS_GROUP0 + NUM_LEDS_GROUP1 + NUM_LEDS_GROUP2 + NUM_LEDS_GROUP3 + NUM_LEDS_GROUP4)

CRGB leds[NUM_LEDS_TOTAL];

struct LedGroup {
  CRGB* leds;
  int numLeds;
};

LedGroup ledGroups[5] = {
  { leds, NUM_LEDS_GROUP0 }, 
  { leds + NUM_LEDS_GROUP0, NUM_LEDS_GROUP1 },
  { leds + NUM_LEDS_GROUP0 + NUM_LEDS_GROUP1, NUM_LEDS_GROUP2 },
  { leds + NUM_LEDS_GROUP0 + NUM_LEDS_GROUP1 + NUM_LEDS_GROUP2, NUM_LEDS_GROUP3 },
  { leds + NUM_LEDS_GROUP0 + NUM_LEDS_GROUP1 + NUM_LEDS_GROUP2 + NUM_LEDS_GROUP3, NUM_LEDS_GROUP4 }
};

// LED-State
struct LedState {
  CRGB color;
  unsigned long duration;
  bool blink;
  unsigned long startTime;
  bool active;
  bool isOn;
};
LedState ledStates[5];

// -------------------------
// Counter mit Polling statt Interrupts
// -------------------------

constexpr unsigned long DEBOUNCE_INTERVAL = 50000;  // µs (50 ms)
unsigned long lastPollTime = 0;

// Letzter bekannter Zustand der Pins
int lastCounter0State = HIGH;
int lastCounter1State = HIGH;
int lastCounter2State = HIGH;
int lastCounter3State = HIGH;

// Letzte Zählzeit für Entprellung
unsigned long lastCounter0Time = 0;
unsigned long lastCounter1Time = 0;
unsigned long lastCounter2Time = 0;
unsigned long lastCounter3Time = 0;

void pollCounters() {
    unsigned long now = micros();

    // Polling-Intervall: nur alle 5 ms (1000 µs)
    if (now - lastPollTime < 5000) return;
    lastPollTime = now;

    int state0 = digitalRead(counter0Pin);
    if (state0 == LOW && lastCounter0State == HIGH && (now - lastCounter0Time > DEBOUNCE_INTERVAL)) {
        counter0++;
        lastCounter0Time = now;
    }
    lastCounter0State = state0;

    int state1 = digitalRead(counter1Pin);
    if (state1 == LOW && lastCounter1State == HIGH && (now - lastCounter1Time > DEBOUNCE_INTERVAL)) {
        counter1++;
        lastCounter1Time = now;
    }
    lastCounter1State = state1;

    int state2 = digitalRead(counter2Pin);
    if (state2 == LOW && lastCounter2State == HIGH && (now - lastCounter2Time > DEBOUNCE_INTERVAL)) {
        counter2++;
        lastCounter2Time = now;
    }
    lastCounter2State = state2;

    int state3 = digitalRead(counter3Pin);
    if (state3 == LOW && lastCounter3State == HIGH && (now - lastCounter3Time > DEBOUNCE_INTERVAL)) {
        counter3++;
        lastCounter3Time = now;
    }
    lastCounter3State = state3;
}




// -------------------------
// Hilfsfunktionen
// -------------------------
String getEffectiveMacForTopic() {
  if (spoofedMac.length() > 0) {
    String tmp = spoofedMac;
    tmp.toUpperCase();
    tmp.replace(":", "-");
    return tmp;
  }
  return formatMacForTopic(WiFi.macAddress());
}

String getEffectiveMacRaw() {
  if (spoofedMac.length() > 0) {
    String tmp = spoofedMac;
    tmp.toUpperCase();
    return tmp;
  }
  return WiFi.macAddress();
}

// -------------------------
// Hilfsfunktion: Key=Value aus Payload
// -------------------------
String getValueFromPayload(String payload, String key) {
  int pos = payload.indexOf(key + "=");
  if (pos < 0) return "";
  int start = pos + key.length() + 1;
  int end = payload.indexOf(';', start);
  if (end < 0) end = payload.length();
  return payload.substring(start, end);
}


// Angepasst: Invertierte Logik
String gpioStateLogical(int pin) {
  return String(digitalRead(pin) == HIGH ? 1 : 0);
}

void setLedGroup(int group, CRGB color, unsigned long duration, bool blink) {
  if (group < 0 || group > 4) return;   
  fill_solid(ledGroups[group].leds, ledGroups[group].numLeds, color);
  FastLED.show();

  ledStates[group].color = color;
  ledStates[group].duration = duration;
  ledStates[group].blink = blink;
  ledStates[group].startTime = millis();
  ledStates[group].active = true;
  ledStates[group].isOn = true;
}

void updateLeds() {
  unsigned long now = millis();
  for (int g = 0; g < 5; g++) {   // vorher 2
    if (!ledStates[g].active) continue;

    if (ledStates[g].duration > 0 && now - ledStates[g].startTime > ledStates[g].duration) {
      fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB::Black);
      FastLED.show();
      ledStates[g].active = false;
      continue;
    }

    if (ledStates[g].blink) {
      if ((now / 500) % 2 == 0) {
        if (!ledStates[g].isOn) {
          fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, ledStates[g].color);
          FastLED.show();
          ledStates[g].isOn = true;
        }
      } else {
        if (ledStates[g].isOn) {
          fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB::Black);
          FastLED.show();
          ledStates[g].isOn = false;
        }
      }
    }
  }
}
// -------------------------
// MQTT Handler
// -------------------------
void handleMQTTPayload(String topic, String payload) {
  Serial.println("MQTT IN: " + topic + " -> " + payload);

  // Counter-Übernahme beim Boot
  if (topic == baseTopic + "/channel/00/counter/00") {
    if (payload.startsWith("value=")) counter0 = payload.substring(6).toInt();
    lastSentCounter0 = counter0;   // sync Merker
  }
  if (topic == baseTopic + "/channel/00/counter/01") {
  if (payload.startsWith("value=")) counter1 = payload.substring(6).toInt();
  lastSentCounter1 = counter1;   // ✅ richtig
}
if (topic == baseTopic + "/channel/00/counter/02") {
  if (payload.startsWith("value=")) counter2 = payload.substring(6).toInt();
  lastSentCounter2 = counter2;
}
if (topic == baseTopic + "/channel/00/counter/03") {
  if (payload.startsWith("value=")) counter3 = payload.substring(6).toInt();
  lastSentCounter3 = counter3;
}
 // GPIO-Übernahme beim Boot (letzter bekannter Wert)
  if (topic == baseTopic + "/channel/00/gpios/12") {
    if (payload.startsWith("state=")) {
      int state = payload.substring(6).toInt();
      lastSentGpio12 = state;
      digitalWrite(gpio12, state ? HIGH : LOW);
    }
  }
  if (topic == baseTopic + "/channel/00/gpios/13") {
    if (payload.startsWith("state=")) {
      int state = payload.substring(6).toInt();
      lastSentGpio13 = state;
      digitalWrite(gpio13, state ? HIGH : LOW);
    }
  }
  if (topic == baseTopic + "/channel/00/gpios/25") {
    if (payload.startsWith("state=")) {
      int state = payload.substring(6).toInt();
      lastSentGpio25 = state;
      digitalWrite(gpio25, state ? HIGH : LOW);
    }
  }
  if (topic == baseTopic + "/channel/00/gpios/33") {
    if (payload.startsWith("state=")) {
      int state = payload.substring(6).toInt();
      lastSentGpio33 = state;
      digitalWrite(gpio33, state ? HIGH : LOW);
    }
  }

  if (topic == baseTopic + "/config/in") {
    if (payload == "reboot") {
      mqttClient.publish((baseTopic + "/config/out").c_str(), "Rebooting...", true);
      delay(500);
      ESP.restart();
      return;
    }
    if (payload.startsWith("cmd=SET_STATE;value=")) {
      String val = payload.substring(String("cmd=SET_STATE;value=").length());
      mqttClient.publish((baseTopic + "/config/state").c_str(), val.c_str(), true);
      mqttClient.publish((baseTopic + "/config/out").c_str(), ("ACK state=" + val).c_str(), true);
      return;
    }
    // 🐶 Watchdog-Test per MQTT
    if (payload == "crash") {
      Serial.println("⚠️ Test: absichtlicher Watchdog-Fehler!");
      while (true) {
        // kein esp_task_wdt_reset() → nach 30s Neustart
        delay(1000);
      }
    }
  }

  if (topic == baseTopic + "/channel/00/in") {
   if (payload.indexOf("cmd=SET_OUTPUT") >= 0) {
  int gpio = getValueFromPayload(payload, "gpio").toInt();
  int state = getValueFromPayload(payload, "state").toInt();
  unsigned long duration = getValueFromPayload(payload, "duration").toInt();

  if (gpio == gpio12 || gpio == gpio13 || gpio == gpio25 || gpio == gpio33) {
    digitalWrite(gpio, state ? HIGH : LOW);

    if (gpio == gpio12) lastSentGpio12 = state;
    if (gpio == gpio13) lastSentGpio13 = state;
    if (gpio == gpio25) lastSentGpio25 = state;
    if (gpio == gpio33) lastSentGpio33 = state;

    if (duration > 0 && state == 1) {
      for (int i = 0; i < 4; i++) {
        if (gpioStates[i].pin == gpio) {
          gpioStates[i].active = true;
          gpioStates[i].offTime = millis() + duration;
        }
      }
    }

    String ack = "GPIO" + String(gpio) + " -> state=" + String(state);
    mqttClient.publish((baseTopic + "/channel/00/out").c_str(), ack.c_str(), true);

    String gpioMsg = "state=" + String(state);
    mqttClient.publish((baseTopic + "/channel/00/gpios/" + String(gpio)).c_str(), gpioMsg.c_str(), true);

    mqttClient.publish((baseTopic + "/channel/00/device").c_str(), "active", true);
  }
}


if (payload.indexOf("cmd=SET_LED") >= 0) {
  int group    = getValueFromPayload(payload, "group").toInt();
  int red      = getValueFromPayload(payload, "red").toInt();
  int green    = getValueFromPayload(payload, "green").toInt();
  int blue     = getValueFromPayload(payload, "blue").toInt();
  unsigned long duration = getValueFromPayload(payload, "duration").toInt();
  int blink    = getValueFromPayload(payload, "blink").toInt();

  setLedGroup(group, CRGB(red, green, blue), duration, blink);

  String ack = "ACK LED group=" + String(group) +
               " RGB=" + String(red) + "," + String(green) + "," + String(blue) +
               " duration=" + String(duration) + " blink=" + String(blink);
  mqttClient.publish((baseTopic + "/channel/00/out").c_str(), ack.c_str(), true);
}

  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  String p;
  for (unsigned int i = 0; i < length; i++) p += (char)payload[i];
  handleMQTTPayload(t, p);
}

// -------------------------
// WiFi & MQTT
// -------------------------
void initWiFi() {
  Serial.print("Verbinde WLAN: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println("\n✅ WLAN verbunden: " + WiFi.localIP().toString());
}

void initMQTT() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void mqttReconnect() {
  if (mqttClient.connected()) return;

  unsigned long now = millis();
  if (now - lastMqttReconnectAttempt < 3000) return; // alle 3s versuchen
  lastMqttReconnectAttempt = now;

  // Basis-Client-ID aus (evtl. getürkter) MAC
  String baseClientId = "ESP32_" + getEffectiveMacForTopic();
  String clientId = baseClientId;

  Serial.print("Verbinde zu MQTT als ");
  Serial.print(clientId);
  Serial.println(" ...");

  // Erster Verbindungsversuch mit Basis-ID
  bool ok = mqttClient.connect(
        clientId.c_str(),
        mqtt_user,
        mqtt_pass,
        (baseTopic + "/channel/00/device").c_str(),
        1,
        true,
        "offline"
      );

  if (!ok) {
    int rc = mqttClient.state();
    Serial.print("Fehler rc=");
    Serial.println(rc);

    // Wenn Identifier rejected (rc == 2) -> versuche einmalig alternative ID
    if (rc == 2) {
      String altId = baseClientId + "_" + String(random(100, 999));
      Serial.print("Versuche alternative Client-ID: ");
      Serial.println(altId);

      ok = mqttClient.connect(
            altId.c_str(),
            mqtt_user,
            mqtt_pass,
            (baseTopic + "/channel/00/device").c_str(),
            1,
            true,
            "offline"
      );

      if (ok) {
        clientId = altId; // optional, damit du die aktive ID kennst
        Serial.println("✅ MQTT verbunden mit alternativer Client-ID");
      } else {
        Serial.print("Alternative ID ebenfalls fehlgeschlagen, rc=");
        Serial.println(mqttClient.state());
      }
    }
  } else {
    Serial.println("✅ MQTT verbunden");
  }

  if (ok) {
    // Gemeinsamer Setup-Code bei erfolgreichem Connect
    mqttClient.subscribe((baseTopic + "/#").c_str());
    mqttClient.subscribe((baseTopic + "/channel/00/counter/00").c_str());
    mqttClient.subscribe((baseTopic + "/channel/00/counter/01").c_str());
    mqttClient.subscribe((baseTopic + "/channel/00/counter/02").c_str());
    mqttClient.subscribe((baseTopic + "/channel/00/counter/03").c_str());

    // Device sofort aktiv melden
    mqttClient.publish((baseTopic + "/channel/00/device").c_str(), "active", true);

    bootTime = millis();
    initialPublishDone = false;

    if (watchdogResetDetected) {
      mqttClient.publish((baseTopic + "/config/out").c_str(), "watchdog_reset", true);
      watchdogResetDetected = false;
    }
  }
 }
void handleMQTT() {
  if (!mqttClient.connected()) mqttReconnect();
  mqttClient.loop();
}

// -------------------------
// Initiale Retained + GPIO Restore
// -------------------------
void publishInitialRetained() {
    //mqttClient.publish((baseTopic + "/config/mac-address").c_str(), WiFi.macAddress().c_str(), true);
    mqttClient.publish((baseTopic + "/config/mac-address").c_str(), getEffectiveMacRaw().c_str(), true);
    mqttClient.publish((baseTopic + "/config/ip-address").c_str(), WiFi.localIP().toString().c_str(), true);
    mqttClient.publish((baseTopic + "/config/state").c_str(), "ok", true);
    mqttClient.publish((baseTopic + "/config/wifi-rssi").c_str(), String(WiFi.RSSI()).c_str(), true);

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char bootTimeStr[30];
        strftime(bootTimeStr, sizeof(bootTimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        mqttClient.publish((baseTopic + "/config/boot-timestamp").c_str(), bootTimeStr, true);

        char localTime[30];
        strftime(localTime, sizeof(localTime), "%Y-%m-%d %H:%M:%S", &timeinfo);
        mqttClient.publish((baseTopic + "/config/localtime-date").c_str(), localTime, true);
    }

    mqttClient.publish((baseTopic + "/config/software-version").c_str(), SOFTWARE_VERSION, true);
    mqttClient.publish((baseTopic + "/config/in").c_str(), "init", true);
    mqttClient.publish((baseTopic + "/config/out").c_str(), "init", true);
    mqttClient.publish((baseTopic + "/channel/00/in").c_str(), "init", true);
    mqttClient.publish((baseTopic + "/channel/00/out").c_str(), "init", true);
    mqttClient.publish((baseTopic + "/channel/00/device").c_str(), "active", true);

    // -------------------------
    // GPIO-States wiederherstellen
    // -------------------------
    int pins[4] = { gpio12, gpio13, gpio25, gpio33 };
    int* lastStates[4] = { &lastSentGpio12, &lastSentGpio13, &lastSentGpio25, &lastSentGpio33 };

    for (int i = 0; i < 4; i++) {
        if (*lastStates[i] >= 0) {
            // Wenn Retained-Wert bereits vorhanden, verwende ihn
            digitalWrite(pins[i], *lastStates[i] ? HIGH : LOW);
        } else {
            // Kein Retained-Wert → echten Pin-Zustand übernehmen
            int state = digitalRead(pins[i]);
            digitalWrite(pins[i], state);  
            *lastStates[i] = (state == HIGH ? 1 : 0);
        }

        // MQTT-Status veröffentlichen
        mqttClient.publish(
            (baseTopic + "/channel/00/gpios/" + String(pins[i])).c_str(),
            ("state=" + String(*lastStates[i])).c_str(),
            true
        );
    }

    // Counter initial veröffentlichen
    mqttClient.publish((baseTopic + "/channel/00/counter/00").c_str(), ("value=" + String(counter0)).c_str(), true);
    mqttClient.publish((baseTopic + "/channel/00/counter/01").c_str(), ("value=" + String(counter1)).c_str(), true);
    mqttClient.publish((baseTopic + "/channel/00/counter/02").c_str(), ("value=" + String(counter2)).c_str(), true);
    mqttClient.publish((baseTopic + "/channel/00/counter/03").c_str(), ("value=" + String(counter3)).c_str(), true);
}


// -------------------------
// LED Startsequenz
// -------------------------
void ledStartupSequence() {
int numGroups = 5;        // Anzahl der LED-Gruppen
  int speed = 70;           // Geschwindigkeit des Lauflichts (ms)
  int trailLength = 2;      // Länge des Schweifs

  // Berechne die Gesamtzahl der LEDs
  int totalLeds = 0;
  for (int g = 0; g < numGroups; g++) totalLeds += ledGroups[g].numLeds;

  // Schleife für ein paar Durchläufe
  for (int cycle = 0; cycle < 1; cycle++) {
    // Vorwärts
    for (int i = 0; i < totalLeds; i++) {
      int idx = 0;
      for (int g = 0; g < numGroups; g++) {
        for (int l = 0; l < ledGroups[g].numLeds; l++) {
          // Berechne Helligkeit basierend auf Abstand zur Haupt-LED
          int distance = abs(idx - i);
          byte brightness = 0;
          if (distance < trailLength) {
            brightness = 255 - (distance * (255 / trailLength));
          }
          ledGroups[g].leds[l] = CRGB(brightness, 0, 0);  // Rot mit Schweif
          idx++;
        }
      }
      FastLED.show();
      delay(speed);
    }

    // Rückwärts
    for (int i = totalLeds - 2; i > 0; i--) {
      int idx = 0;
      for (int g = 0; g < numGroups; g++) {
        for (int l = 0; l < ledGroups[g].numLeds; l++) {
          int distance = abs(idx - i);
          byte brightness = 0;
          if (distance < trailLength) {
            brightness = 255 - (distance * (255 / trailLength));
          }
          ledGroups[g].leds[l] = CRGB(brightness, 0, 0);
          idx++;
        }
      }
      FastLED.show();
      delay(speed);
    }
  }

  // Endzustand: Blau
  for (int g = 0; g < numGroups; g++) fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB(0, 0, 255));
  FastLED.show();
}

   // -------------------------
// LED Startsequenz
// -------------------------
//void ledStartupSequence() {
//  for (int g = 0; g < 5; g++) fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB(255, 0, 0));
//  FastLED.show(); delay(500);

//  for (int g = 0; g < 5; g++) fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB(0, 255, 0));
//  FastLED.show(); delay(500);

  // Endzustand: Blau
  //for (int g = 0; g < 5; g++) fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB(0, 0, 255));
 // FastLED.show();
//}


// -------------------------
// Setup & Loop
// -------------------------
void setup() {
  Serial.begin(115200);
  delay(10);
    // Seed für alternative Client-ID (falls nötig)
  randomSeed((uint32_t)esp_random());
  Serial.println("Booting...");

  // Prüfen, ob Neustart durch Watchdog erfolgte
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_TASK_WDT) {
    Serial.println("⚠️ Neustart durch Watchdog!");
    watchdogResetDetected = true;
  }
  initWiFi();

// ⏰ NTP-Zeit holen
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Mitteleuropäische Zeit + Sommerzeit
  tzset();

#ifdef USE_TLS
  espClient.setInsecure(); 
  Serial.println("TLS aktiv (setInsecure())");
#else
  Serial.println("TLS deaktiviert");
#endif

  String macTopic = getEffectiveMacForTopic();
  baseTopic = String(topicPrefix) + "/" + macTopic;
  Serial.println("baseTopic = " + baseTopic + "  (using MAC " + getEffectiveMacRaw() + ")");


  initMQTT();
  // GPIOs als Outputs
  pinMode(gpio12, OUTPUT);
  pinMode(gpio13, OUTPUT);
  pinMode(gpio25, OUTPUT);
  pinMode(gpio33, OUTPUT);

  // Counter-Inputs
  pinMode(counter0Pin, INPUT_PULLUP);
  pinMode(counter1Pin, INPUT_PULLUP);
  pinMode(counter2Pin, INPUT_PULLUP);
  pinMode(counter3Pin, INPUT_PULLUP);



  // LEDs
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS_TOTAL);
  FastLED.clear();
  FastLED.show();

  ledStartupSequence();

  //  Watchdog starten (30 Sekunden Timeout)
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);

  bootTime = millis();
  initialPublishDone = false;
}

void loop() {
  pollCounters();
  //  Watchdog resetten
  esp_task_wdt_reset();

  // WLAN prüfen
  if (WiFi.status() != WL_CONNECTED) {
    initWiFi();
  }

  // MQTT prüfen
  handleMQTT();

  // LED Animationen
  updateLeds();

  // GPIO-Duration überwachen
for (int i = 0; i < 4; i++) {
  if (gpioStates[i].active && millis() > gpioStates[i].offTime) {
    gpioStates[i].active = false;
    digitalWrite(gpioStates[i].pin, LOW);

    // Merker aktualisieren
    if (gpioStates[i].pin == gpio12) lastSentGpio12 = 0;
    if (gpioStates[i].pin == gpio13) lastSentGpio13 = 0;
    if (gpioStates[i].pin == gpio25) lastSentGpio25 = 0;
    if (gpioStates[i].pin == gpio33) lastSentGpio33 = 0;

    // MQTT-State senden
    String gpioMsg = "state=0";
    mqttClient.publish((baseTopic + "/channel/00/gpios/" + String(gpioStates[i].pin)).c_str(), gpioMsg.c_str(), true);
  }
}


  // Initiale Retained Werte nach 5s schicken
  if (!initialPublishDone && millis() - bootTime > 5000) {
    publishInitialRetained();
    initialPublishDone = true;
  }

  // --- Lokalzeit periodisch publizieren (jede localtimeInterval ms) ---
static unsigned long lastTimePub = 0;
unsigned long nowTime = millis();

if (nowTime - lastTimePub > localtimeInterval) {
  lastTimePub = nowTime;

  // Nur versuchen, wenn MQTT verbunden ist (optional aber sinnvoll)
  if (mqttClient.connected()) {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char localTime[30];
      strftime(localTime, sizeof(localTime), "%Y-%m-%d %H:%M:%S", &timeinfo);
      mqttClient.publish((baseTopic + "/config/localtime-date").c_str(), localTime, true);
      Serial.print("⏰ Localtime published: ");
      Serial.println(localTime);
    } else {
      Serial.println("⚠️ getLocalTime() failed - no NTP yet");
    }
  } else {
    Serial.println("⚠️ MQTT not connected - skipping localtime publish");
  }
}


  // Counter publish: sofort bei Änderung + Heartbeat alle 5 Minuten
static unsigned long lastHeartbeat = 0;
unsigned long now = millis();

if (counter0 != lastSentCounter0 || now - lastHeartbeat > 300000) { // 5 min
  lastSentCounter0 = counter0;
  mqttClient.publish((baseTopic + "/channel/00/counter/00").c_str(), ("value=" + String(counter0)).c_str(), true);
}

if (counter1 != lastSentCounter1 || now - lastHeartbeat > 300000) {
  lastSentCounter1 = counter1;
  mqttClient.publish((baseTopic + "/channel/00/counter/01").c_str(), ("value=" + String(counter1)).c_str(), true);
}

if (counter2 != lastSentCounter2 || now - lastHeartbeat > 300000) {
  lastSentCounter2 = counter2;
  mqttClient.publish((baseTopic + "/channel/00/counter/02").c_str(), ("value=" + String(counter2)).c_str(), true);
}

if (counter3 != lastSentCounter3 || now - lastHeartbeat > 300000) {
  lastSentCounter3 = counter3;
  mqttClient.publish((baseTopic + "/channel/00/counter/03").c_str(), ("value=" + String(counter3)).c_str(), true);
}

if (now - lastHeartbeat > 300000) {
  lastHeartbeat = now; // Heartbeat zurücksetzen
}

  // WLAN-Signalstärke periodisch senden
  static unsigned long lastRssiPublish = 0;
  if (millis() - lastRssiPublish > 60000) {
    lastRssiPublish = millis();
    mqttClient.publish((baseTopic + "/config/wifi-rssi").c_str(), String(WiFi.RSSI()).c_str(), true);
  }
    delay(5); 
}


