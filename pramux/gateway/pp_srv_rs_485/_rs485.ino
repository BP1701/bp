uint8_t last_gatestatus = 0;
_device_command cmd;

void init_rs485()
{
  rs485.onEvent(rs485Callback);
  rs485.set_log_enabled(true);
  rs485.begin(1, 38400);
}

void rs485Callback(_rs485message event)
{
#ifdef _DEBUG
  char msg[2048];
  //log("RS485 Message:");
  //sprintf(msg, "Adresse: 0x%02x\tCommand: 0x%02x\tPayload: %s\tLänge: %d, Progversion: 0x%02x",
  //    event.devaddr, event.command, event.payload, event.len_payload, event.payload[0]);
  //log(msg);
#endif

  // ACK received
  if (0x06 == event.command)
  {
    // event.len_payload = 0 = old version
    // event.len_payload = 1 = new version --> event.payload[0] client_prog_versionv (0x01 - 0xFF)

    if (event.len_payload > 0)
    {
      devices[get_device_num(event.devaddr)].client_prog_version = event.payload[0];
    }

    if (event.len_payload > 1)
    {
      uint8_t status = event.payload[1];
      if (status != devices[get_device_num(event.devaddr)].last_status)
      {
        devices[get_device_num(event.devaddr)].last_status = status;

        // gate status
        if (devices[get_device_num(event.devaddr)].type == PP_ACT_GAT_RS485)
        {
          if (status & 0x01 && !(status & 0x02) )
          {
            // gate is disabled
            char topic[128];
            char mqtt_payload[32];
            sprintf(topic, "%s/%s/channel/%02d/state", mqtt_topic_praefix, mqtt_device_serial, get_device_num(event.devaddr));
            sprintf(mqtt_payload, "state=disabled");
            publish(topic, mqtt_payload, true);
          }

          if (!(status & 0x01))
          {
            // gate is enabled
            char topic[128];
            char mqtt_payload[32];
            sprintf(topic, "%s/%s/channel/%02d/state", mqtt_topic_praefix, mqtt_device_serial, get_device_num(event.devaddr));
            sprintf(mqtt_payload, "state=enabled");
            publish(topic, mqtt_payload, true);
          }

          if (status & 0x02)
          {
            // gate undefined
            char topic[128];
            char mqtt_payload[32];
            sprintf(topic, "%s/%s/channel/%02d/state", mqtt_topic_praefix, mqtt_device_serial, get_device_num(event.devaddr));
            sprintf(mqtt_payload, "state=undefined");
            publish(topic, mqtt_payload, true);
          }
        }
        // power supply station status
        if (devices[get_device_num(event.devaddr)].type == PP_ACT_PSS_RS485 || devices[get_device_num(event.devaddr)].type == PP_ACT_ASS_RS485 || devices[get_device_num(event.devaddr)].type == PP_ACT_EV_CS || devices[get_device_num(event.devaddr)].type == PP_ACT_GAT_RS485)
        {
          Serial.printf("ACK status = %d\n", status);
          if (status & 0x02)
          {
            // slave power meter value undefined
            // send stored value if available. Otherwise wait for mqtt power meter value
            uint8_t novalue = 0;
            for (int i = 0; i < devices[get_device_num(event.devaddr)].counter_count; i ++)
            {
              if (devices[get_device_num(event.devaddr)].counter_valid[i] == false)
              {
                novalue++;
              }
            }

            if (novalue == 0)
            {
              uint8_t num = get_device_num(event.devaddr);
              uint8_t cnt = devices[get_device_num(event.devaddr)].counter_count;
              memset(cmd.next_command_payload, 0x0, 128);
              for (int i = 0; i < cnt; i++)
              {
                Serial.printf("sending value for pm %d value %d\n", i, devices[num].counter_value[i]);
                cmd.next_command_payload[0 + (i * 4)] = (uint8_t)((devices[num].counter_value[i] & 0xFF000000) >> 24);
                cmd.next_command_payload[1 + (i * 4)] = (uint8_t)((devices[num].counter_value[i] & 0xFF0000) >> 16);
                cmd.next_command_payload[2 + (i * 4)] = (uint8_t)((devices[num].counter_value[i] & 0xFF00) >> 8);
                cmd.next_command_payload[3 + (i * 4)] = (uint8_t)((devices[num].counter_value[i] & 0xFF));

              }

              cmd.next_command_payload_len = 4 * cnt;
              cmd.next_command = CMD_SET_COUNTER_VALUE;

              set_next_command(num, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
            }
            else
            {
              Serial.println("no counter values available");

              // TEST_SG
              // subscribe counter values
              char topic[128];
              byte payload[128];
              if (!devices[get_device_num(event.devaddr)].wait_for_counter_data && client.connected())
              {
                for (int pmc = 0; pmc < devices[get_device_num(event.devaddr)].counter_count; pmc++)
                {
                  sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, get_device_num(event.devaddr), pmc);
                  Serial.print("Subscribe: ");
                  Serial.println(topic);
                  client.subscribe(topic);
                  devices[get_device_num(event.devaddr)].wait_for_counter_data = true;
                }
              }

            }
          }
          /*
            if (status & 0x04)
            {
            // slave gpio state undefined
            // request state
            char topic[128];
            char mqtt_payload[32];
            sprintf(topic, "prs485gw/%s/channel/%02d/state", mqtt_device_serial, get_device_num(event.devaddr));
            sprintf(mqtt_payload, "gpios=undefined");
            publish(topic, mqtt_payload, true);
            }
          */
        }
      }
    }
  }

  if (0x32 == event.command)
  {
    int device_num = get_device_num(event.devaddr);
    char msg[128];
    if (event.payload[0] != DATASOURCE_IO64_COUNTER_TRIGGER)
    {
      log("RS485 message with payload:");
      sprintf(msg, "Adresse: 0x%02x\tCommand: 0x%02x\tPayload: %s\tLänge: %d Source 0x%02x Version: 0x%02x",
              event.devaddr, event.command, event.payload, event.len_payload, event.payload[0], devices[device_num].client_prog_version);
      log(msg);
    }
    // publish mqtt event
    char topic[128];
    char mqtt_payload[32];

    if (device_num >= 0)
    {
      sprintf(topic, "%s/%s/channel/%02d/out", mqtt_topic_praefix, mqtt_device_serial, device_num);
      if (devices[device_num].client_prog_version == 0x0)
      {
        Serial.println("0x00");
        sprintf(mqtt_payload, "rfid=%s", event.payload + 1);
        Serial.println(mqtt_payload);
      }
      else
      {
        if (event.payload[0] != DATASOURCE_IO64_COUNTER_TRIGGER)
        {
          Serial.print("prepare mqtt payload - version = ");
          Serial.println(devices[device_num].client_prog_version);
          Serial.print("event.payload");
          Serial.println(event.payload[0]);
        }

        switch (event.payload[0])
        {
          case DATASOURCE_RFID_FEIG:
            {
              if (devices[device_num].type != PP_PAP_GAT_RS485)
              {
                sprintf(mqtt_payload, "rfid=%s", event.payload + 1);        // RFID Feig reader
                Serial.println(mqtt_payload);
              }
              else
              {
                sprintf(topic, "%s/%s/channel/%02d/out/rfid", mqtt_topic_praefix, mqtt_device_serial, device_num);
                sprintf(mqtt_payload, "%s", event.payload + 1);            // RFID Feig reader
                Serial.println(mqtt_payload);
              }

              break;
            }
          case DATASOURCE_RFID_QRT310:
            {
              sprintf(mqtt_payload, "more=%s", event.payload + 1);        // RFID motor reader
              break;
            }
          case DATADOURCE_SCAN_OPTI:
            {
              if (devices[device_num].type != PP_PAP_GAT_RS485)
              {
                sprintf(mqtt_payload, "scan=%s", event.payload + 1);        // Barcode Opticon scanner
                Serial.println(mqtt_payload);
              }
              else
              {
                sprintf(topic, "%s/%s/channel/%02d/out/scan", mqtt_topic_praefix, mqtt_device_serial, device_num);
                sprintf(mqtt_payload, "%s", event.payload + 1);            // // Barcode Opticon scanner
                Serial.println(mqtt_payload);
              }
              break;
            }          
          case DATASOURCE_PINPAD:
            {
              sprintf(mqtt_payload, "pin=%s", event.payload + 1);        // Barcode Opticon scanner
              break;
            }
          case DATASOURCE_COIN_EMP800:
            {
              sprintf(mqtt_payload, "coin=%s", event.payload + 1);        // Barcode Opticon scanner
              break;
            }
          case DATASOURCE_COUNTER:
            {
              // change topic
              sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, device_num, event.payload[1]);

              // mqtt payload
              uint32_t tmp_value = 0;
              tmp_value += event.payload[2]; tmp_value <<= 8;
              tmp_value += event.payload[3]; tmp_value <<= 8;
              tmp_value += event.payload[4]; tmp_value <<= 8;
              tmp_value += event.payload[5];

              if ((tmp_value % devices[device_num].counter_mod) == 0 || ((devices[device_num].type != PP_ACT_PSS_RS485) && (devices[device_num].type != PP_ACT_ASS_RS485)))
              {
                sprintf(mqtt_payload, "value=%4d", tmp_value);
              }
              else
              {
                mqtt_payload[0] = 0x0;
              }

              // store value local
              Serial.printf("store value %d for channel %d, pm %d\n", tmp_value, device_num, event.payload[1]);
              devices[device_num].counter_value[event.payload[1]] = tmp_value;

              break;
            }
          case DATASOURCE_AVAIABLE_GPIOS:
            {
              uint8_t gpio_count = 0;

              bool update_mqtt = false;
              uint8_t dev_count = event.payload[1];
              if (dev_count & 0x80)
              {
                Serial.println("update mqtt gpio state");
                // delete update flag
                dev_count &= ~(0x80);
                update_mqtt = true;
                Serial.print("dev count: ");
                Serial.println(dev_count);
              }


              for (uint8_t i = 2; i < (dev_count * 2) + 2; i++)
              {
                // change topic
                sprintf(topic, "%s/%s/channel/%02d/gpios/%02d", mqtt_topic_praefix, mqtt_device_serial, device_num, event.payload[i]);
                Serial.println(topic);


                // store gpio
                devices[device_num].gpios_available[gpio_count++] = event.payload[i];

                if (!update_mqtt)
                {
                  sprintf(mqtt_payload, "state=%d", event.payload[++i]);
                  publish(topic, mqtt_payload, true);
                  update_mqtt = false;
                }
                else
                {
                  client.subscribe(topic);
                  // skip value
                  i++;
                }
              }
              break;
            }
          case DATASOURCE_INPUT_GPIOS:
            {
              uint8_t dev_count = event.payload[1] / 2;

              for (int i = 0; i < dev_count; i++)
              {
                Serial.print("gpio: ");
                Serial.print(event.payload[2 + (i * 2)]);
                Serial.print(" state: ");
                Serial.println(event.payload[2 + (i * 2) + 1]);
                // change topic
                sprintf(topic, "%s/%s/channel/%02d/input/%02d", mqtt_topic_praefix, mqtt_device_serial, device_num, event.payload[2 + (i * 2)]);
                Serial.println(topic);
                sprintf(mqtt_payload, "state=%d", event.payload[2 + (i * 2) + 1]);
                Serial.println(mqtt_payload);
                publish(topic, mqtt_payload, true);

                if(devices[get_device_num(event.devaddr)].type == PP_PAP_GAT_RS485)
                {
                  
                  if((event.payload[2 + (i * 2)]) == PAP_GATE_STATE_GPIO)
                  {
                    sprintf(topic, "%s/%s/channel/%02d/out/open", mqtt_topic_praefix, mqtt_device_serial, device_num);
                    Serial.println(topic);
                    sprintf(mqtt_payload, "%s", event.payload[2 + (i * 2) + 1] ? "false" : "true");
                    Serial.println(mqtt_payload);
                    publish(topic, mqtt_payload, true);
                  }
                  if((event.payload[2 + (i * 2)]) == PAP_PRESENCE_STATE_GPIO)
                  {
                    sprintf(topic, "%s/%s/channel/%02d/out/presence", mqtt_topic_praefix, mqtt_device_serial, device_num);
                    Serial.println(topic);
                    sprintf(mqtt_payload, "%s", event.payload[2 + (i * 2) + 1] ? "true" : "false");
                    Serial.println(mqtt_payload);
                    publish(topic, mqtt_payload, true);
                  }
                }                 
              }
            }
          case DATASOURCE_IO64_COUNTER_TRIGGER:
            {
              //Serial.printf("received io64 counter trigger mask %02x%02x%02x%02x", event.payload[1], event.payload[2], event.payload[3], event.payload[4]);
              // parsing payload into a long value
              uint32_t tmp_value = 0;
              tmp_value += event.payload[1]; tmp_value <<= 8;
              tmp_value += event.payload[2]; tmp_value <<= 8;
              tmp_value += event.payload[3]; tmp_value <<= 8;
              tmp_value += event.payload[4];

              // evaluate values
              //Serial.println("found trigger on:");
              for (int c = 0; c < 32; c++)
              {
                if (tmp_value & (1 << c))
                {
                  //Serial.printf(" %d ", c);
                  devices[device_num].counter_value[c] += 10;

                  // do not transmit every value
                  //if ((devices[device_num].counter_value[c] % 10) == 0)
                  //{
                  sprintf(topic, "%s/%s/channel/%02d/io64/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, device_num, c);
                  sprintf(mqtt_payload, "value=%d", devices[device_num].counter_value[c]);
                  //}
                  //else
                  //{
                  //  mqtt_payload[0] = 0x0;
                  //}

                }
              }
              //Serial.println();
              break;
            }
          case DATASOURCE_IO64_OUTPUT_DATA:
            {
              if (event.payload[1] != 0xFF)
              {
                uint8_t gpio = event.payload[1];
                uint8_t state = event.payload[2];
                sprintf(topic, "%s/%s/channel/%02d/io64/output/%02d", mqtt_topic_praefix, mqtt_device_serial, device_num, gpio);
                sprintf(mqtt_payload, "state=%d", state);

                if (state)
                {
                  devices[device_num].io64_output_state |= (1 << gpio);
                }
                else
                {
                  devices[device_num].io64_output_state &= ~(1 << gpio);
                }
              }
              else
              {
                // subscribe gpios
                mqtt_payload[0] = 0x0;
                if (devices[device_num].io64_outputs_defined < 32)
                {
                  sprintf(topic, "%s/%s/channel/%02d/io64/output/#", mqtt_topic_praefix, mqtt_device_serial, device_num);
                  client.subscribe(topic);
                  Serial.printf("subscribe topic %s\n", topic);
                }
                else
                {
                  Serial.printf("io64 outputs alredy defined ... sending data to client ");
                  Serial.printf("%ld\n", devices[device_num].io64_output_state);

                  cmd.next_command_payload[0] = (uint8_t)((devices[device_num].io64_output_state & 0xFF000000) >> 24);
                  cmd.next_command_payload[1] = (uint8_t)((devices[device_num].io64_output_state & 0xFF0000) >> 16);
                  cmd.next_command_payload[2] = (uint8_t)((devices[device_num].io64_output_state & 0xFF00) >> 8);
                  cmd.next_command_payload[3] = (uint8_t)((devices[device_num].io64_output_state & 0xFF));

                  cmd.next_command_payload_len = 4;
                  cmd.next_command = CMD_IO64_SETALL;

                  set_next_command(device_num, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
                }
              }
              break;
            }
          default:
            {
              sprintf(mqtt_payload, "id=%s", event.payload + 1);          // Misc ID
              break;
            }
        }
      }
      if (event.payload[0] != DATASOURCE_INPUT_GPIOS)
      {
        if (event.payload[0] != DATASOURCE_COUNTER && event.payload[0] != DATASOURCE_IO64_COUNTER_TRIGGER && event.payload[0] != DATASOURCE_IO64_OUTPUT_DATA)
        {
          if (event.payload[0] != DATASOURCE_AVAIABLE_GPIOS)
          {
            publish(topic, mqtt_payload);
          }
        }
        else
        {
          Serial.print("len mqtt_payload ");
          Serial.println(strlen(mqtt_payload));

          if (strlen(mqtt_payload) > 0)
          {
            publish(topic, mqtt_payload, true);
          }
        }
      }
    }
  }
  if (0x36 == event.command)
  {
    char msg[64];
    sprintf(msg, "Uptime device 0x%02x: 0x%02x 0x%02x 0x%02x 0x%02x", event.devaddr, event.payload[0], event.payload[1], event.payload[2], event.payload[3]);
    log(msg);
  }

  if (0x51 == event.command)
  {
    char msg[64];
    sprintf(msg, "prog_version=%d;app_version=%d;uptime=%02x%02x%02x%02x", event.payload[0], event.payload[1], event.payload[2], event.payload[3], event.payload[4], event.payload[5]);
    log(msg);
    // publish mqtt event
    char topic[128];
    char mqtt_payload[32];

    sprintf(topic, "%s/%s/channel/%02d/out", mqtt_topic_praefix, mqtt_device_serial, get_device_num(event.devaddr));
    publish(topic, msg);
  }

  if (0x55 == event.command)
  {
    // possible answers
    // 0 - Gate is closed
    // 1 - Gate is open
    // 2 - Gate is closed after passage
    // 3 - Gate is closed. No passage detected

    bool delete_status_request = false;

    Serial.print("0x55 Status-Gate: ");
    Serial.println(event.payload[0], HEX);

    if (event.payload[0] == 0)
    {
      if (1 == last_gatestatus)
      {

        //Serial.print("0x55 Status-Gate: ");
        //Serial.print(event.payload[0], HEX);
        Serial.println(" --> exit with passage=false");
        event.payload[0] = 3;
      }
    }
    if (event.payload[0] == 1)
    {
      if (last_gatestatus != 1)
      {
        //Serial.print("0x55 Status-Gate: ");
        //Serial.println(event.payload[0], HEX);
      }
      last_gatestatus = (uint8_t)event.payload[0];
    }
    if (event.payload[0] == 2)
    {
      //Serial.print("0x55 Status-Gate: ");
      //Serial.println(event.payload[0], HEX);
      Serial.println("Status: passage=true");
      delete_status_request = true;
      // publish mqtt event
      char topic[128];
      char mqtt_payload[32];
      int device_num = get_device_num(event.devaddr);
      if (device_num >= 0)
      {
        sprintf(topic, "%s/%s/channel/%02d/out", mqtt_topic_praefix, mqtt_device_serial, device_num);
        sprintf(mqtt_payload, "passage=true");
        publish(topic, mqtt_payload);
      }

      last_gatestatus = 0;
    }
    if (event.payload[0] == 3)
    {
      //Serial.print("0x55 Status-Gate: ");
      //Serial.println(event.payload[0], HEX);
      Serial.println("Status: passage=false");
      delete_status_request = true;
      // publish mqtt event
      char topic[128];
      char mqtt_payload[32];
      int device_num = get_device_num(event.devaddr);
      if (device_num >= 0)
      {
        sprintf(topic, "%s/%s/channel/%02d/out", mqtt_topic_praefix, mqtt_device_serial, device_num);
        sprintf(mqtt_payload, "passage=false");
        publish(topic, mqtt_payload);
      }

      last_gatestatus = 0;
    }

    if (!delete_status_request)
    {
      // schedule new status requeat
      cmd.next_command = CMD_STATUS_REQUEST;
      cmd.next_command_payload[0] = 0x55;
      cmd.next_command_payload_len = 1;
      set_next_command(get_device_num(event.devaddr), cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len, false);
    }
  }

  if (poll_device_last_device_id == event.devaddr)
  {
    poll_device_wait_for_answer = false;
    devices[get_device_num(event.devaddr)].wait_for_answer = false;
  }
}

// rs485 task
void task_rs485(void *param)
{
  for (;;)
  {
    while (wait_for_update)
    {
      Serial.println("   --> task rs485 suspended");
      x_task_rs485_suspended = true;
      vTaskSuspend(x_rs485_handle);
    }

    rs485.receive();


    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}
