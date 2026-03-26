//#define PLATFORM_SDK_20

#include "pp_io_modul_rs485_st.h"
#include <PP_RS485.h>
#include <PP_RFID.h>
#include <PP_OPTICON.h>
#include <PP_PINPAD.h>
#include <PP_MCP23017.h>
#include <pp_nexscan.h>
#include <time.h>
#include <EEPROM.h>
#include "PP_ST_CCTALK.h"
#include "PP_ST_EMP800.h"
#include <esp_task_wdt.h>

#ifdef PLATFORM_SDK_20
  #include <LittleFS.h>
#endif

#include <mbedtls/md5.h>

#define PROG  "PP_TT_IO_MODUL_RS485"
#define VER   0.3
#define byte_prog_ver 0x03
//#define byte_prog_ver 0x00

#define byte_app_version 0x04

// device address
#define DEVADDR 0x10

#define _DEBUG
#define UPDATE_TESTING
#define WDT_TIMEOUT 3

// special standalone mode "Nordsee Camp"
// #define NSC

// enable modules
//#define MOD_COUNTER
#define MOD_GPIO
//#define MOD_NEXSCAN
#define MOD_OPTICON
#define MOD_RFID
#define MOD_DIGITAL_INPUT
//#define MOD_PINPAD
//#define MOD_I2CRFID;
//#define MOD_COINS;
//#define MOD_IO64
//#define MOD_EV_CHARG
//#define MOD_BEEPER
//#define MOD_GATE
//#define MOD_AQUA_STATION

bool BLUE_LED_TRIGGER = false;
bool RED_LED_TRIGGER = false;

#ifdef MOD_EV_CHARG
  #define MOD_GPIO
  #define MOD_COUNTER
  #define MOD_STATUS_LEDS
  #define MOD_NEXSCAN
#endif
// there can be only ONE
#ifdef MOD_IO64
  #undef MOD_COUNTER
  #undef MOD_GPIO
  #undef MOD_NEXSCAN
  #undef MOD_OPTICON
  #undef MOD_RFID
  #undef MOD_DIGITAL_INPUT
  #undef MOD_PINPAD
  #undef MOD_I2CRFID
  #undef MOD_COINS
#endif

#ifdef MOD_IO64
uint16_t iomodul_counter_value_tmp[32];
uint16_t iomodul_counter_values[32];
uint32_t iomodul_debug_timer = 0;
#endif

#ifdef MOD_COINS //Uses counters for cash and coins
  #undef MOD_COUNTER
  #define COUNTER_COUNT 2
#endif

#ifdef MOD_AQUA_STATION
  #define MOD_GPIO
  #define MOD_COUNTER
  #define COUNTER_COUNT 1
  uint8_t  counter_pins[] = {36};
#endif

// modul power-meter
#ifdef MOD_COUNTER
#define COUNTER_S0_MIN 20
#define COUNTER_S0_MAX 1500
//#define COUNTER_COUNT 1 // MAX = 4 // Wassersaeule
#define COUNTER_COUNT 2 // MAX = 4 // Stromsaeule 2x
//#define COUNTER_COUNT 4  // MAX = 4 // Stromsaeule 4x
//uint8_t  counter_pins[] = {36}; // Wassersaule
uint8_t  counter_pins[] = {36, 39}; // Stromsaeule 2x
//uint8_t  counter_pins[] = {36, 39, 34, 35}; // Stromsaeule 4x
//uint8_t  counter_pins[] = {36, 39}; // ev-css
volatile uint32_t counter_s0_duration[COUNTER_COUNT];
volatile uint8_t counter_s0_value[COUNTER_COUNT];
volatile bool     counter_pins_value[COUNTER_COUNT];
volatile bool     counter_pins_temp_value[COUNTER_COUNT];
#endif

volatile uint32_t counter_value[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 , 0};
volatile uint8_t  counter_update_flag = 0;

// global defines
//#define LED_NUM_LEDS 8
#ifdef MOD_EV_CHARG
#define LED_DATA_PIN 5
#else
#define LED_DATA_PIN 15
#endif
// globals
PP_RFID rfid;
PP_OPTICON opticon;
PP_RS485 rs485(DEVADDR);
PP_PINPAD pinpad;
PP_NEXSCAN nexscan;
PP_MCP23017 port_exp_output_0;
PP_MCP23017 port_exp_output_1;
PP_MCP23017 port_exp_input_0;
PP_MCP23017 port_exp_input_1;

byte rfid_data[64];
uint8_t  rfid_data_len;
bool rfid_data_available = false;
byte opticon_data[2048];
uint16_t  opticon_data_len;
bool opticon_data_avaiable = false;
byte counter_data[64];
uint16_t counter_data_len;
bool counter_data_available = false;
uint8_t counter_data_source;
bool gpios_data_available = false;
byte gpios_data[32];
uint8_t gpios_data_len;
bool input_gpios_data_available = false;
byte pinpad_data[10];
uint8_t  pinpad_data_len;
bool pinpad_data_available = false;
byte coin_data[8];
uint8_t  coin_data_len;
bool coin_data_available = false;

