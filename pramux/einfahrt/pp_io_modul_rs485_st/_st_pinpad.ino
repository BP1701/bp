//#define PINPAD_DISABLE_PIN 36 //Sanistation
#define PINPAD_DISABLE_PIN 13 //Aqua Station

bool pinpad_disable;

int init_pinpad()
{
  pinpad.onPinpadEvent(cb_pinpad_event);
  pinpad.onLedEvent(cb_led_event);
  pinpad.init();

#ifdef PINPAD_DISABLE_PIN
  #ifndef MOD_AQUA_STATION
    pinMode(PINPAD_DISABLE_PIN, INPUT);  
  #endif   
#endif 

  pinpad_disable = false;
  pinpad_timer = 0;

  return 0;
}

void cb_pinpad_event(byte *result, int len)
{
  char cres[10];
  memcpy(cres, result, len);
  cres[len] = 0x0;

  Serial.print("Pinpad: ");
  Serial.println(cres);

#ifdef PINPAD_DISABLE_PIN
    pinpad_disable = digitalRead(PINPAD_DISABLE_PIN);  
#endif  

  if(!pinpad_disable)
  {
    memcpy(pinpad_data, result, len);
    pinpad_data_len = len;
    pinpad_data_available = true;

    pinpad_timer = millis();
  }
  else
  {
    Serial.print("Pinpad disabled"); 
  }  
}

void cb_led_event(uint8_t group, uint8_t red, uint8_t green, uint8_t blue, uint32_t duration)
{
  
#ifdef PINPAD_DISABLE_PIN
    pinpad_disable = digitalRead(PINPAD_DISABLE_PIN);  
#endif 
  
  if(!pinpad_disable)
  { 
    //Serial.printf("update led. group %d, red %d, green %d, blue, duration %d\n", group, red, green, blue, duration);
    led_set(group, red, green, blue, duration, 0);
  }
  else
  {
    //Serial.printf("Pinpad disabled, blink red");
    led_set(0, 48, 0, 0, 2000, 1);
  }
}
