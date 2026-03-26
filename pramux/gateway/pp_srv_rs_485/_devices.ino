uint32_t last_cmd_ms = 0;

void init_devices()
{
  Serial.println("init devices");

  for (int i = 0; i < NUM_DEVICES; i++)
  {
    devices[i].device_id = 0;
    devices[i].type = TYPE_NONE;
    devices[i].last_seen = 0;
    devices[i].last_cmd_ms = millis();
    devices[i].actor_addr = 0xFF;
    devices[i].last_cmd_try = 0;
    devices[i].file_upload = false;
    sprintf(devices[i].file_name, "");
    devices[i].file_size = 0;
    devices[i].file_offset = 0;
    devices[i].last_command = CMD_NONE;
    devices[i].last_command_payload = NULL;
    devices[i].last_command_payload_len = 0;
    devices[i].wait_for_answer = false;
    devices[i].client_prog_version = 0;
    devices[i].last_status = 0;
    devices[i].poll_interval = 0;
    devices[i].counter_count = 0;
    devices[i].counter_mod = 10;
    devices[i].last_seen_message = false;
    devices[i].io64_output_state = 0;
    devices[i].io64_outputs_defined = 0;

    for (int cnt = 0; cnt < 32; cnt++)
    {
      devices[i].counter_value[cnt] = 0;
      devices[i].counter_valid[cnt] = false;
      devices[i].gpios_available[cnt] = 0xFF;
      devices[i].gpios_state[cnt] = 0xFF;
    }

    devices[i].counter_count = 0;
    devices[i].wait_for_counter_data = false;

    devices[i].last_command_payload = (byte*)malloc(128 * sizeof(byte));
    if (devices[i].last_command_payload == NULL)
    {
      Serial.print("Fehler bei der Speicherzuweisung last_command. Device ");
      Serial.println(i);
    }

    // create command queue
    if (devices[i].x_device_command_queue = xQueueCreate(10, sizeof(_device_command)))
      if (NULL == devices[i].x_device_command_queue)
      {
        Serial.printf("error creating queue for device %d\n", i);
      }
  }

  // TODO: read device configuration from config file


  devices[0].device_id = 0x10;
  devices[0].type = PP_PAP_GAT_RS485;
  devices[0].last_seen = 0;
  devices[0].last_cmd_ms = millis();
  devices[0].counter_count = 1;
    
  devices[1].device_id = 0x11;
  devices[1].type = TYPE_NONE;
  devices[1].last_seen = 0;
  devices[1].last_cmd_ms = millis();
  devices[1].counter_count = 0;

  devices[2].device_id = 0x12;
  devices[2].type = TYPE_NONE;
  devices[2].last_seen = 0;
  devices[2].last_cmd_ms = millis();
  devices[2].counter_count = 0;

  devices[3].device_id = 0x13;
  devices[3].type = TYPE_NONE;
  devices[3].last_seen = 0;
  devices[3].last_cmd_ms = millis();
  devices[3].counter_mod = 1;
  devices[3].counter_count = 3;

  devices[4].device_id = 0x14;
  devices[4].type = TYPE_NONE;
  devices[4].last_seen = 0;
  devices[4].last_cmd_ms = millis();
  devices[4].counter_count = 0;

  devices[5].device_id = 0x15;
  devices[5].type = TYPE_NONE;
  devices[5].last_seen = 0;
  devices[5].last_cmd_ms = millis();
  devices[5].counter_count = 4;

  devices[6].device_id = 0x16;
  devices[6].type = TYPE_NONE;
  devices[6].last_seen = 0;
  devices[6].last_cmd_ms = millis();
  devices[6].counter_count = 4;

  devices[7].device_id = 0x17;
  devices[7].type = TYPE_NONE;
  devices[7].last_seen = 0;
  devices[7].last_cmd_ms = millis();
  devices[7].counter_count = 3;
  devices[7].counter_mod = 1;

  devices[8].device_id = 0x18;
  devices[8].type = TYPE_NONE;
  devices[8].last_seen = 0;
  devices[8].last_cmd_ms = millis();

  devices[9].device_id = 0x19;
  devices[9].type = TYPE_NONE;
  devices[9].last_seen = 0;
  devices[9].last_cmd_ms = millis();

  devices[10].device_id = 0x1A;
  devices[10].type = TYPE_NONE;
  devices[10].last_seen = 0;
  devices[10].last_cmd_ms = millis();

  devices[11].device_id = 0x1B;
  devices[11].type = TYPE_NONE;
  devices[11].last_seen = 0;
  devices[11].last_cmd_ms = millis();

  devices[12].device_id = 0x1C;
  devices[12].type = TYPE_NONE;
  devices[12].last_seen = 0;
  devices[12].last_cmd_ms = millis();

  poll_device_wait_for_answer = false;
  poll_device_last_device_id = 0;
  last_cmd_ms = millis();


  // init "last_seen_message"
  for (int i = 0; i < NUM_DEVICES; i++)
  {
    if (devices[i].type != TYPE_NONE && devices[i].device_id > 0)
    {
      char topic[128];
      sprintf(topic, "%s/%s/channel/%02d/device-state", mqtt_topic_praefix, mqtt_device_serial, i);
      publish(topic, "no answer", true);
      devices[i].last_seen_message = true;
    }
  }
  Serial.println("...devices initialized");
}

