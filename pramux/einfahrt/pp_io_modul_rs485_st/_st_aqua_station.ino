#define AQUA_CHECK_INTERVAL 250
#define AQUA_VALVE_GPIO 13
#define AQUA_VALVE_TIMEOUT 1800000 //in ms 300000
#define AQUA_FLUSH_VALVE_GPIO 25
#define AQUA_FLUSH_QUANTITY 10 // in Liter
#define AQUA_FLUSH_INTERVAL 86400000 //in ms 86400000

uint8_t gpios_aqua_station[] = {40, 41, 42, 42}; //Virtuelle GPIos zur Tarifauswahl
uint8_t amounts_aqua_station[] = {10, 20, 50, 100};
uint32_t last_aqua_check_timer = 0;
uint32_t last_aqua_flush_timer = 0;

uint8_t current_amount = 0;
boolean flush_is_active = false;

void init_aqua_station()
{
  current_amount = AQUA_FLUSH_QUANTITY;
  
  set_gpio(AQUA_VALVE_GPIO, 1, 0, 0);
  set_gpio(AQUA_FLUSH_VALVE_GPIO, 1, 0, 0);
  flush_is_active = true;

  last_aqua_flush_timer = millis();
    
  counter_value[0] = 0;
  
  for (int i = 0; i < sizeof(gpios_aqua_station); i++)
  {
    Serial.printf("gpio %d = amount %d\n", gpios_aqua_station[i], amounts_aqua_station[i]);
  }  
}

uint8_t is_aqua_gpio_available(uint8_t gpio)
{
  for (uint8_t i = 0; i < sizeof(gpios_aqua_station); i++)
  {
    if (gpios_aqua_station[i] == gpio)
    {
      return i;
    }
  }
  return 0xFF;
}

void set_amount_index(uint8_t amount_index)
{
  current_amount = amounts_aqua_station[amount_index];
  counter_value[0] = 0;

  Serial.printf("set current amount to: %d\n", current_amount);
  
  set_gpio(AQUA_VALVE_GPIO, 1, AQUA_VALVE_TIMEOUT, 0);
}

void loop_aqua_station()
{
  uint32_t t = millis();
  boolean water_is_active;

  if ((t - last_aqua_check_timer) < AQUA_CHECK_INTERVAL)
  {
    return;
  }

  last_aqua_check_timer = t;
  water_is_active = digitalRead(AQUA_VALVE_GPIO);

  if (water_is_active)
  {
    if (counter_value[0] > current_amount) 
    {
      set_gpio(AQUA_VALVE_GPIO, 0, 0, 0);
      set_gpio(AQUA_FLUSH_VALVE_GPIO, 0, 0, 0);
      last_aqua_flush_timer = t; //Zeitstempel letzte Wasserentnahme
      flush_is_active = false;
    }
    else if (flush_is_active) 
    {
      if ((t - last_aqua_flush_timer) > AQUA_VALVE_TIMEOUT) //Timeout wenn beim Spülen keine Wasserabnahme erfolgt
      {    
        set_gpio(AQUA_VALVE_GPIO, 0, 0, 0);
        set_gpio(AQUA_FLUSH_VALVE_GPIO, 0, 0, 0); 
        flush_is_active = false;   
      }
    }    
  }
  else { //nur Spuelen wenn keine Wasserentnahme aktiv ist
    if ((t - last_aqua_flush_timer) > AQUA_FLUSH_INTERVAL)
    {
      current_amount = AQUA_FLUSH_QUANTITY;
      counter_value[0] = 0;
      last_aqua_flush_timer = t; //Zeitstempel letzte Wasserentnahme
          
      set_gpio(AQUA_VALVE_GPIO, 1, 0, 0);
      set_gpio(AQUA_FLUSH_VALVE_GPIO, 1, 0, 0);
      flush_is_active = true;    
    }  
  }
}