uint32_t iomodul_input_data;
uint8_t  iomodul_input_data_len;
bool iomodul_input_data_available = false;


uint16_t iomodul_output_data;
uint8_t  iomodul_output_data_len;
bool iomodul_output_available = false;

bool card_data_available = false;
uint32_t card_data_new_card;

bool update_data_available = false;
uint8_t update_data = 0;

bool counter_undef = true;
bool outputs_undef = true;
bool gateway_never_seen = true;

uint32_t pinpad_timer;
#define PINPAD_DELAY 1500

uint32_t _time_rfid;
uint32_t _time_scan;

void setup()
{
  // init debug console
  //Serial.begin(115200);
  Serial.begin(9600, SERIAL_8N1, 21, 1);
  delay(1000);

  Serial.print(PROG);
  Serial.print(" ");
  Serial.print(VER);
  Serial.println(" starting...");

  Serial.printf("RS485 Device-Address: 0x%02x\n", DEVADDR);

  /* TEST TEST TEST */
  int artikel = 0;
  int id = 0;
  uint32_t timestamp = 0;

  // initialize leds
  init_leds();
  // initialize rs485
  init_rs485();
  // initialize eeprom aka non-volatile storage
  init_eeprom();
  // initialize filesystem(LITTLEFS)
  init_fs();
  // initialize fw_update module
  init_fw_update();

#ifdef WDT_TIMEOUT
  Serial.println("Configuring WDT...");
  esp_task_wdt_init(WDT_TIMEOUT, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch
#endif

#ifdef MOD_GPIO
  // initialize outputs
  init_outputs();
#endif

#ifdef MOD_RFID
  // initialize rfid (feig)
  init_rfid();
#endif

#ifdef MOD_OPTICON
  // initialize barcode scanner (opticon)
  init_opticon();
#endif

#ifdef MOD_NEXSCAN
  init_nexscan();
  #define MOD_BEEPER
#endif

#ifdef MOD_COUNTER
  init_counter();
#endif

#ifdef MOD_DIGITAL_INPUT
  init_inputs();
#endif

#ifdef MOD_PINPAD
  // init pinpad
  if (pinpad_timer == 0)
  {
    init_pinpad();
  }
  else
  {
    if (millis() - pinpad_timer > PINPAD_DELAY)
    {
      led_set(0, 0, 0, 8, 0, 0);
      pinpad_timer = 0;
    }
  }

  digitalWrite(12, HIGH); // Enable LED Background 
#endif

#ifdef MOD_GPIO
  set_gpio(21, 1, 150, 0);
#endif

#ifdef MOD_IO64
  //debug
  for (int i = 0; i < 32; i++)
  {
    iomodul_counter_values[i] = 0;
  }
  init_io();
#endif

#ifdef MOD_I2CRFID
  init_i2c_rfid();
#endif

#ifdef MOD_COINS
  init_coins();
#endif

#ifdef MOD_STATUS_LEDS
  init_status_leds();
#endif

#ifdef MOD_BEEPER
  init_beeper();
#endif  

#ifdef MOD_GATE
  init_gate();
#endif  

#ifdef MOD_AQUA_STATION
  init_aqua_station();
#endif
  
  Serial.println("begin loop");
}

void loop()
{
  // process rs485 data
  rs485.receive();

  // process led
  set_leds();

#ifdef WDT_TIMEOUT
  esp_task_wdt_reset();
#endif    

#ifdef MOD_GPIO
  // process outputs
  set_outputs();
#endif

#ifdef MOD_RFID
  // process rfid (feig)
  rfid.scan();
#endif

#ifdef MOD_OPTICON
  // process opticon scanner
  opticon.scan();
#endif

#ifdef MOD_NEXSCAN
  nexscan.scan();
#endif

#ifdef MOD_COUNTER
  // counter
  loop_counter();
#endif


#ifdef MOD_DIGITAL_INPUT
  // read inputs
  loop_inputs();
#endif

#ifdef MOD_PINPAD
  // process pinpad
  if (pinpad_timer == 0)
  {
    pinpad.loop_pinpad();
  }
  else
  {
    if (millis() - pinpad_timer > PINPAD_DELAY)
    {
      led_set(0, 64, 0, 0, 75, 0);
      pinpad_timer = 0;
    }
  }
#endif

#ifdef MOD_I2CRFID
  loop_i2c_rfid();
#endif

#ifdef MOD_COINS
  loop_coins();
#endif

#ifdef MOD_IO64
  io_modul_run();
#endif

#ifdef MOD_STATUS_LEDS
  update_status_leds();
#endif

#ifdef MOD_BEEPER
  loop_beeper();
#endif

#ifdef MOD_GATE
  loop_gate();
#endif

#ifdef MOD_AQUA_STATION
  loop_aqua_station();
#endif
}
