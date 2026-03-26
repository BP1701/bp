#ifdef MOD_COUNTER

uint32_t time_check_counter = 0; 

void init_counter()
{
  // reset values
  for (uint8_t i = 0; i < COUNTER_COUNT; i++)
  {
    counter_s0_value[i]   = 0xFF;
    counter_pins_value[i] = false;
  }
  // enable interrupt pins
  for (uint8_t i = 0; i < COUNTER_COUNT; i++)
  {
    pinMode(counter_pins[i], INPUT);
  }
}

void loop_counter()
{
  int pin;
  bool pin_value;
  
  if ((millis() - time_check_counter) < (COUNTER_S0_MIN / 2)) { 
    return;
  }

  time_check_counter = millis();

  for (uint8_t i = 0; i < COUNTER_COUNT; i++)
  {
    pin = counter_pins[i];
#ifdef MOD_EV_CHARG
    pin_value = digitalRead(pin); //EV Charg Module
#else
    pin_value = !digitalRead(pin); //Normal IO Module
#endif
 
    if (counter_pins_value[i] != pin_value)
    {
      if (counter_pins_temp_value[i] != pin_value) 
      {
         counter_pins_temp_value[i] = pin_value; //erst im tmp Speichern...
      }
      else 
      {
        counter_pins_value[i] = counter_pins_temp_value[i];   //und nach der Entprellzeit in Values speichern       
      
        if(pin_value) 
        {
          counter_value[i]++;
          counter_update_flag |= 0x01 << i;
          counter_data_available = true;

          Serial.print("counter ");
          Serial.print(i); 
          Serial.print(" at pin ");
          Serial.print(pin);
          Serial.print(" flag ");
          Serial.print(counter_update_flag);
          Serial.print(" value ");
          Serial.println(counter_value[i]);
        } 
      }
    } 
    else 
    {
      counter_pins_temp_value[i] = pin_value; //prellen, tmp value zurück setzen
    }
  }
}







#endif
