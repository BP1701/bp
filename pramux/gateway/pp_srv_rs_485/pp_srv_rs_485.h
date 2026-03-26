#ifndef __PP_SRV_RS_485__
#define __PP_SRV_RS_485__

enum _device_type
{
  TYPE_NONE,          // 00 - not defined
  PP_SEN_RID_RS485,   // 01 - sensor with RFID Reader
  PP_ACT_REL_RS485,   // 02 - remote RS485 relay
  PP_ACT_GAT_RS485,   // 03 - gate controller
  PP_ACT_PSS_RS485,   // 04 - power supply station
  PP_ACT_REL_INT,     // 05 - internal output
  PP_SEN_CNT_RS485,   // 06 - RS485 counter
  PP_SEN_CNT_INT,     // 07 - internal counter
  PP_SEN_CUR_R485,    // 08 - current meter
  PP_ACT_EV_CS,       // 09 - EV Charging Station
  PP_SEN_COM_RS485,   // 0A - combi sensor (rfid / scanner / motoreader / pinpad)
  PP_SEN_SCN_RS485,   // 0B - sensor with Barcode-Scanner
  PP_SEN_PIN_RS485,   // 0C - sensor with PIN-Pad
  PP_64IO_RS485,      // 0D - 64 Bit IO-Modul (32 Input / 32 Output)
  PP_SEN_INP_INT,     // 0E - internal input
  PP_ACT_ASS_RS485,   // 0F - aqua supply station
  PP_PAP_GAT_RS485    // 10 - park und power gate
};

enum _commands
{
  CMD_ALIVE,
  CMD_OPENGATE,
  CMD_GETUPTIME,
  CMD_SET_LED,
  CMD_SET_LED_DEFAULT,
  CMD_REQUEST_FILE_UPLOAD,
  CMD_FILE_UPLOAD,
  CMD_SET_TFT_IMAGE,
  CMD_TFT_TEXT_TO_IMG,
  CMD_STATUS_REQUEST,
  CMD_MORE_MOVE,
  CMD_DISABLE,
  CMD_SET_GPIO,
  CMD_SET_COUNTER_VALUE,
  CMD_RESET_GATEWAY,
  CMD_RESET_DEVICE,
  CMD_IO64_SETALL,
  CMD_NONE
};

struct _device
{
  uint8_t device_id;
  _device_type type;
  uint32_t last_seen;
  uint32_t last_cmd_ms;
  uint8_t actor_addr;
  uint8_t last_cmd_try;
  bool file_upload;
  char file_name[32];
  int file_size;
  int file_offset;
  _commands last_command;
  byte *last_command_payload;
  uint8_t last_command_payload_len;
  bool wait_for_answer;
  byte client_prog_version;
  uint8_t last_status;
  int poll_interval;
  int counter_value[32];
  uint8_t counter_count;
  uint8_t counter_mod;
  uint8_t gpios_available[32];
  uint8_t gpios_state[32];
  bool last_seen_message;
  bool wait_for_counter_data;
  bool counter_valid[32];
  uint32_t io64_output_state;
  uint8_t io64_outputs_defined;
  
  QueueHandle_t x_device_command_queue;
};

struct _device_command
{
  _commands next_command;
  byte next_command_payload[128];
  uint8_t next_command_payload_len;
};

#endif
