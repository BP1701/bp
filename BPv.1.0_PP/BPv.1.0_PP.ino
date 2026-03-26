/******************************************************************************************
 *  Projekt: WLAN Pinpad
 *  Version: 1.0
 *  Autor:   B.P
 *  Datum:   01.10.2025
 *
 *  Beschreibung:
 *    ESP32-basiertes WLAN-Pinpad mit MQTT, WS2812-LEDs und GPIO-Steuerung.
 *
 *  Funktionen:
 *    - WLAN + MQTT Verbindung (optional TLS)
 *    - PIN-Eingabe über Keypad (4x3 Tasten)
 *    - LED-Gruppen (WS2812) für Feedback
 *    - GPIO-Ausgänge (4x) mit optionaler Dauersteuerung (Duration)
 *    - MQTT Initial-Publish, State-ACK, Retained States
 *    - Watchdog-Schutz
 *    - NTP Zeitabgleich & lokale Zeitsynchronisation
 *
 *  GPIO-Belegung:
 *    - GPIO12 : Output / Relais oder LED / Pin 1
 *    - GPIO13 : Output / Relais oder LED / Pin 2
 *    - GPIO25 : Output / Relais oder LED / Pin 3
 *    - GPIO33 : Output / Relais oder LED / Pin 4
 *
 *  Keypad-Pins:
 *    - Rows : 23, 2, 14, 4
 *    - Cols : 39, 34, 35
 *
 *  LED-Gruppen (WS2812):
 *    - LED_PIN   : 15
 *    - Gruppe 0  : 2 LEDs
 *    - Gruppe 1  : 2 LEDs
 *    - Gruppe 2  : 2 LEDs
 *    - Gruppe 3  : 2 LEDs
 *    - Gruppe 4  : 2 LEDs
 *
 *  MQTT-Befehle:
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
#include <Keypad.h>


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
//#define USE_TLS   // einkommentieren = TLS an, auskommentieren = TLS aus
#define SOFTWARE_VERSION "BP:v.1.0_PP"

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
const char* ssid     = "FRITZ!Box 7520 QM";
const char* password = "28389585079405190635";

const char* mqtt_server = "192.168.178.108";
#ifdef USE_TLS
  const int mqtt_port = 8883;
#else
  const int mqtt_port = 1885;
#endif
const char* mqtt_user = "mqtt";
const char* mqtt_pass = "G4Cm#g";

// ⏰ Intervall für Localtime-Update (ms)
const unsigned long localtimeInterval = 60000;   

// -------------------------
// Topic-Prefix
// -------------------------
const char* topicPrefix = "Pinpad";

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

// -------------------------
// Keypad Setup
// -------------------------
const byte ROWS = 4; 
const byte COLS = 3; 

char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};

byte rowPins[ROWS] = {23, 2, 14, 4}; // anpassen an deine Verkabelung // R1=MOSI, R2=DC, R3=CS, R4=RST
byte colPins[COLS] = {39, 34, 35};     // anpassen an deine Verkabelung // C1, C2, C3

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String enteredPin = "";      // PIN wird hier gesammelt
const int PIN_LENGTH = 4;    // Länge des PINs
const unsigned long LED_FEEDBACK_DURATION = 2000; // ms, Dauer, bis alle LEDs wieder blau gehen
unsigned long ledFeedbackTime = 0;
bool pinSent = false;



// Non-blocking MQTT-Reconnect
unsigned long lastMqttReconnectAttempt = 0;


bool watchdogResetDetected = false;   // Watchdog-Flag

// Flag & Timer für verzögerten Initial-Publish
bool initialPublishDone = false;
unsigned long bootTime = 0;

#define LED_PIN 15
#define NUM_LEDS_GROUP0 2
#define NUM_LEDS_GROUP1 2
#define NUM_LEDS_GROUP2 2
#define NUM_LEDS_GROUP3 2
#define NUM_LEDS_GROUP4 2

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



void handlePinLedFeedback() {
  if (pinSent && millis() - ledFeedbackTime > LED_FEEDBACK_DURATION) {
    for (int g = 0; g < 4; g++) setLedGroup(g, CRGB::Blue, 0, false);
    pinSent = false;
    enteredPin = "";
  }
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
  int posG = payload.indexOf("gpio=");
  int posS = payload.indexOf("state=");
  int posD = payload.indexOf("duration=");

  if (posG >= 0 && posS >= 0) {
    int gpio = payload.substring(posG + 5, payload.indexOf(';', posG) > 0 ? payload.indexOf(';', posG) : payload.length()).toInt();
    int state = payload.substring(posS + 6, payload.indexOf(';', posS) > 0 ? payload.indexOf(';', posS) : payload.length()).toInt();
    unsigned long duration = 0;

    if (posD >= 0) {
      duration = payload.substring(posD + 9, payload.indexOf(';', posD) > 0 ? payload.indexOf(';', posD) : payload.length()).toInt();
    }

    if (gpio == gpio12 || gpio == gpio13 || gpio == gpio25 || gpio == gpio33) {
      // Invertierte Logik
      digitalWrite(gpio, state ? HIGH : LOW);

      // Merker aktualisieren
      if (gpio == gpio12) lastSentGpio12 = state;
      if (gpio == gpio13) lastSentGpio13 = state;
      if (gpio == gpio25) lastSentGpio25 = state;
      if (gpio == gpio33) lastSentGpio33 = state;

      // Falls duration > 0 → Zeitsteuerung aktivieren
      if (duration > 0 && state == 1) {
        for (int i = 0; i < 4; i++) {
          if (gpioStates[i].pin == gpio) {
            gpioStates[i].active = true;
            gpioStates[i].offTime = millis() + duration;
          }
        }
      }

      // ACK & MQTT-State
      String ack = "GPIO" + String(gpio) + " -> state=" + String(state);
      mqttClient.publish((baseTopic + "/channel/00/out").c_str(), ack.c_str(), true);

      String gpioMsg = "state=" + String(state);
      mqttClient.publish((baseTopic + "/channel/00/gpios/" + String(gpio)).c_str(), gpioMsg.c_str(), true);

      mqttClient.publish((baseTopic + "/channel/00/device").c_str(), "active", true);
    }
  }
}


    if (payload.indexOf("cmd=SET_LED") >= 0) {
      int group = payload.substring(payload.indexOf("group=") + 6, payload.indexOf(';', payload.indexOf("group="))).toInt();
      int red = payload.substring(payload.indexOf("red=") + 4, payload.indexOf(';', payload.indexOf("red="))).toInt();
      int green = payload.substring(payload.indexOf("green=") + 6, payload.indexOf(';', payload.indexOf("green="))).toInt();
      int blue = payload.substring(payload.indexOf("blue=") + 5, payload.indexOf(';', payload.indexOf("blue="))).toInt();
      int duration = payload.substring(payload.indexOf("duration=") + 9, payload.indexOf(';', payload.indexOf("duration="))).toInt();
      int blink = payload.substring(payload.indexOf("blink=") + 6).toInt();

      setLedGroup(group, CRGB(red, green, blue), duration, blink);

      String ack = "ACK LED group=" + String(group) +
                   " RGB=" + String(red) + "," + String(green) + "," + String(blue) +
                   " duration=" + String(duration) + " blink=" + String(blink);
                   // ACK wirklich zurückschicken
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
// Initiale Retained
// -------------------------
void publishInitialRetained() {
  mqttClient.publish((baseTopic + "/config/mac-address").c_str(), WiFi.macAddress().c_str(), true);
  mqttClient.publish((baseTopic + "/config/ip-address").c_str(), WiFi.localIP().toString().c_str(), true);
  mqttClient.publish((baseTopic + "/config/state").c_str(), "ok", true);

  mqttClient.publish((baseTopic + "/config/wifi-rssi").c_str(), String(WiFi.RSSI()).c_str(), true);

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char bootTime[30];
    strftime(bootTime, sizeof(bootTime), "%Y-%m-%d %H:%M:%S", &timeinfo);
    mqttClient.publish((baseTopic + "/config/boot-timestamp").c_str(), bootTime, true);

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

// GPIO-States wiederherstellen oder Default 0
  if (lastSentGpio12 >= 0) {
  digitalWrite(gpio12, lastSentGpio12 ? HIGH : LOW);
} else {
  digitalWrite(gpio12, LOW); // Default 0
}
  if (lastSentGpio13 >= 0) {
  digitalWrite(gpio13, lastSentGpio13 ? HIGH : LOW);
} else {
  digitalWrite(gpio13, LOW);
}
  if (lastSentGpio25 >= 0) {
  digitalWrite(gpio25, lastSentGpio25 ? HIGH : LOW);
} else {
  digitalWrite(gpio25, LOW);
}
  if (lastSentGpio33 >= 0) {
  digitalWrite(gpio33, lastSentGpio33 ? HIGH : LOW);
} else {
  digitalWrite(gpio33, LOW);
}
  mqttClient.publish((baseTopic + "/channel/00/gpios/12").c_str(), ("state=" + gpioStateLogical(gpio12)).c_str(), true);
  mqttClient.publish((baseTopic + "/channel/00/gpios/13").c_str(), ("state=" + gpioStateLogical(gpio13)).c_str(), true);
  mqttClient.publish((baseTopic + "/channel/00/gpios/25").c_str(), ("state=" + gpioStateLogical(gpio25)).c_str(), true);
  mqttClient.publish((baseTopic + "/channel/00/gpios/33").c_str(), ("state=" + gpioStateLogical(gpio33)).c_str(), true);
  }

// -------------------------
// LED Startsequenz
// -------------------------
void ledStartupSequence() {
  for (int g = 0; g < 5; g++) fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB(255, 0, 0));
  FastLED.show(); delay(500);

  for (int g = 0; g < 5; g++) fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB(0, 255, 0));
  FastLED.show(); delay(500);

  // Endzustand: Blau
  for (int g = 0; g < 5; g++) fill_solid(ledGroups[g].leds, ledGroups[g].numLeds, CRGB(0, 0, 255));
  FastLED.show();
}

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
  //  Watchdog resetten
  esp_task_wdt_reset();

  // WLAN prüfen
  if (WiFi.status() != WL_CONNECTED) {
    initWiFi();
  }

  // MQTT prüfen
  handleMQTT();

  char key = keypad.getKey();
if (key) {
  if (key >= '0' && key <= '9') {
    // Zahl gedrückt
    enteredPin += key;

    // LED-Feedback: Gruppe grün für visuelle Bestätigung
    if (enteredPin.length() == 1) setLedGroup(0, CRGB::Green, LED_FEEDBACK_DURATION, false);
    else if (enteredPin.length() == 2) setLedGroup(1, CRGB::Green, LED_FEEDBACK_DURATION, false);
    else if (enteredPin.length() == 3) setLedGroup(2, CRGB::Green, LED_FEEDBACK_DURATION, false);
    else if (enteredPin.length() == 4) setLedGroup(3, CRGB::Green, LED_FEEDBACK_DURATION, false);

  } else if (key == '*') {
    // Reset
    enteredPin = "";
    pinSent = false;
    for (int g = 0; g < 4; g++) setLedGroup(g, CRGB::Blue, 0, false);
  } else if (key == '#') {
    if (enteredPin.length() == PIN_LENGTH) {
      // PIN senden
      String msg = "pin=" + enteredPin;
      mqttClient.publish((baseTopic + "/channel/00/out").c_str(), msg.c_str(), true);
      Serial.println("📨 PIN gesendet: " + enteredPin);
      pinSent = true;
      ledFeedbackTime = millis();
    }
  }
}

  // LED Animationen
  updateLeds();
  handlePinLedFeedback();

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

  // WLAN-Signalstärke periodisch senden
  static unsigned long lastRssiPublish = 0;
  if (millis() - lastRssiPublish > 60000) {
    lastRssiPublish = millis();
    mqttClient.publish((baseTopic + "/config/wifi-rssi").c_str(), String(WiFi.RSSI()).c_str(), true);
  }
}

 
