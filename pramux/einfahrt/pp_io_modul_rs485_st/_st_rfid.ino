// init rfid class
int init_rfid()
{
#ifdef _DEBUG
  rfid.setLog(log);
#endif
  memset(rfid_data, 0x0, 64);
  rfid_data_len = 0;
  rfid_data_available = false;
  rfid.onEvent(rfidCallback);
  //rfid.begin(2, 38400);
  rfid.begin(2, 9600, RDM6300);
}

// callback function is called when rfid data available
void rfidCallback(byte *result, int len)
{
#ifdef _DEBUG
  char message[64];
  memset(message, 0x0, 64);
  sprintf(message, "RFID: ");
  memcpy(message + 6, result, len);
  sprintf(message, "%s LEN: %d", message, len);
  log(message);
#endif
  memcpy(rfid_data, result, len);
  rfid_data_len = len;
  rfid_data_available = true;
  Serial.print("--> time rfid ");
  Serial.println(millis() - _time_rfid);
}
