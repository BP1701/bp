void init_nexscan()
{
#ifdef _DEBUG
  nexscan.setLog(log);
#endif
  memset(rfid_data, 0x0, 64);
  memset(opticon_data, 0x0, 64);
  rfid_data_len = 0;
  opticon_data_len = 0;
  rfid_data_available = false;
  opticon_data_avaiable = false;
  nexscan.onEvent(nexscanCallback);
  nexscan.begin(2, 9600);
}

void nexscanCallback(byte *result, int len, byte datasource)
{
#ifdef _DEBUG
  char message[64];
  memset(message, 0x0, 64);
  sprintf(message, "NEXSCAN: ");
  memcpy(message + 9, result, len);
  sprintf(message, "%s LEN: %d DATASOURCE: %02x %02x", message, len, (uint8_t)datasource, DATASOURCE_RFID_FEIG);
  log(message);
#endif

  switch (datasource)
  {
    case DATASOURCE_RFID_FEIG:
      {
        memcpy(rfid_data, result, len);
        rfid_data_len = len;
        rfid_data_available = true;
        beep();
        //set_gpio(21, 1, 150, 0);

#ifdef NSC
        if (rfid_data[0] == 0x31 && rfid_data[1] == 0x32 && rfid_data[2] == 0x31 && rfid_data[3] == 0x37)
        {
          Serial.println("NordseeCamp Karte gefunden");
          led_set(2, 0, 128, 0, 1000, 0);
          set_gpio(12, 1, 250, 0);
        }
        else
        {
          led_set(2, 128, 0, 0, 1000, 0);
        }
#endif
        break;
      }
    case DATADOURCE_SCAN_OPTI:
      {
        memcpy(opticon_data, result, len);
        opticon_data_len = len;
        opticon_data_avaiable = true;
        beep();  
        //set_gpio(21, 1, 150, 0);

#ifdef NSC
        int res;
        int artikel = 0;
        int id = 0;
        uint32_t timestamp = 0;
        char *code;

        code = (char*)malloc(len + 1);
        memcpy(code, result, len);
        code[len] = 0x0;

        if (check_offline_code(code, artikel, id, timestamp) == 0)
        {
          Serial.println("gültiger Barcode");
          led_set(2, 0, 128, 0, 1000, 0);
          set_gpio(12, 1, 250, 0);
        }
        else
        {
          Serial.println("ungültiger Barcode");
          led_set(2, 128, 0, 0, 1000, 0);
        }

        free(code);
        Serial.println(ESP.getFreeHeap());
#endif
      }
    case DATASOURCE_RFID_IDRW01:
      {
        char _tmp_rfid_dat[64];
        memset(_tmp_rfid_dat, 0x0, 64);

        for (uint8_t i = 3; i < (len - 1); i++) //(COM-Adr)(0x13)(Status2)(4...n-1)(csum)
        {
          sprintf(_tmp_rfid_dat, "%s%02X", _tmp_rfid_dat, result[i]);
        }
        
        rfid_data_len = (len - 4) * 2;
        memcpy(rfid_data, _tmp_rfid_dat, rfid_data_len);        
        rfid_data_available = true;
        beep();
        //set_gpio(21, 1, 150, 0);
      }
            
      break;
  }
}
