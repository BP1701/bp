void init_rs485()
{
  rs485.onEvent(rs485Callback);
  rs485.set_log_enabled(true);
#ifndef MOD_IO64
  rs485.begin(1, 38400);
#else
  rs485.begin(2, 38400);
#endif
}

void rs485Callback(_rs485message event)
{
#ifdef _DEBUG
  if (ALIVE != event.command)
  {
    char msg[64];
    log("RS485 Message:");
    if (FILE_UPLOAD == event.command)   // do not print payload for fileupload, bugs the console output
    {
      sprintf(msg, "Adresse: 0x%02x\tCommand: 0x%02x\tLänge: %d",
              event.devaddr, event.command, event.len_payload);
    }
    else
    {
      sprintf(msg, "Adresse: 0x%02x\tCommand: 0x%02x\tPayload: %s\tLänge: %d",
              event.devaddr, event.command, event.payload, event.len_payload);
    }
    log(msg);
  }
#endif

  // slave: receive ALIVE requests from master and answer with ACK or with data if available
  if (ALIVE == event.command)
  {
#ifdef MOD_STATUS_LEDS
    BLUE_LED_TRIGGER = true;
#endif
    if (!rfid_data_available && !card_data_available && !opticon_data_avaiable && !counter_data_available &&
        !gpios_data_available && !input_gpios_data_available && !pinpad_data_available && !update_data_available &&
        !iomodul_input_data_available && !iomodul_output_available && !coin_data_available)
    {
      gateway_never_seen = false;
      send_ack();
    }
    else
    {
      //Serial.println("Send DATA");
#ifdef MOD_STATUS_LEDS
      if (rfid_data_available || card_data_available || opticon_data_avaiable || counter_data_available || 
      gpios_data_available || input_gpios_data_available || pinpad_data_available ||  
      iomodul_input_data_available || iomodul_output_available || coin_data_available) {
        RED_LED_TRIGGER = true;
      }
#endif
      if (rfid_data_available)
      {
        Serial.print("RFID LEN Payload: ");
        Serial.println(rfid_data_len + 1);

        byte *payload;
        payload = (byte*)malloc(rfid_data_len + 2);
        payload[0] = DATASOURCE_RFID_FEIG;
        memcpy(payload + 1, rfid_data, rfid_data_len);
        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = rfid_data_len + 1;
        rs485.send(msg);

#ifdef _DEBUG
        char message[64];
        memset(message, 0x0, 64);
        sprintf(message, "SEND: ");
        memcpy(message + 6, payload, rfid_data_len + 1);
        log(message);
#endif

        free(payload);
        // delete posted data TODO: it would be better to receive an ACK before deleting the data
        memset(rfid_data, 0x0, 64);
        rfid_data_len = 0;
        rfid_data_available = false;
      }
      else if (card_data_available)
      {
        char cd[64];
        sprintf(cd, "%ld", card_data_new_card);

        Serial.print("CARD_DATA LEN Payload: ");
        Serial.println(strlen(cd) + 1);

        byte *payload;
        payload = (byte*)malloc(strlen(cd) + 2);
        payload[0] = DATASOURCE_RFID_QRT310;
        memcpy(payload + 1, cd, strlen(cd));
        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = strlen(cd) + 1;
        rs485.send(msg);

#ifdef _DEBUG
        char message[64];
        memset(message, 0x0, 64);
        sprintf(message, "SEND: ");
        memcpy(message + 6, cd, strlen(cd));
        log(message);
#endif

        free(payload);
        // delete posted data TODO: it would be better to receive an ACK before deleting the data
        card_data_available = false;
      }
      else if (opticon_data_avaiable)
      {
        Serial.print("LEN OPTICON Payload: ");
        Serial.println(opticon_data_len);

        byte *payload;
        payload = (byte*)malloc(opticon_data_len + 2);
        payload[0] = DATADOURCE_SCAN_OPTI;
        memcpy(payload + 1, opticon_data, opticon_data_len);
        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = opticon_data_len + 1;
        rs485.send(msg);

#ifdef _DEBUG
        char message[2048];
        memset(message, 0x0, 2048);
        sprintf(message, "SEND OPTICON: ");
        memcpy(message + 6, opticon_data, opticon_data_len);
        log(message);
#endif

        free(payload);
        memset(opticon_data, 0x0, 2048);
        opticon_data_len = 0;
        opticon_data_avaiable = false;
      }
      else if (counter_data_available)
      {

        // prepare data
        if (counter_update_flag & 0x01)
        {
          Serial.println("prepare data for pm0");
          // prepare send rs485 data
          counter_data_source = 0;
          uint32_t tmp_value = counter_value[0];
          Serial.println(tmp_value);
          counter_data[0] = (tmp_value & 0xFF000000) >> 24;
          counter_data[1] = (tmp_value & 0xFF0000) >> 16;
          counter_data[2] = (tmp_value & 0xFF00) >> 8;
          counter_data[3] = (tmp_value & 0xFF);
          counter_data_len = 4;
          counter_data_available = true;

          // delete flag
          counter_update_flag &= ~(1 << 0);
        }

        if (counter_update_flag & 0x02)
        {
          Serial.println("prepare data for pm1");
          // prepare send rs485 data
          counter_data_source = 1;
          uint32_t tmp_value = counter_value[1];
          Serial.println(tmp_value);
          counter_data[0] = (tmp_value & 0xFF000000) >> 24;
          counter_data[1] = (tmp_value & 0xFF0000) >> 16;
          counter_data[2] = (tmp_value & 0xFF00) >> 8;
          counter_data[3] = (tmp_value & 0xFF);
          counter_data_len = 4;
          counter_data_available = true;

          // delete flag
          counter_update_flag &= ~(1 << 1);
        }

        if (counter_update_flag & 0x04)
        {
          Serial.println("prepare data for pm0");
          // prepare send rs485 data
          counter_data_source = 2;
          uint32_t tmp_value = counter_value[2];
          Serial.println(tmp_value);
          counter_data[0] = (tmp_value & 0xFF000000) >> 24;
          counter_data[1] = (tmp_value & 0xFF0000) >> 16;
          counter_data[2] = (tmp_value & 0xFF00) >> 8;
          counter_data[3] = (tmp_value & 0xFF);
          counter_data_len = 4;
          counter_data_available = true;

          // delete flag
          counter_update_flag &= ~(1 << 2);
        }

        if (counter_update_flag & 0x08)
        {
          Serial.println("prepare data for pm1");
          // prepare send rs485 data
          counter_data_source = 3;
          uint32_t tmp_value = counter_value[3];
          Serial.println(tmp_value);
          counter_data[0] = (tmp_value & 0xFF000000) >> 24;
          counter_data[1] = (tmp_value & 0xFF0000) >> 16;
          counter_data[2] = (tmp_value & 0xFF00) >> 8;
          counter_data[3] = (tmp_value & 0xFF);
          counter_data_len = 4;
          counter_data_available = true;

          // delete flag
          counter_update_flag &= ~(1 << 3);
        }

        Serial.print("LEN counter Payload: ");
        Serial.println(counter_data_len);

        byte *payload;
        payload = (byte*)malloc(counter_data_len + 2);
        payload[0] = DATASOURCE_COUNTER;
        payload[1] = counter_data_source;
        memcpy(payload + 2, counter_data, counter_data_len);
        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = counter_data_len + 2;
        rs485.send(msg);

#ifdef _DEBUG
        char message[64];
        memset(message, 0x0, 64);
        sprintf(message, "SEND COUNTER: 0x%02x, 0x%02x, 0x%02x, 0x%02x", counter_data[0], counter_data[1], counter_data[2], counter_data[3]);
        log(message);
#endif

        free(payload);
        memset(counter_data, 0x0, 64);
        counter_data_len = 0;
        counter_data_available = false;
      }
      else if (gpios_data_available)
      {
        byte *payload;
        payload = (byte*)malloc(2 * sizeof(gpios_available) + 2);
        payload[0] = DATASOURCE_AVAIABLE_GPIOS;
        payload[1] = sizeof(gpios_available);

        // mqtt update flag
        if (outputs_undef)
        {
          Serial.println("set update flag");
          payload[1] |= 0x80;
        }

        outputs_undef = false;


        uint8_t payload_pos = 2;
        for (int i = 0; i < sizeof(gpios_available); i++)
        {
          payload[payload_pos++] = gpios_available[i];
          payload[payload_pos++] = gpio_state[i];
        }

        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = 2 * sizeof(gpios_available) + 2;
        rs485.send(msg);

#ifdef _DEBUG
        char message[64];
        memset(message, 0x0, 64);
        sprintf(message, "SEND AVAILABLE GPIOS:");

        for (int i = 0; i < sizeof(gpios_available); i++)
        {
          sprintf(message, "%s %d-%d", message, gpios_available[i], gpio_state[i]);
        }
        free(payload);
        gpios_data_available = false;
        log(message);
#endif
      }
      else if (input_gpios_data_available)
      {
        byte *payload;
        payload = (byte*)malloc((input_gpios_count * 2) + 2);
        payload[0] = DATASOURCE_INPUT_GPIOS;

        char log_message[64];
        memset(log_message, 0x0, 64);
        sprintf(log_message, "SEND AVAILABLE INPUTS:");

        uint8_t message_len = 0;
        Serial.print("change mask ");
        Serial.println(input_change_mask);
        for (int i = 0; i < input_gpios_count; i++)
        {
          if (input_change_mask & (1 << i))
          {
            payload[2 + (message_len * 2)] = input_gpios_available[i];
            payload[2 + (message_len * 2) + 1] = ((input_gpios_state & (1 << i)) >> i);
            message_len++;

            sprintf(log_message, "%s %d-%d", log_message, input_gpios_available[i], (input_gpios_state & (1 << i)) >> i);
          }
        }

        payload[1] = message_len * 2;

        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = 2 * message_len + 2;
        rs485.send(msg);

        free(payload);
        input_gpios_data_available = false;
        log(log_message);
      }
      else if (pinpad_data_available)
      {
        Serial.print("Pinpad LEN Payload: ");
        Serial.println(pinpad_data_len);

        byte *payload;
        payload = (byte*)malloc(pinpad_data_len + 2);
        payload[0] = DATASOURCE_PINPAD;
        memcpy(payload + 1, pinpad_data, pinpad_data_len);
        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = pinpad_data_len + 1;
        rs485.send(msg);

        free(payload);
        pinpad_data_available = false;
      }
      else if (iomodul_input_data_available)
      {
        byte *payload;
        payload = (byte*)malloc(5);
        payload[0] = DATASOURCE_IO64_COUNTER_TRIGGER;
        payload[1] = (iomodul_input_data & 0xFF000000) >> 24;
        payload[2] = (iomodul_input_data & 0xFF0000) >> 16;
        payload[3] = (iomodul_input_data & 0xFF00) >> 8;
        payload[4] = (iomodul_input_data & 0xFF);

        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = 5;
        rs485.send(msg);

        //#ifdef _DEBUG
        char message[64];
        memset(message, 0x0, 64);
        sprintf(message, "SEND O64_COUNTER_TRIGGER: 0x%02x, 0x%02x, 0x%02x, 0x%02x", payload[1], payload[2], payload[3], payload[4]);
        log(message);
        //#endif

        free(payload);
        iomodul_input_data = 0;
        iomodul_input_data_available = false;
      }
      else if (iomodul_output_available)
      {
        byte *payload;
        payload = (byte*)malloc(3);
        payload[0] = DATASOURCE_IO64_OUTPUT_DATA;
        payload[1] = (iomodul_output_data >> 8) & 0xFF;
        payload[2] = iomodul_output_data & 0xFF;

        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = 3;
        rs485.send(msg);

#ifdef _DEBUG
        char message[64];
        memset(message, 0x0, 64);
        sprintf(message, "SEND DATASOURCE_IO64_OUTPUT_DATA: 0x%02x, 0x%02x", payload[1], payload[2]);
        log(message);
#endif

        free(payload);

        if (outputs_undef)
        {
          outputs_undef = false;
        }

        iomodul_output_data = 0x0;
        iomodul_output_available = false;

      }
      else if (update_data_available)
      {
        send_update_status(update_data);
        update_data_available = false;
      }
      else if (coin_data_available)
      {
        Serial.print("COIN_EMP800 LEN Payload: ");
        Serial.println(rfid_data_len + 1);

        byte *payload;
        payload = (byte*)malloc(coin_data_len + 2);
        payload[0] = DATASOURCE_COIN_EMP800;
        memcpy(payload + 1, coin_data, coin_data_len);
        _rs485message msg;
        msg.devaddr = DEVADDR;
        msg.command = SEND_DATA;
        msg.payload = payload;
        msg.len_payload = coin_data_len + 1;
        rs485.send(msg);

#ifdef _DEBUG
        char message[64];
        memset(message, 0x0, 64);
        sprintf(message, "SEND: ");
        memcpy(message + 6, payload, coin_data_len + 1);
        log(message);
#endif

        free(payload);
        // delete posted data TODO: it would be better to receive an ACK before deleting the data
        memset(coin_data, 0x0, 64);
        coin_data_len = 0;
        coin_data_available = false;
      }
    }
  }
  else if (SET_LED == event.command)
  {
    // parse payload
    if (event.len_payload < 5)
    {
      send_nak();
    }
    else
    {
      uint8_t group = 0;
      uint8_t red = 0;
      uint8_t green = 0;
      uint8_t blue = 0;
      uint32_t duration = 0;
      uint8_t blink = 0;
      if (byte_prog_ver > 0)
      {
        group = event.payload[0];
        red = event.payload[1];
        green = event.payload[2];
        blue = event.payload[3];
        duration += event.payload[4]; duration <<= 8;
        duration += event.payload[5]; duration <<= 8;
        duration += event.payload[6]; duration <<= 8;
        duration += event.payload[7];
        blink = event.payload[8];
      }
      else
      {
        group = 0;
        red = event.payload[0];
        green = event.payload[1];
        blue = event.payload[2];
        duration += event.payload[3]; duration <<= 8;
        duration += event.payload[4]; duration <<= 8;
        duration += event.payload[5]; duration <<= 8;
        duration += event.payload[6];
        blink = event.payload[7];
      }


      Serial.print("Duration: ");
      Serial.print(duration);
      Serial.print(" Blink: ");
      Serial.println(blink);

      led_set(group, red, green, blue, duration, blink);

      // confirm command excecuted
      send_ack();

      pinpad_timer = 0;
    }
  }
  else if (SET_LED_DEFAULT == event.command)
  {
    // parse payload
    if (event.len_payload < 4)
    {
      send_nak();
    }
    else
    {
      led_write_defaults(0, event.payload[0], event.payload[1], event.payload[2], event.payload[3]);
    }
  }
  else if (GET_UPTIME == event.command)
  {
    byte *payload;
    payload = (byte*)malloc(4 + 1);

    unsigned long uptime = millis();
    payload[0] = (uptime & 0xFF000000) >> 24;
    payload[1] = (uptime & 0xFF0000) >> 16;
    payload[2] = (uptime & 0xFF00) >> 8;
    payload[3] = (uptime & 0xFF);

    _rs485message msg;
    msg.devaddr = DEVADDR;
    msg.command = ANSWER_UPTIME;
    msg.payload = payload;
    msg.len_payload = 4;
    rs485.send(msg);

    free(payload);
  }
  else if (STATUS_REQUEST == event.command)
  {
    if (STATUS_ACTOR == event.payload[0])
    {
      byte *payload;
      payload = (byte*)malloc(1);
      //memcpy(payload, &gate_status, 1);
      
      if (kaba_state == 3)
      {
        kaba_state = 1;
      }      
      Serial.print("Status ");
      Serial.print(gate_status);      
      payload[0] = gate_status;
      _rs485message msg;
      //msg.devaddr = RS485_DEV_ADDR;
      msg.devaddr = DEVADDR;
      msg.command = STATUS_ACTOR;
      msg.payload = payload;
      msg.len_payload = 1;
      rs485.send(msg);
      free(payload);
      
      if (gate_status == 1)
      {
        Serial.println(": open");
      }
      if (gate_status == 2)
      {
        Serial.println(": nornal pass");
      }
      if (gate_status == 3)
      {
        Serial.println(": failed pass");
      }    
    }    
     
    if (STATUS_VERSION == event.payload[0])
    {
      byte *payload;
      payload = (byte*)malloc(6);

      unsigned long uptime = millis();
      payload[0] = byte_prog_ver;
      payload[1] = byte_app_version;
      payload[2] = (uptime & 0xFF000000) >> 24;
      payload[3] = (uptime & 0xFF0000) >> 16;
      payload[4] = (uptime & 0xFF00) >> 8;
      payload[5] = (uptime & 0xFF);

      _rs485message msg;
      msg.devaddr = DEVADDR;
      msg.command = STATUS_VERSION;
      msg.payload = payload;
      msg.len_payload = 6;
      rs485.send(msg);

      free(payload);
    }
  }
  else if (REQUEST_RESET == event.command)
  {
    send_ack();
    delay(100);
    ESP.restart();
  }
#ifdef MOD_COUNTER || MOD_COINS
  else if (COUNTER_SET == event.command)
  {
    //
    // receive 1 value payload_len = 4 + 1 (first byte = counter_addr
    // receive all values counter_count = len_payload / 4

    if (5 == event.len_payload)
    {
      uint8_t tmp_channel;
      uint32_t tmp_value = 0;

      tmp_channel = event.payload[0];
      tmp_value += event.payload[1]; tmp_value <<= 8;
      tmp_value += event.payload[2]; tmp_value <<= 8;
      tmp_value += event.payload[3]; tmp_value <<= 8;
      tmp_value += event.payload[4];

      Serial.print("setting counter_value ");
      Serial.print(tmp_value);
      Serial.print(" for power-meter ");
      Serial.println(tmp_channel);

      if (tmp_value >= counter_value[tmp_channel])
      {
        counter_value[tmp_channel] = tmp_value;
      }
      else
      {
        counter_update_flag |= tmp_channel;
      }
      counter_undef = false;
    }
    else
    {
      uint8_t tmp_value = event.len_payload / 4;

      for (int i = 0; i < (tmp_value > COUNTER_COUNT ? COUNTER_COUNT : tmp_value); i++)
      {
        uint32_t tmp_value = 0;
        tmp_value += event.payload[0 + (i * 4)]; tmp_value <<= 8;
        tmp_value += event.payload[1 + (i * 4)]; tmp_value <<= 8;
        tmp_value += event.payload[2 + (i * 4)]; tmp_value <<= 8;
        tmp_value += event.payload[3 + (i * 4)];

        Serial.print("setting counter_value ");
        Serial.print(tmp_value);
        Serial.print(" for power-meter ");
        Serial.println(i);

        counter_value[i] = tmp_value;
      }
      counter_undef = false;
    }

    send_ack();

  }
#endif
  else if (SET_GPOI == event.command)
  {
    uint8_t gpio = event.payload[0];
    uint8_t state = event.payload[1];
    uint32_t duration = 0;
    duration += event.payload[2]; duration <<= 8;
    duration += event.payload[3]; duration <<= 8;
    duration += event.payload[4]; duration <<= 8;
    duration += event.payload[5];
    uint8_t blink = event.payload[6];

#ifndef MOD_IO64

  uint8_t idx;
  
  #ifdef MOD_GPIO
    idx = is_gpio_available(gpio);
    if (idx != 0xFF)
    {
      //digitalWrite(gpio, state);
      //gpio_state[idx] = state;
      //gpios_data_available = true;
      set_gpio(gpio, state, duration, blink);
    }
  #endif
   #ifdef MOD_AQUA_STATION  
    idx = is_aqua_gpio_available(gpio);
    if (idx != 0xFF)
    {
      set_amount_index(idx);
    }
  #endif
#else
    Serial.printf("setting gpio %d state %d duration %ld blink %d\n", gpio, state, duration, blink);
    if (gpio < 16)
    {
      port_exp_output_0.digitalWrite(gpio, state, duration, blink);
    }
    else
    {
      port_exp_output_1.digitalWrite(gpio - 16, state, duration, blink);
    }
#endif
    send_ack();
  }
  else if (SET_ALL_GPIOS == event.command)
  {
    uint16_t low_out = 0;
    uint16_t high_out = 0;

    low_out  = (event.payload[2] << 8) + event.payload[3];
    high_out = (event.payload[0] << 8) + event.payload[1];

    Serial.printf("set all %ld %ld\n", low_out, high_out);
    port_exp_output_0.set_all(low_out);
    port_exp_output_1.set_all(high_out);

  }
  //GATE 
  else if (SET_ACTOR == event.command) {
    
    if (event.len_payload >= 2)
    {
      send_nak(); //Payload error
    }
    else
    {
      gate_move = 1;
      gate_status = 1;
      Serial.print("rs485 SET_ACTOR\n");
      send_ack();
    }
  }
  else if (STATUS_REQUEST == event.command) {
    if (STATUS_ACTOR == event.payload[0]) {
      byte *payload;
      payload = (byte*)malloc(1);
      //memcpy(payload, &gate_status, 1);
      
      if (kaba_state == 3) {
        kaba_state = 1;
      }      
      Serial.print("Status ");
      Serial.print(gate_status);      
      payload[0] = gate_status;
      _rs485message msg;
      //msg.devaddr = RS485_DEV_ADDR;
      msg.devaddr = DEVADDR;
      msg.command = STATUS_ACTOR;
      msg.payload = payload;
      msg.len_payload = 1;
      rs485.send(msg);
      free(payload);
      
      if (gate_status == 1) {
        Serial.println(": open");
      }
      if (gate_status == 2) {
        Serial.println(": nornal pass");
      }
      if (gate_status == 3) {
        Serial.println(": failed pass");
      }    
    }    
  }
  else if (DISABLE == event.command) {
    
    if (event.len_payload != 1)
    {
      send_nak(); //Payload error
    }
    else
    {
      if (event.payload[0]) //Disable -> LED RED, CRT310 Shutter, Reject IDs
      {
        gate_disable = 1;
        //led_set_defaults(0,   0, 0, 0, 0);
        //led_set_defaults(1, 255, 0, 0, 0);
      }
      else
      { 
        gate_disable = 0;
        //led_set_defaults(0, 0, 0, 255, 0);
        //led_set_defaults(1, 0, 0,   0, 0);
      }
      gate_disable_undefined = false;

      Serial.print("rs485 DISABLE ");
      Serial.println(gate_disable);
      
      send_ack();
    }
  }
  //END GATE
  else if (REQUEST_UPLOAD == event.command)     // new
  {
    Serial.println("REQUEST_UPLOAD\n");
    send_ack();

    // parse bytes(0-3) from payload into int value
    unsigned int filesize = event.payload[3] | (event.payload[2] << 8) | (event.payload[1] << 16) | (event.payload[0] << 24);

    uint8_t receivedHash[HASH_SIZE];

    // retrieve hash from payload
    for (int i = 4; i <= 19; i++)
    {
      receivedHash[i - 4] = event.payload[i];
    }
    store_file_hash("HASH", receivedHash);

    char filepath[128];
    // parse filename (4-x) from payload
    for (int i = 20; i <= event.len_payload; i++)
    {
      filepath[i - 20] = event.payload[i];
    }
    correctFilepath(filepath);

#ifdef _DEBUG
    Serial.printf("Path: %s\n", filepath);
    Serial.printf("Hash: ");
    printBytes(receivedHash, HASH_SIZE);
    Serial.printf("Size to be written: %d\n", filesize);
#endif

    if (isSufficientSpace(&filesize))
    {
      // init file transmission
      update_seq_number = 0;
      num_bytes_per_payload = 0;
      total_package_count = 0;
      strcpy(last_update_file_name, filepath);
      file_size_received = filesize;
      update_data = REQUEST_SUCCESSFUL;
      openFile(last_update_file_name);
    }
    else
    {
      update_data = REQUEST_FAILED;
    }
    update_data_available = true;

  }
  else if (FILE_UPLOAD == event.command)        // new
  {

    uint8_t chunk_size = event.len_payload - 2;
    byte received_bytes[chunk_size];
    uint8_t calculatedHash[HASH_SIZE];

    if (total_package_count == 0)
    {
      total_package_count = ceil(file_size_received / (float) chunk_size);
      num_bytes_per_payload = chunk_size;
      update_data = TRANSMISSION_STARTED;
      update_data_available = true;
#ifdef _DEBUG
      Serial.printf("Bytes per payload: %d \t Expected package count: %d\n", num_bytes_per_payload, total_package_count);
#endif
    }


    // retrieve file pos. from payload
    uint16_t pos = event.payload[1] | (event.payload[0] << 8);

    if (!update_seq_number == pos) {
      fw_update_log("Wrong sequence number.");
      update_data = TRANSMISSION_FAILED;
      update_data_available = true;
      return ;
    }
    else
    {
      update_seq_number++;
    }

    int i;
    // retrieve file data bytes from payload
    for (i = 0; i < chunk_size; i++)
    {
      received_bytes[i] = event.payload[i + 2];
    }
    if (!appendFile(received_bytes, pos * num_bytes_per_payload, chunk_size))
    {
      update_data = TRANSMISSION_FAILED;
      update_data_available = true;
      closeFile();
      // Send ACK before return
      send_ack();
      return ;
    }
    // Send ACK after finished file operation
    send_file_ack(pos + 1);
    if (total_package_count == pos + 1)
    {
      fw_update_log("File Transmission completed...");
      send_update_status(TRANSMISSION_COMPLETED);
      closeFile();

      calculateFileHash(LITTLEFS, last_update_file_name, calculatedHash);
      if (validate_file_hash(calculatedHash))
      {
        fw_update_log("Hash verified.");
        update_data = FILE_VERIFIED;
      }
      else
      {
        fw_update_log("Hash wrong.");
        update_data = FILE_CORRUPTED;

      }
      update_data_available = true;
    }

  }
  else if (FIRMWARE_UPDATE == event.command)
  {

    send_ack();

    uint8_t i = 0;
    // retrieve hash from payload
    uint8_t receivedHash[HASH_SIZE];
    for (; i < HASH_SIZE; i++)
    {
      receivedHash[i] = event.payload[i];
    }
    // retrieve filename / filepath from payload
    char receivedFilename[128];
    for (; i <= event.len_payload; i++)
    {
      receivedFilename[i - HASH_SIZE] = event.payload[i];
    }

    // execute update from given filename
    if (validate_file_hash(receivedHash))
    {
      updateFromFS(LITTLEFS, receivedFilename);
    }
    else
    {
      update_data = FILE_CORRUPTED;
      update_data_available = true;
    }

  }
  else if (DELETE_FILE == event.command)
  {
    send_ack();
    if (event.payload[0] == 1)
    {
      if (deleteAllFiles("/") >= 0)
      {
        update_data = FILE_DELETED;
      }
      else
      {
        update_data = DELETE_FAILED;
      }
    }
    else
    {
      char received_filepath[128];

      for (int i = 0; i < event.len_payload; i++)  // retrieve filename/filepath from payload
      {
        received_filepath[i] = event.payload[i + 1];
      }

      if (deleteFile(received_filepath))
      {
        update_data = FILE_DELETED;
      }
      else
      {
        update_data = DELETE_FAILED;
      }
    }
    update_data_available = true;
    listDir("/", 1);

  }
}

