#include <Wire.h>
#include <FastLED.h>

#define I2C_ADDRESS 0x08
#define BEEPER PD5
#define GPIO_INTERRUPT_1 PD3
#define WS2812PIN PD4

#define NUM_LEDS 2
#define I2C_MAX_TELEGRAM_LENGHT 16
#define MAX_ID_LEN 25
#define I2C_REGISTER_GETTAGID 0x12
#define I2C_REGISTER_BEEP 0x13
#define I2C_TIMEOUT_TIME 2000     // Tag wird nach I2C_TIMEOUT_TIME gelöscht
#define SCAN_BLOCK_TIME 3000      // 3 Sekunden kein neuer Scan

// ------------------- GLOBAL -------------------
char tag_uuid[MAX_ID_LEN];        // Aktueller Code
char tmp_uuid[MAX_ID_LEN];        // Scanner Buffer
volatile int pos = 0;

bool tag_present = false;
unsigned long i2cTimeoutTimer = 0;
unsigned long scanBlockTimeout = 0;   // <-- Neuer Timeout

bool i2c_write_request = false;
int i2c_rx_register;
byte i2c_received_data[I2C_MAX_TELEGRAM_LENGHT];

CRGB leds[NUM_LEDS];
unsigned long beepTimer = 0;

// ------------------- LED / BEEP -------------------
void doBeep(int t){ beepTimer = millis() + t; digitalWrite(BEEPER,HIGH); }
void checkBeep(){ if(beepTimer>0 && millis()>beepTimer){ digitalWrite(BEEPER,LOW); beepTimer=0; } }

void uuid_read() {
  while(Serial1.available()){
    char c = Serial1.read();

    if(c == '\r' || c == '\n'){  // Ende-Zeichen erkannt
      if(pos > 0){ // nur wenn wirklich Daten vorhanden
        tmp_uuid[pos] = 0x00;

        // Trim Whitespaces
        int start = 0;
        while(tmp_uuid[start] == ' ' || tmp_uuid[start] == '\t') start++;
        int end = pos - 1;
        while(end >= start && (tmp_uuid[end] == ' ' || tmp_uuid[end] == '\t')) {
          tmp_uuid[end] = 0x00;
          end--;
        }

        if(start <= end){
          // **Nur neuen Scan akzeptieren, wenn Scan-Timeout abgelaufen**
          if(millis() >= scanBlockTimeout){
            char* cleanTag = &tmp_uuid[start];

            // Tag direkt übernehmen
            strncpy(tag_uuid, cleanTag, MAX_ID_LEN);
            tag_uuid[MAX_ID_LEN-1] = 0x00;
            tag_present = true;

            digitalWrite(GPIO_INTERRUPT_1, HIGH);
            Serial.print("Tag found: ");
            Serial.println(tag_uuid);

            doBeep(300);

            i2cTimeoutTimer = millis() + I2C_TIMEOUT_TIME;
            scanBlockTimeout = millis() + SCAN_BLOCK_TIME;  // <-- Start Scan-Block
          } else {
           // Serial.println("Scan ignored: Timeout active");
          }
        }

        pos = 0; // Buffer zurücksetzen
      }
    } else {
      if(pos < MAX_ID_LEN-1) tmp_uuid[pos++] = c;
    }
  }

  // Tag nach Anzeige automatisch verwerfen
  if(tag_present && millis() > i2cTimeoutTimer){
    tag_present = false;
    digitalWrite(GPIO_INTERRUPT_1, LOW);
    memset(tag_uuid, 0, MAX_ID_LEN);
  }
}

// ------------------- I2C -------------------
void receiveEvent(int howMany){
  i2c_write_request = false;
  if (howMany < 1) return;

  i2c_rx_register = Wire.read();
  int i = 0;
  while (Wire.available()) {
    if (i < I2C_MAX_TELEGRAM_LENGHT)
      i2c_received_data[i++] = Wire.read();
    else
      Wire.read();
  }

  if (i2c_rx_register == I2C_REGISTER_GETTAGID) {
    i2c_write_request = true;
  }

  if (i2c_rx_register == I2C_REGISTER_BEEP) {
    doBeep(300);
  }
}

void requestEvent(){
  if(!i2c_write_request) return;

  if(i2c_rx_register == I2C_REGISTER_GETTAGID){
    uint8_t len = strlen(tag_uuid);

    if(len == 0){
      Wire.write((uint8_t)0);
      i2c_write_request = false;
      return;
    }

    digitalWrite(GPIO_INTERRUPT_1, LOW);
    Wire.write(len);
    Wire.write((uint8_t*)tag_uuid, len);

    // Tag sofort verwerfen nach I2C-Abfrage
    memset(tag_uuid,0,MAX_ID_LEN);
    tag_present = false;
    i2c_write_request = false;
  }
}

// ------------------- SETUP -------------------
void setup(){
  Serial.begin(115200);
  Serial1.begin(9600);
  Wire.begin(I2C_ADDRESS);
  Wire.onReceive(receiveEvent);
  Wire.onRequest(requestEvent);

  pinMode(GPIO_INTERRUPT_1,OUTPUT);
  pinMode(BEEPER,OUTPUT);
  digitalWrite(GPIO_INTERRUPT_1,LOW);
  digitalWrite(BEEPER,LOW);

  FastLED.addLeds<WS2812B,WS2812PIN,GRB>(leds,NUM_LEDS);
  for(int i=0;i<NUM_LEDS;i++) leds[i] = CRGB(255,0,0); // LED dauerhaft rot
  FastLED.show();

  memset(tag_uuid,0,MAX_ID_LEN);
  Serial.println("Scanner Ready");
  doBeep(300);
}

// ------------------- LOOP -------------------
void loop(){
  uuid_read();
  checkBeep();
}
