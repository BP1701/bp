void get_next_mqtt_payload()
{
  _mqtt_payload p_mqtt_payload;
  _device_command cmd;

  if ( xQueueReceive( x_mqtt_payload_q, &p_mqtt_payload, (TickType_t) 0) != pdPASS)
  {
    return;
  }

  // copy topic;
  char topic_cpy[128];
  sprintf(topic_cpy, p_mqtt_payload.topic);

  // parse channel if available
  int channel;
  if (strstr(p_mqtt_payload.topic, "channel"))
  {
    channel = parse_channel(p_mqtt_payload.topic);
  }
  else
  {
    channel = -1;
  }

  /*
    if (-1 != channel && cmd.wait_for_answer == true)
    {

      if (xQueueSendToFront(x_mqtt_payload_q, &p_mqtt_payload, 512) != pdTRUE)
      {
        Serial.println("Device is busy. re-queueing message");
      }
    }
  */

  char *c_payload;


  c_payload = (char*)malloc(p_mqtt_payload.length + 1);

  int i;
  for (i = 0; i < p_mqtt_payload.length; i++)
  {
    c_payload[i] = (char)p_mqtt_payload.payload[i];
  }
  c_payload[i] = 0x0;

  // remove linefeed
  if (c_payload[i - 1] == '\n')
  {
    c_payload[i - 1] = 0x0;
  }


  if (strcmp("init", c_payload) == 0)
  {
    return;
  }

  Serial.println("parse next MQTT payload");

  Serial.print("Message arrived [");
  Serial.print(topic_cpy);
  Serial.print("] ");

  Serial.print("Payload: ");
  Serial.println(c_payload);
  // Serial.print(" ");



  parse_params(c_payload);

  // parse retained counter value for PP_ACT_PSS_RS485
  if (devices[channel].type == PP_ACT_PSS_RS485 || devices[channel].type == PP_ACT_ASS_RS485 || devices[channel].type == PP_SEN_CNT_INT ||
      devices[channel].type == PP_ACT_EV_CS || devices[channel].type == PP_64IO_RS485 || devices[channel].type == PP_ACT_GAT_RS485) 
  {
    if (strstr(topic_cpy, "/counter"))
    {
      uint8_t pm_num;

      if (mqtt_params.contains("value"))
      {
        client.unsubscribe(topic_cpy);
        Serial.print("unsubscribe: ");
        Serial.println(topic_cpy);

        // parse counter num
        if (devices[channel].type == PP_64IO_RS485)
        {
          pm_num = parse_subchannel(topic_cpy, 6);
        }
        else
        {
          pm_num = parse_subchannel(topic_cpy, 5);
        }

        if (!devices[channel].counter_valid[pm_num])
        {
          Serial.printf("adding received counter value... %d\n", pm_num);
          devices[channel].counter_value[pm_num] = atol(mqtt_params["value"]);
          devices[channel].counter_valid[pm_num] = true;

          // just process new values
          if (devices[channel].type == PP_SEN_CNT_INT || devices[channel].type == PP_64IO_RS485)
          {
            return;
          }
        }
        else
        {
          return;
        }
      }
      else
      {
        return;
      }

      Serial.print("received counter value for");
      Serial.print(" channel=");
      Serial.print(channel);
      Serial.print(" counter =");
      Serial.print(pm_num);
      Serial.print(" value=");
      Serial.print(devices[channel].counter_value[pm_num]);
      Serial.println();



      cmd.next_command_payload[0] = (uint8_t)pm_num;
      cmd.next_command_payload[1] = (uint8_t)((devices[channel].counter_value[pm_num] & 0xFF000000) >> 24);
      cmd.next_command_payload[2] = (uint8_t)((devices[channel].counter_value[pm_num] & 0xFF0000) >> 16);
      cmd.next_command_payload[3] = (uint8_t)((devices[channel].counter_value[pm_num] & 0xFF00) >> 8);
      cmd.next_command_payload[4] = (uint8_t)((devices[channel].counter_value[pm_num] & 0xFF));

      cmd.next_command_payload_len = 5;
      cmd.next_command = CMD_SET_COUNTER_VALUE;

      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);

      free(c_payload);
      return;
    }
  }

  if (devices[channel].type == PP_ACT_PSS_RS485 || devices[channel].type == PP_ACT_ASS_RS485 || devices[channel].type == PP_SEN_RID_RS485 ||
      devices[channel].type == PP_ACT_REL_RS485 || devices[channel].type == PP_ACT_EV_CS || devices[channel].type == PP_SEN_COM_RS485  ||
      devices[channel].type == PP_SEN_SCN_RS485 || devices[channel].type == PP_SEN_PIN_RS485)
  {
    if (strstr(topic_cpy, "/gpios"))
    {
      // parse gpio num
      if (mqtt_params.contains("state"))
      {
        Serial.print("unsubscripe ");
        Serial.println(topic_cpy);
        client.unsubscribe(topic_cpy);
        int gpio = parse_subchannel(topic_cpy, 5);
        uint8_t state = atoi(mqtt_params["state"]);
        Serial.printf("received gpio state %d for gpio %d\n", state, gpio);

        // store values
        for (uint8_t i = 0; i < 16; i++)
        {
          if (devices[channel].gpios_available[i] == gpio)
          {
            devices[channel].gpios_state[i] = state;
            break;
          }
        }

        cmd.next_command_payload[0] = gpio;
        cmd.next_command_payload[1] = state;
        cmd.next_command_payload[2] = 0x00;
        cmd.next_command_payload[3] = 0x00;
        cmd.next_command_payload[4] = 0x00;
        cmd.next_command_payload[5] = 0x00;
        cmd.next_command_payload[6] = 0x00;

        cmd.next_command_payload_len = 7;
        cmd.next_command = CMD_SET_GPIO;

        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);

      }
      return;
    }
  }

  if (devices[channel].type == PP_64IO_RS485)
  {
    Serial.print("debug:  ");
    Serial.println(topic_cpy);
    if (strstr(topic_cpy, "/output"))
    {
      if (mqtt_params.contains("state"))
      {
        client.unsubscribe(topic_cpy);
        Serial.print("unsubscribe: ");
        Serial.println(topic_cpy);

        uint8_t  gpio = parse_subchannel(topic_cpy, 6);

        devices[channel].io64_outputs_defined++;

        uint8_t state = atoi(mqtt_params["state"]);
        Serial.printf("state = %d\n", state);
        if (state)
        {
          devices[channel].io64_output_state |= (1 << gpio);
        }
        else
        {
          devices[channel].io64_output_state &= ~(1 << gpio);
        }
      }

      Serial.printf("io64_output_defined = %ld\n", devices[channel].io64_outputs_defined);
      if (devices[channel].io64_outputs_defined == 32)
      {
        Serial.printf("io64 outputs defined ");
        Serial.printf("%ld\n", devices[channel].io64_output_state);

        char topic[128];
        sprintf(topic, "%s/%s/channel/%02d/io64/output/#", mqtt_topic_praefix, mqtt_device_serial, channel);
        client.unsubscribe(topic);
        Serial.printf("unsubscribe topic %s\n", topic);

        cmd.next_command_payload[0] = (uint8_t)((devices[channel].io64_output_state & 0xFF000000) >> 24);
        cmd.next_command_payload[1] = (uint8_t)((devices[channel].io64_output_state & 0xFF0000) >> 16);
        cmd.next_command_payload[2] = (uint8_t)((devices[channel].io64_output_state & 0xFF00) >> 8);
        cmd.next_command_payload[3] = (uint8_t)((devices[channel].io64_output_state & 0xFF));

        cmd.next_command_payload_len = 4;
        cmd.next_command = CMD_IO64_SETALL;

        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);

      }
      return;
    }
  }

  if (devices[channel].type == PP_PAP_GAT_RS485)
  {
    Serial.print("debug:  ");
    Serial.println(topic_cpy);

    if (strstr(topic_cpy, "/in/gate"))
    {
      uint8_t  pap_open_gpio     = PAP_GATE_OPEN_GPIO;
      uint8_t  pap_open_state    = 0; // default is close
      uint32_t pap_open_duration = 0;
            
      uint8_t pap_close_gpio      = PAP_GATE_CLOSE_GPIO;
      uint8_t pap_close_state     = 0;
      uint32_t pap_close_duration = 500;

      if (strcmp("OPEN", mqtt_params["cmd"]) == 0) 
      {
        pap_open_state = 1;
        pap_open_duration = 500;
      }
      else if (strcmp("ALWAYS_OPEN", mqtt_params["cmd"]) == 0) 
      {
        pap_open_state = 1;
      }
      else if (strcmp("CLOSE", mqtt_params["cmd"]) == 0) 
      {
        pap_close_state = 1;
      }      
      else {
        ;
      }

      if (pap_open_state || pap_close_state) 
      {
        cmd.next_command_payload[0] = pap_open_gpio;
        cmd.next_command_payload[1] = pap_open_state;
        cmd.next_command_payload[2] = (uint8_t)((pap_open_duration & 0xFF000000) >> 24);
        cmd.next_command_payload[3] = (uint8_t)((pap_open_duration & 0xFF0000) >> 16);
        cmd.next_command_payload[4] = (uint8_t)((pap_open_duration & 0xFF00) >> 8);
        cmd.next_command_payload[5] = (uint8_t)((pap_open_duration & 0xFF));
        cmd.next_command_payload[6] = 0;

        cmd.next_command_payload_len = 7;
        cmd.next_command = CMD_SET_GPIO;

        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      }

      if (pap_close_state)
      {
        cmd.next_command_payload[0] = pap_close_gpio;
        cmd.next_command_payload[1] = pap_close_state;
        cmd.next_command_payload[2] = (uint8_t)((pap_close_duration & 0xFF000000) >> 24);
        cmd.next_command_payload[3] = (uint8_t)((pap_close_duration & 0xFF0000) >> 16);
        cmd.next_command_payload[4] = (uint8_t)((pap_close_duration & 0xFF00) >> 8);
        cmd.next_command_payload[5] = (uint8_t)((pap_close_duration & 0xFF));
        cmd.next_command_payload[6] = 0;

        cmd.next_command_payload_len = 7;
        cmd.next_command = CMD_SET_GPIO;

        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);      
      }
      return;
    }
    else if (strstr(topic_cpy, "/gpios"))
    {
      // parse gpio num
      if (mqtt_params.contains("state"))
      {
        Serial.print("unsubscripe ");
        Serial.println(topic_cpy);
        client.unsubscribe(topic_cpy);
        int gpio = parse_subchannel(topic_cpy, 5);
        uint8_t state = atoi(mqtt_params["state"]);
        Serial.printf("received gpio state %d for gpio %d\n", state, gpio);

        // store values
        for (uint8_t i = 0; i < 16; i++)
        {
          if (devices[channel].gpios_available[i] == gpio)
          {
            devices[channel].gpios_state[i] = state;
            break;
          }
        }
        
        cmd.next_command_payload[0] = gpio;
        cmd.next_command_payload[1] = state;
        cmd.next_command_payload[2] = 0x00;
        cmd.next_command_payload[3] = 0x00;
        cmd.next_command_payload[4] = 0x00;
        cmd.next_command_payload[5] = 0x00;
        cmd.next_command_payload[6] = 0x00;

        cmd.next_command_payload_len = 7;
        cmd.next_command = CMD_SET_GPIO;

        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      }
    return;
    }
  }

  if (!mqtt_params.contains("cmd"))
  {
    Serial.println("received no command. leaving...");
    free(c_payload);
    return;
  }

  // config commands
  if (strstr(topic_cpy, "config"))
  {
    Serial.println("received config command");

    if (strcmp("GET_VERSION", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command GET_VERSION");
      char tp[128];
      char pl[128];
      sprintf(tp, "%s/%s/config/out", mqtt_topic_praefix, mqtt_device_serial);
      sprintf(pl, "%s VER=%s", PROG, VER);
      publish(tp, pl);
    }
    else if (strcmp("UPDATE_FIRMWARE", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command UPDATE_FIRMWARE");
      if (!mqtt_params.contains("url"))
      {
        return;
      }
      if (!mqtt_params.contains("filename"))
      {
        return;
      }

      update_firmaware(mqtt_params["url"], mqtt_params["filename"]);

    }
    else if (strcmp("DOWNLOAD_FILE", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command DOWNLOAD_FILE");
      if (!mqtt_params.contains("url"))
      {
        return;
      }
      if (!mqtt_params.contains("filename"))
      {
        return;
      }

      download_file(mqtt_params["url"], mqtt_params["filename"]);
    }
    else if (strcmp("DELETE_FILE", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command DELETE_FILE");

      if (!mqtt_params.contains("filename"))
      {
        return;
      }

      delete_file(mqtt_params["filename"]);

    }
    else if (strcmp("LIST_FILE", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command LIST_FILE");

      char *result;
      result = (char*)malloc(sizeof(char) * 512);

      listDir(SD_MMC, "/", 0, result);

      char topic[128];
      sprintf(topic, "%s/%s/config/out", mqtt_topic_praefix, mqtt_device_serial);
      publish(topic, result);

    }
    else if (strcmp("GATEWAY_RESET", mqtt_params["cmd"]) == 0)
    {
      Serial.println("command GATEWAY_RESET");
      char topic[128];
      sprintf(topic, "%s/%s/config/out", mqtt_topic_praefix, mqtt_device_serial);
      client.publish(topic, "resetting device...");
      rebootEspWithReason("remote reset request");
    }
    else if (strcmp("TEST", mqtt_params["cmd"]) == 0)
    {
      char topic[128];
      sprintf(topic, "%s/%s/config/state", mqtt_topic_praefix, mqtt_device_serial);
      client.publish(topic, "", true);
    }
    else
    {
      Serial.println("unknown command");
    }
  }

  // device commands
  if (channel >= 0)
  {
    if (strcmp("SET_LED", mqtt_params["cmd"]) == 0)
    {
      uint8_t red, green, blue, blink;
      uint32_t duration;


      Serial.println("execute command SET_LED");
      Serial.print("Channel: ");
      Serial.println(channel);

      if (mqtt_params.contains("red"))
      {
        red = (uint8_t)atoi(mqtt_params["red"]);
      }
      else
      {
        red = 0;
      }

      if (mqtt_params.contains("green"))
      {
        green = (uint8_t)atoi(mqtt_params["green"]);
      }
      else
      {
        green = 0;
      }

      if (mqtt_params.contains("blue"))
      {
        blue = (uint8_t)atoi(mqtt_params["blue"]);
      }
      else
      {
        blue = 0;
      }

      if (mqtt_params.contains("duration"))
      {
        duration = (uint32_t)atoi(mqtt_params["duration"]);
      }
      else
      {
        duration = 0;
      }

      if (mqtt_params.contains("blink"))
      {
        blink = (uint8_t)atoi(mqtt_params["blink"]);
      }
      else
      {
        blink = 0;
      }



      uint8_t extra_data = 0;

      if (devices[channel].client_prog_version > 0)
      {
        extra_data = 1;
        Serial.println("LED set extra_data to 1");
        if (mqtt_params.contains("group"))
        {
          cmd.next_command_payload[0] = atoi(mqtt_params["group"]);
        }
        else
        {
          Serial.println("LED set group to default (0)");
          cmd.next_command_payload[0] = 0;
        }

      }

      Serial.print("Duration: ");
      Serial.println(duration);
      Serial.print("Blink: ");
      Serial.println((uint8_t)atoi(mqtt_params["blink"]));

      cmd.next_command_payload[0 + extra_data] = red;
      cmd.next_command_payload[1 + extra_data] = green;
      cmd.next_command_payload[2 + extra_data] = blue;
      cmd.next_command_payload[3 + extra_data] = (uint8_t)((duration & 0xFF000000) >> 24);
      cmd.next_command_payload[4 + extra_data] = (uint8_t)((duration & 0xFF0000) >> 16);
      cmd.next_command_payload[5 + extra_data] = (uint8_t)((duration & 0xFF00) >> 8);
      cmd.next_command_payload[6 + extra_data] = (uint8_t)((duration & 0xFF));
      cmd.next_command_payload[7 + extra_data] = blink;

      cmd.next_command_payload_len = 8 + extra_data;
      cmd.next_command = CMD_SET_LED;

      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);

    }
    else if ("SET_LED_DEFAULTS" == mqtt_params["cmd"])
    {
      Serial.println("execute command SET_LED_DEFAULTS");
      uint8_t red, green, blue, blink;
      uint32_t duration;


      Serial.println("execute command SET_LED");
      Serial.print("Channel: ");
      Serial.println(channel);

      if (mqtt_params.contains("red"))
      {
        red = (uint8_t)atoi(mqtt_params["red"]);
      }
      else
      {
        red = 0;
      }

      if (mqtt_params.contains("green"))
      {
        green = (uint8_t)atoi(mqtt_params["green"]);
      }
      else
      {
        green = 0;
      }

      if (mqtt_params.contains("blue"))
      {
        blue = (uint8_t)atoi(mqtt_params["blue"]);
      }
      else
      {
        blue = 0;
      }

      if (mqtt_params.contains("duration"))
      {
        duration = (uint32_t)atoi(mqtt_params["duration"]);
      }
      else
      {
        duration = 0;
      }

      if (mqtt_params.contains("blink"))
      {
        blink = (uint8_t)atoi(mqtt_params["blink"]);
      }
      else
      {
        blink = 0;
      }



      uint8_t extra_data = 0;

      if (devices[channel].client_prog_version > 0)
      {
        extra_data = 1;
        Serial.println("LED set extra_data to 1");
        if (mqtt_params.contains("group"))
        {
          cmd.next_command_payload[0] = atoi(mqtt_params["group"]);
        }
        else
        {
          Serial.println("LED set group to default (0)");
          cmd.next_command_payload[0] = 0;
        }

      }

      Serial.print("Duration: ");
      Serial.println(duration);
      Serial.print("Blink: ");
      Serial.println((uint8_t)atoi(mqtt_params["blink"]));

      cmd.next_command_payload[0 + extra_data] = red;
      cmd.next_command_payload[1 + extra_data] = green;
      cmd.next_command_payload[2 + extra_data] = blue;
      cmd.next_command_payload[3 + extra_data] = (uint8_t)((duration & 0xFF000000) >> 24);
      cmd.next_command_payload[4 + extra_data] = (uint8_t)((duration & 0xFF0000) >> 16);
      cmd.next_command_payload[5 + extra_data] = (uint8_t)((duration & 0xFF00) >> 8);
      cmd.next_command_payload[6 + extra_data] = (uint8_t)((duration & 0xFF));
      cmd.next_command_payload[7 + extra_data] = blink;

      cmd.next_command_payload_len = 8 + extra_data;
      cmd.next_command = CMD_SET_LED;

      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);




    }
    else if (strcmp("SET_OUTPUT", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command SET_OUTPUT");

      int state = 0;
      if (!mqtt_params.contains("state"))
      {
        Serial.println("missing parameter: state");
        return;
      }
      else
      {
        state = atoi(mqtt_params["state"]);
        Serial.print("State: ");
        Serial.println(mqtt_params["state"]);
      }

      int gpio = 0;
      if (!mqtt_params.contains("gpio"))
      {
        Serial.printf("set_output %d - %d\n", channel, devices[channel].type);
        if (devices[channel].type == PP_ACT_REL_INT)
        {
          if (devices[channel].actor_addr == 0xFF)
          {
            gpio = channel;
          }
          else
          {
            gpio = devices[channel].actor_addr;
          }
          Serial.print(" Channel / Output-Channel: ");
          Serial.print(gpio);
        }
        else
        {
          Serial.println("missing parameter: gpio");
          return;
        }
      }
      else
      {
        gpio = atoi(mqtt_params["gpio"]);
        Serial.print(" GPIO: ");
        Serial.print(gpio);
      }


      int duration = 0;
      if (mqtt_params.contains("duration"))
      {
        duration = atoi(mqtt_params["duration"]);
      }

      int blink = 0;
      if (mqtt_params.contains("blink"))
      {
        blink = atoi(mqtt_params["blink"]);
      }

      Serial.print(" Duration: ");
      Serial.print(duration);
      Serial.print(" Blink: ");
      Serial.println(blink);

      // internal io
      if (devices[channel].type == PP_ACT_REL_INT)
      {
        if (0 == duration)
        {
#ifndef GWV2
          port_exp_0.digitalWrite(gpio, state ? HIGH : LOW);
#else
          port_exp_output_0.digitalWrite(gpio, state ? HIGH : LOW);
#endif
        }
        else
        {
#ifndef GWV2
          port_exp_0.digitalWrite(gpio, mqtt_params["state"] ? HIGH : LOW, duration, false);
#else
          port_exp_output_0.digitalWrite(gpio, mqtt_params["state"] ? HIGH : LOW, duration, blink);
#endif
        }
      }
      // rs485 io
      else if (devices[channel].type == PP_ACT_PSS_RS485 || devices[channel].type == PP_ACT_ASS_RS485 || devices[channel].type == PP_ACT_REL_RS485  || devices[channel].type == PP_ACT_EV_CS || devices[channel].type == PP_64IO_RS485 || devices[channel].type == PP_SEN_PIN_RS485 || devices[channel].type == PP_ACT_GAT_RS485 || devices[channel].type == PP_SEN_RID_RS485)
      {

        cmd.next_command_payload[0] = gpio;
        cmd.next_command_payload[1] = state;
        cmd.next_command_payload[2] = (uint8_t)((duration & 0xFF000000) >> 24);
        cmd.next_command_payload[3] = (uint8_t)((duration & 0xFF0000) >> 16);
        cmd.next_command_payload[4] = (uint8_t)((duration & 0xFF00) >> 8);
        cmd.next_command_payload[5] = (uint8_t)((duration & 0xFF));
        cmd.next_command_payload[6] = blink;

        cmd.next_command_payload_len = 7;
        cmd.next_command = CMD_SET_GPIO;

        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      }
      else
      {
        Serial.println("command not supported by device");
      }
    }
    else if (strcmp("UPDATE_CLIENT", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command UPDATE_CLIENT");

      if (!mqtt_params.contains("device_id"))
      {
        return;
      }

      if (!mqtt_params.contains("filename"))
      {
        return;
      }
    }
    else if (strcmp("SET_TFT_IMAGE", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command SET_TFT_IMAGE");
      if (!mqtt_params.contains("image"))
      {
        return;
      }
      Serial.print("Channel: ");
      Serial.println(channel);
      Serial.print("Image: ");
      Serial.println((uint8_t)atoi(mqtt_params["image"]));
      cmd.next_command_payload_len = 1;
      cmd.next_command_payload[0] = (uint8_t)atoi(mqtt_params["image"]);
      cmd.next_command_payload_len = 1;
      cmd.next_command = CMD_SET_TFT_IMAGE;

      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
    }
    else if (strcmp("SET_TFT_TEXT", mqtt_params["cmd"]) == 0)
    {
      Serial.println("execute command SET_TFT_TEXT");

      if (!mqtt_params.contains("image"))
      {
        return;
      }

      if (!mqtt_params.contains("line1"))
      {
        return;
      }

      Serial.print("Channel: ");
      Serial.println(channel);
      Serial.print("Image: ");
      Serial.println(mqtt_params["image"]);
      Serial.print("Line1: ");
      Serial.println(mqtt_params["line1"]);

      if (mqtt_params.contains("line2"))
      {
        Serial.print("Line2: ");
        Serial.println(mqtt_params["line2"]);
      }
      if (mqtt_params.contains("line3"))
      {
        Serial.print("Line3: ");
        Serial.println(mqtt_params["line3"]);
      }

      int l1 = 0;
      int l2 = 0;
      int l3 = 0;;
      if (mqtt_params.contains("line1"))
      {
        l1 = strlen(mqtt_params["line1"]);
      }
      if (mqtt_params.contains("line2"))
      {
        l2 = strlen(mqtt_params["line2"]);
      }
      if (mqtt_params.contains("line3"))
      {
        l3 = strlen(mqtt_params["line3"]);
      }

      uint8_t len = l1 + l2 + l3 + 4;

      Serial.println("after strlen");

      memset(cmd.next_command_payload, 0x0, len + 1);

      char n_payload[128];
      //n_payload = (char*)malloc(len + 1);

      sprintf(n_payload, "0%s\n%s\n%s\n",
              mqtt_params["line1"],
              mqtt_params.contains("line2") ? mqtt_params["line2"] : " ",
              mqtt_params.contains("line3") ? mqtt_params["line3"] : " ");

      n_payload[0] = (uint8_t)atoi(mqtt_params["image"]);

      memcpy(cmd.next_command_payload, n_payload, len);
      Serial.println("after memcpy");
      cmd.next_command_payload_len = len;
      cmd.next_command = CMD_TFT_TEXT_TO_IMG;

      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      //free(n_payload);
    }
    else if (strcmp("OPEN_GATE", mqtt_params["cmd"]) == 0)
    {
      // Gate Steuerung
      Serial.println("execute command OPEN_GATE");
      Serial.print("Channel: ");
      Serial.println(channel);

      if (!mqtt_params.contains("open"))
      {
        return;
      }
      else if (strcmp("access", mqtt_params["open"]) == 0)
      {
        cmd.next_command_payload[0] = 1;
        cmd.next_command_payload_len = 1;
        cmd.next_command = CMD_OPENGATE;
        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      }
      else if (strcmp("exit", mqtt_params["open"]) == 0)
      {
        cmd.next_command_payload[0] = 0;
        cmd.next_command_payload_len = 1;
        cmd.next_command = CMD_OPENGATE;
        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      }
      else if (strcmp("true", mqtt_params["open"]) == 0)
      {
        cmd.next_command_payload[0] = 1;
        cmd.next_command_payload_len = 1;
        cmd.next_command = CMD_OPENGATE;
        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      }
      else if (strcmp("false", mqtt_params["open"]) == 0)
      {
        cmd.next_command_payload[0] = 0xFF;
        cmd.next_command_payload_len = 1;
        cmd.next_command = CMD_OPENGATE;
        set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
      }

    }
    else if (strcmp("DISABLE", mqtt_params["cmd"]) == 0)
    {
      if (!mqtt_params.contains("state"))
      {
        return;
      }

      cmd.next_command = CMD_DISABLE;
      cmd.next_command_payload_len = 1;
      cmd.next_command_payload[0] = atoi(mqtt_params["state"]);
      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
    }
    else if (strcmp("DEVICE_STATUS", mqtt_params["cmd"]) == 0)
    {
      if (!mqtt_params.contains("device"))
      {
        return;
      }

      cmd.next_command = CMD_STATUS_REQUEST;
      cmd.next_command_payload_len = 1;
      cmd.next_command_payload[0] = 0x51;
      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
    }
    else if (strcmp("DEVICE_RESET", mqtt_params["cmd"]) == 0)
    {
      if (!mqtt_params.contains("device"))
      {
        return;
      }

      cmd.next_command = CMD_RESET_DEVICE;
      cmd.next_command_payload_len = 0;
      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);

      cmd.next_command = CMD_RESET_DEVICE;
      cmd.next_command_payload_len = 0;

    }
    else if (strcmp("MORE_MOVE", mqtt_params["cmd"]) == 0)
    {
      if (!mqtt_params.contains("direction"))
      {
        return;
      }
      cmd.next_command = CMD_MORE_MOVE;
      cmd.next_command_payload[0] = atoi(mqtt_params["direction"]) + 0x30;
      cmd.next_command_payload_len = 1;
      set_next_command(channel, cmd.next_command, cmd.next_command_payload, cmd.next_command_payload_len);
    }
    else
    {
      Serial.println("unknown command");
    }
  }

  free(c_payload);
}

uint8_t parse_channel(char *topic)
{
  char *token;
  uint8_t token_count = 0;
  uint8_t channel = 0;

  token = strtok(topic, "/");
  while ( token != NULL )
  {
    token_count++;
    if ((mqtt_topic_praefix_field_count + 3) == token_count) //Pos of Channel ID
    {
      channel = atoi(token);
    }
    token = strtok(NULL, "/");
  }

  free(token);

  return channel;
}

uint8_t parse_subchannel(char *topic, int pos)
{
  char *token;
  uint8_t token_count = 0;
  uint8_t pm_num = 0;

  token = strtok(topic, "/");
  while ( token != NULL )
  {
    token_count++;
    if ((pos + mqtt_topic_praefix_field_count) == token_count)
    {
      pm_num = atoi(token);
    }
    token = strtok(NULL, "/");
  }

  free(token);

  return pm_num;
}

uint8_t parse_params(char *mqtt_payload)
{
  // delete old map
  while (mqtt_params.keyAt(0))
  {
    //Serial.println("remove parameter");
    mqtt_params.remove(mqtt_params.keyAt(0));
  }

  char *cmd_seq;
  char *cmd_seq_end;
  cmd_seq = strtok_r(mqtt_payload, ";", &cmd_seq_end);
  while ( cmd_seq != NULL )
  {
    char *key_end;
    char *key, *value;
    key = strtok_r(cmd_seq, "=", &key_end);
    value = strtok_r(NULL, "=", &key_end);

    if (value != NULL)
    {
      mqtt_params[key] = value;
    }
    else
    {
      Serial.printf("empty value... Key: % s\n", key);
    }

    cmd_seq = strtok_r(NULL, ";", &cmd_seq_end);
  }

  free(cmd_seq);
  free(cmd_seq_end);
}