void send_ack()
{
  byte *payload;
  payload = (byte*)malloc(2);
  memset(payload, 0x0, 2);
  payload[0] |= byte_prog_ver & 0x0F;
  payload[0] |= ((byte_app_version & 0x0F) << 4);

  byte status = 0x0;
  if (byte_app_version == 4)
  {
    // prepare status byte
    // bit 0 -  undefined
    // bit 1 -  counter undefined

#ifndef MOD_IO64
    if (counter_undef)
    {
      status |= 0x02;
    }
#endif
  }
  else
  {
    status = 0x0;
  }

  payload[1] = status;

  _rs485message msg;
  msg.devaddr = DEVADDR;
  msg.command = ACK;
  msg.payload = payload;
  msg.len_payload = 2;
  rs485.send(msg);
  free(payload);
}

void send_nak()
{
  _rs485message msg;
  msg.devaddr = DEVADDR;
  msg.command = NAK;
  msg.payload = NULL;
  msg.len_payload = 0;
  rs485.send(msg);
}

void send_update_status(byte state)
{
  byte *payload;
  payload = (byte*)malloc(1);
  memset(payload, state, 1);
  _rs485message msg;
  msg.devaddr = DEVADDR;
  msg.command = UPDATE_STATUS;
  msg.payload = payload;
  msg.len_payload = 1;
  rs485.send(msg);
  free(payload);
}

void send_file_ack(uint16_t seq_no)
{
  byte *payload;
  payload = (byte*)malloc(4);
  memset(payload, 0x0, 4);
  payload[0] |= byte_prog_ver & 0x0F;
  payload[0] |= ((byte_app_version & 0x0F) << 4);

  byte status = 0x0;
  if (byte_app_version == 4)
  {
    // prepare status byte
    // bit 0 -  undefined
    // bit 1 -  counter undefined

    if (counter_undef)
    {
      status |= 0x02;
    }
  }
  else
  {
    status = 0x0;
  }

  payload[1] = status;

  // add seq number for file upload
  payload[2]  = (seq_no >> 8) & 0xFF; //offset 0

  payload[3] = seq_no & 0xFF;        //offset 1

  _rs485message msg;
  msg.devaddr = DEVADDR;
  msg.command = ACK;
  msg.payload = payload;
  msg.len_payload = 4;
  rs485.send(msg);
  free(payload);
}