uint8_t get_device_num(uint8_t device_id)
{
  for (uint8_t i = 0; i < NUM_DEVICES; i++)
  {
    if (device_id == devices[i].device_id)
    {
      return i;
    }
  }

  return -1;
}

uint8_t get_device_num_actor(uint8_t actor_addr, _device_type type)
{
  for (uint8_t i = 0; i < NUM_DEVICES; i++)
  {
    if (actor_addr == devices[i].actor_addr && devices[i].type == type)
    {
      return i;
    }
  }
  return 0xFF;
}

uint8_t get_device_num(_device_type type)
{
  for (uint8_t i = 0; i < NUM_DEVICES; i++)
  {
    if (devices[i].type == type)
    {
      return i;
    }
  }
  return 0xFF;
}

bool set_next_command(uint8_t device, _commands command, byte* payload, uint16_t payload_len, bool send_to_back = true)
{
  if (TYPE_NONE == devices[device].type)
  {
    Serial.println("device not configured");
    return false;
  }

  _device_command cmd;
  memset(cmd.next_command_payload, 0x0, 128);
  cmd.next_command = command;
  memcpy(cmd.next_command_payload, payload, payload_len);
  cmd.next_command_payload_len = payload_len;

  if (payload_len == 0)
  {
    memset(cmd.next_command_payload, 0x0, 128);

  }

  Serial.printf("queue command %d for device %d\n", command, device);

  if (send_to_back)
  {
    if (xQueueSendToBack(devices[device].x_device_command_queue, &cmd, 0) == pdTRUE)
    {
      return true;
    }
    else
    {
      Serial.printf("failed to queue next command for device %d", device);
    }
  }
  else
  {
    if (xQueueSendToFront(devices[device].x_device_command_queue, &cmd, 0) == pdTRUE)
    {
      return true;
    }
    else
    {
      Serial.printf("failed to queue next command for device %d", device);
    }
  }

  return false;
}

