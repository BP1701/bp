#define OLIMEX
#define GWV2

#include "pp_srv_rs_485.h"
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <FS.h>
#include "SD_MMC.h"
#include <time.h>
#include <PP_RS485.h>
#ifndef GWV2
#include <PP_PCF8574.h>
#else
#include <PP_MCP23017.h>
#include <PP_MCP23008.h>
#endif
#include <ETH.h>
#include <PubSubClient.h>
#include <HashMap.h>
#include <HTTPClient.h>
#include <Update.h>


#define PROG  "PP_SRV_RS485"
#define VER   "2.05.04.22 rem"

#define NUM_DEVICES 16

#define DEVICES_POLL_INTERVAL 25

//#define RESTART_CLIENTS_PORT 11

#define MQTT_SERVER "mqtt.pramux.de"
#define MQTT_PORT 8883
#define MQTT_USER "pramux"
#define MQTT_SECRET ";Wp{=(*_2a*H.p83/!b(CB8t"
#define IO_I2C_LOW 0x38
#define IO_I2C_HIGH 0x39
#define HTTPS_USER "pramux"
#define HTTPS_SECRET "*Pt6hGXY96q9M_PEC5msTnpR"

//PAP
#define PAP_OCR_SUPPORT
#define PAP_GATE_OPEN_GPIO      12
#define PAP_GATE_CLOSE_GPIO     13
#define PAP_GATE_STATE_GPIO     36
#define PAP_PRESENCE_STATE_GPIO 39


const char mqtt_topic_praefix[] = "pap/mdsi/prs485gw";
uint8_t mqtt_topic_praefix_field_count;
char mqtt_device_serial[18];
uint8_t chipid[6];

//const char* SSID = "Quellbad";
//const char* PSWD = "4312850687253011";
const char* SSID = "MDSI";
const char* PSWD = "jF8Oq!wCDgcEp_%20";
//const char* SSID = "WLAN_SG1";
//const char* PSWD = "10jp9avbm48r";
//const char* SSID = "iPhone_von_Sascha";
//const char* PSWD = "gxq1lzoi0kuu";

TaskHandle_t x_webserver_handle;
TaskHandle_t x_websocket_handle;
TaskHandle_t x_rs485_handle;
TaskHandle_t x_poll_devices_handle;
TaskHandle_t x_mqtt_handle;
TaskHandle_t x_io_mod_handle;
TaskHandle_t x_io_watchdog_handle;
TaskHandle_t ocr_task_handle;

SemaphoreHandle_t x_serial_s;


bool wait_for_update = false;
bool x_task_rs485_suspended = false;
bool x_task_poll_suspended = false;
bool x_task_mqtt_suspended = false;


#define _DEBUG


CreateComplexHashMap(mqtt_params, char*, char*, 16, compare_map);


// globals

// MQTT
WiFiClientSecure espClient;
WiFiClient espClient1;
PubSubClient client(espClient);

// RS485
//PP_RS485 rs485(32, 0x00);
PP_RS485 rs485(0x00);

// HTTP
HTTPClient https;

static bool eth_connected = false;
char message[64];

// devices and polling
_device devices[NUM_DEVICES];
uint32_t poll_device_timeout;
bool poll_device_wait_for_answer;
int poll_device_last_device_id;

// Port Expander
#ifndef GWV2
PP_PCF8574 port_exp_0;
#else
#define I2C_SDA 21
#define I2C_SCL 22
PP_MCP23017 port_exp_output_0;
PP_MCP23008 port_exp_input_0;
#endif

// misc
uint16_t mqtt_reconnect = 0;
bool eth = true;
bool mqtt_connected = false;

char s_uptime[32];
unsigned int uptime_timer = millis();

void setup()
{
  delay(500);
  Serial.begin(115200);
  Serial.setRxBufferSize(1024);
  Serial1.setRxBufferSize(1024);
  Serial2.setRxBufferSize(1024);

  Serial.print("... starting  ");
  Serial.print(PROG);
  Serial.print(" Ver: ");
  Serial.println(VER);
  delay(2500);

  //init reset pin;
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);

  mqtt_topic_praefix_field_count = 1;
  
  for (int i=0; i < sizeof(mqtt_topic_praefix); i++)
  {
    if(47 == mqtt_topic_praefix[i]) mqtt_topic_praefix_field_count++;   
  }

  Serial.print("MQTT Praefix Field Count: ");
  Serial.println(mqtt_topic_praefix_field_count);  

  esp_efuse_mac_get_default(chipid);
  //esp_efuse_read_mac(chipid);
  
  sprintf(mqtt_device_serial, "%02X-%02X-%02X-%02X-%02X-%02X", chipid[0], chipid[1], chipid[2], chipid[3], chipid[4], chipid[5]);
  //sprintf(mqtt_device_serial, "98-F4-AB-21-23-08"); // example for overwriting the device serial
  Serial.print("Device: ");
  Serial.println(mqtt_device_serial);
  Serial.println();
  Serial.print("connecting to: ");
  Serial.print(MQTT_SERVER);
  Serial.println("/");
  Serial.println(mqtt_topic_praefix);
  init_filesystem();
  init_WiFi();
  //init_ethernet();

  uint16_t reboot_count = 0;
  while (!eth_connected)
  {
    reboot_count++;
    if (reboot_count % 1000 == 0)
    {
      Serial.print(".");
      Serial.println(reboot_count);
    }
    delay(10);

    if (reboot_count > 20000)
    {
      rebootEspWithReason("failed ETH connection");
    }
  }

  x_serial_s = xSemaphoreCreateMutex();

  if (x_serial_s == NULL)
  {
    Serial.println("Mutex can not be created");
  }


  Serial.println("Init Modules");
  
  espClient.setInsecure();
  init_mqtt();
  init_rs485();
  init_io();
  init_devices();
  
#ifdef PAP_OCR_SUPPORT  
  init_OCR();
#endif

  xTaskCreate(task_mqtt, "MQTT", 10000, NULL, 1, &x_mqtt_handle);                 // MQTT Client

  while (!mqtt_connected)
  {
    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
  
  xTaskCreate(task_rs485, "RS485", 5000, NULL, 1, &x_rs485_handle);              // RS485
  xTaskCreate(task_poll_devices, "POLL", 10000, NULL, 1, &x_poll_devices_handle); // poll rs485 devices
  xTaskCreate(task_io_modul, "IO_MOD", 5000, NULL, 1, &x_io_mod_handle);         // io_modul
  xTaskCreate(task_watchdog, "WATCHDOG", 5000, NULL, 1, &x_io_watchdog_handle);  // watchdog
  
#ifdef PAP_OCR_SUPPORT   
  xTaskCreate(task_ocr, "OCR", 8096, NULL, 10, &ocr_task_handle); // MQTT client
#endif

#ifndef GWV2
  // Enable Status LED
  // port_exp_0.digitalWrite(4, HIGH);
#else
  // TODO!! TODO!! port_expander
#endif

}

void loop()
{
  delay(50);
  // do nothing
}
