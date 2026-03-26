// init opticon class
int init_opticon()
{
#ifdef _DEBUG
  opticon.setLog(log);
#endif
  memset(opticon_data, 0x0, 2048);
  opticon_data_len = 0;
  opticon_data_avaiable = false;
  opticon.onEvent(opticonCallback);
  //opticon.begin(4, 9600);
  //opticon.begin(1, 9600);        
  opticon.begin(&Serial);       
}

// callback function is called when opticon data available
void opticonCallback(byte *result, int len)
{
#ifdef _DEBUG
  char message[2048];
  memset(message, 0x0, 2048);
  sprintf(message, "OPTICON: ");
  memcpy(message + 9, result, len);
  log(message);
  Serial.print("OPTICON: Time ");
  Serial.print(millis() - _time_scan);
  Serial.print(" LEN: ");
  Serial.println(len);
  
#endif
  memcpy(opticon_data, result, len);
  opticon_data_len = len;
  opticon_data_avaiable = true;
}

// opticon task
void task_opticon(void *param)
{
  for (;;)
  {
    
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}