void task_poll_devices(void *param)
{
  _device_command cmd;

  for (;;)
  {
    for (int i = 0; i < NUM_DEVICES; i++)
    {
      if (devices[i].type != TYPE_NONE && devices[i].device_id > 0)
      {
        if (devices[i].last_seen > 0)
        {
          if (millis() > devices[i].last_seen + 2500)
          {
            if (!devices[i].last_seen_message)
            {
              // device maybe offline
              char topic[128];
              sprintf(topic, "%s/%s/channel/%02d/device-state", mqtt_topic_praefix, mqtt_device_serial, i);
              publish(topic, "no answer", true);
              devices[i].last_seen_message = true;
            }
          }
          else
          {
            if (devices[i].last_seen_message)
            {
              char topic[128];
              sprintf(topic, "%s/%s/channel/%02d/device-state", mqtt_topic_praefix, mqtt_device_serial, i);
              publish(topic, "ok", true);
              devices[i].last_seen_message = false;
            }
          }
        }

        // wait for poll interval (global or specific)
        // global
        while (last_cmd_ms + DEVICES_POLL_INTERVAL > millis())
        {
          vTaskDelay(1 / portTICK_PERIOD_MS);
        }

        // specific
        if (devices[i].last_cmd_ms + devices[i].poll_interval > millis())
        {
          continue;
        }

        // get device specific next_command
        if ( xQueueReceive( devices[i].x_device_command_queue, &cmd, 0) != pdTRUE)
        {
          // empty queue send alive
          cmd.next_command = CMD_ALIVE;
          memset(cmd.next_command_payload, 0x0, 128);
          cmd.next_command_payload_len = 0;
        }

        // poll device
        if (cmd.next_command == CMD_ALIVE)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x31;
          msg.payload = NULL;
          msg.len_payload = 0;
          rs485.send(msg);

          // save command
          devices[i].last_command = CMD_ALIVE;
          devices[i].last_command_payload_len = 0;
        }

        // open gate
        else if (cmd.next_command == CMD_OPENGATE)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x57;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          Serial.print("--> send command ");

          // save command
          devices[i].last_command = cmd.next_command;


          Serial.print(" --> nach free last_command_payload ");
          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          Serial.print(" --> bevor next command CMD_STATUS_REQUEST ");

          // Next command = CMD_STATUS_REQUEST
          //devices[i].next_command = CMD_STATUS_REQUEST;
          //devices[i].next_command_payload[0] = 0x55;
          //devices[i].next_command_payload_len = 1;
          byte *payload;
          payload = (byte*)malloc(1);
          payload[0] = 0x55;
          set_next_command(i, CMD_STATUS_REQUEST, payload, 1, false);
          free(payload);
          Serial.print(" --> after next command CMD_STATUS_REQUEST ");

          Serial.println(" CMD_OPENGATE");
        }

        else if (cmd.next_command == CMD_STATUS_REQUEST)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x50;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = 1;
          rs485.send(msg);

          //Serial.println("--> send command");

          // save command
          devices[i].last_command = cmd.next_command;

          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;


          // don't reset next_command. reset of next_command depends on the answer

        }

        // get uptime
        else if (cmd.next_command == CMD_GETUPTIME)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x35;
          msg.payload = NULL;
          msg.len_payload = 0;
          rs485.send(msg);

          Serial.println("--> send command GET_UPTIME");

          // save command
          devices[i].last_command = cmd.next_command;
          devices[i].last_command_payload_len = 0;

          // reset next command
          // devices[i].next_command = CMD_ALIVE;
          // devices[i].next_command_payload_len = 0;
        }

        // set led
        else if (cmd.next_command == CMD_SET_LED)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x33;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          Serial.println("--> send command SET_LED");

          // save command
          devices[i].last_command = cmd.next_command;

          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          //devices[i].next_command = CMD_ALIVE;
          //devices[i].next_command_payload_len = 0;

        }
        // set led_default
        else if (cmd.next_command == CMD_SET_LED_DEFAULT)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x34;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          Serial.println("--> send command SET_LED_DEFAULT");

          // save command
          devices[i].last_command = cmd.next_command;

          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          //devices[i].next_command = CMD_ALIVE;
          //devices[i].next_command_payload_len = 0;

        }

        // set tft image
        else if (cmd.next_command == CMD_SET_TFT_IMAGE)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x61;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = 1;
          rs485.send(msg);

          // save command
          devices[i].last_command = cmd.next_command;

          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          //devices[i].next_command = CMD_ALIVE;
          //devices[i].next_command_payload_len = 0;

          Serial.println("--> send command CMD_SET_TFT_IMAGE");
        }

        // set tft image text
        else if (cmd.next_command == CMD_TFT_TEXT_TO_IMG)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x62;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          // save command
          devices[i].last_command = cmd.next_command;


          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          //devices[i].next_command = CMD_ALIVE;
          //devices[i].next_command_payload_len = 0;

          Serial.println("--> send command CMD_TFT_TEXT_TO_IMG");
        }
        else if (cmd.next_command == CMD_MORE_MOVE)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x63;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          // save command
          devices[i].last_command = cmd.next_command;


          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          //devices[i].next_command = CMD_ALIVE;
          //devices[i].next_command_payload_len = 0;

          Serial.println("--> send command CMD_MORE_MOVE");

        }
        else if (cmd.next_command == CMD_DISABLE)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x40;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          // save command
          devices[i].last_command = cmd.next_command;


          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          // devices[i].next_command = CMD_ALIVE;
          // devices[i].next_command_payload_len = 0;

          Serial.println("--> send command CMD_DISABLE");
        }
        else if (cmd.next_command == CMD_SET_COUNTER_VALUE)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x64;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          // save command
          devices[i].last_command = cmd.next_command;


          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          // devices[i].next_command = CMD_ALIVE;
          // devices[i].next_command_payload_len = 0;

          devices[i].wait_for_counter_data = false;

          Serial.println("--> send command CMD_SET_COUNTER_VALUE");
        }
        else if (cmd.next_command == CMD_SET_GPIO)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x56;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          // save command
          devices[i].last_command = cmd.next_command;


          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          // reset next command
          // devices[i].next_command = CMD_ALIVE;
          // devices[i].next_command_payload_len = 0;

          Serial.println("--> send command CMD_SET_GPIO");
        }
        else if (cmd.next_command == CMD_RESET_DEVICE)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x65;
          msg.len_payload = 0;
          rs485.send(msg);

          // save command
          devices[i].last_command = cmd.next_command;


          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          Serial.println("--> send command CMD_RESET_DEVICE");
        }
        else if (cmd.next_command == CMD_IO64_SETALL)
        {
          _rs485message msg;
          msg.devaddr = devices[i].device_id;
          msg.command = 0x58;
          msg.payload = cmd.next_command_payload;
          msg.len_payload = cmd.next_command_payload_len;
          rs485.send(msg);

          memcpy(devices[i].last_command_payload, cmd.next_command_payload, cmd.next_command_payload_len);
          devices[i].last_command_payload_len = cmd.next_command_payload_len;

          Serial.println("--> send command CMD_IO64_SETALL");
        }
        else
        {
          // no command. do nothing
          continue;
        }

        poll_device_wait_for_answer = true;
        devices[i].wait_for_answer = true;
        poll_device_last_device_id = devices[i].device_id;
        devices[i].last_cmd_ms = millis();
        last_cmd_ms = millis();

        // wait for answer
        bool no_answer = false;
        while (poll_device_wait_for_answer)
        {
          // wait 100ms for answer.
          if (devices[i].last_cmd_ms + 100 < millis())
          {
            if (devices[i].last_command != CMD_ALIVE)
            {
              Serial.print("got no answer: ");
              Serial.println(poll_device_last_device_id, HEX);
              Serial.print("last command: ");
              Serial.println(devices[i].last_command);
            }

            no_answer = true;
            break;
          }
        }

        //if (!no_answer || devices[i].last_seen == 0)
        if (!no_answer)
        {
          // got respone from device
          devices[i].last_seen = millis();
        }

        if (no_answer && (devices[i].last_command == CMD_SET_LED
                          || devices[i].last_command == CMD_SET_TFT_IMAGE
                          || devices[i].last_command == CMD_OPENGATE
                          || devices[i].last_command == CMD_STATUS_REQUEST
                          || devices[i].last_command == CMD_MORE_MOVE
                          || devices[i].last_command == CMD_DISABLE
                          || devices[i].last_command == CMD_SET_COUNTER_VALUE
                          || devices[i].last_command == CMD_RESET_DEVICE
                          || devices[i].last_command == SET_GPOI))
        {

          no_answer = false;

          if (devices[i].last_cmd_try < 3)
          {
            Serial.print("device ");
            Serial.print(poll_device_last_device_id, HEX);
            Serial.println(" - repeat last command");

            //devices[i].next_command = devices[i].last_command;
            //memcpy(devices[i].next_command_payload, devices[i].last_command_payload, devices[i].last_command_payload_len);
            //devices[i].next_command_payload_len = devices[i].last_command_payload_len;

            // queue last_command again
            set_next_command(i, devices[i].last_command, devices[i].last_command_payload, devices[i].last_command_payload_len, false);

            devices[i].last_cmd_try++;
          }
          else
          {
            Serial.print("device ");
            Serial.print(poll_device_last_device_id, HEX);
            Serial.println(" - no answer - giving up");
            devices[i].last_cmd_try = 0;
            //reset next command
            //devices[i].next_command = CMD_ALIVE;
            //devices[i].next_command_payload_len = 0;
          }
        }
        else
        {
          devices[i].last_cmd_try = 0;
        }
      }
    }

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}
