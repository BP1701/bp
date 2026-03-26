#ifdef MOD_EV_CHARG
uint8_t gpios_available[] = {18, 19}; //IO-Modul EV
#else
//uint8_t gpios_available[] = {12, 13, 25}; // Wassersaeule, 12-Pinpad Beleuchtung, 13 Wasser, 25 Spuelen
uint8_t gpios_available[] = {12, 13}; // Stromsaeule 2x
//uint8_t gpios_available[] = {12, 13, 25, 33}; // Stromsaeule 4x
//uint8_t gpios_available[] = {18, 19, 23, 32}; // Dagebüll
//uint8_t gpios_available[] = {12, 13, 25, 33}; // Luetetsburg
#endif

uint8_t *gpio_state;
uint8_t *gpio_new_state;
uint8_t *gpio_blink;
uint32_t *gpio_end_time;
uint32_t last_blink_timer;
bool blink_state;
bool blink_old_state;

#define BLINK_INTERVAL 500


void init_outputs()
{
  gpio_state = (uint8_t*)malloc(sizeof(uint8_t) * sizeof(gpios_available));
  gpio_new_state = (uint8_t*)malloc(sizeof(uint8_t) * sizeof(gpios_available));
  gpio_blink = (uint8_t*)malloc(sizeof(uint8_t) * sizeof(gpios_available));
  gpio_end_time = (uint32_t*)malloc(sizeof(uint32_t) * sizeof(gpios_available));

  for (int i = 0; i < sizeof(gpios_available); i++)
  {
    Serial.printf("init gpio %d\n", gpios_available[i]);
    pinMode(gpios_available[i], OUTPUT);
    digitalWrite(gpios_available[i], LOW);
    gpio_state[i] = 0;
    gpio_new_state[i] = 0xFF;
    gpio_blink[i] = 0;
    gpio_end_time[i] = 0;
  }

  last_blink_timer = millis();
  blink_state = true;
  blink_old_state = true;
  gpios_data_available = true;
}

uint8_t is_gpio_available(uint8_t gpio)
{
  for (uint8_t i = 0; i < sizeof(gpios_available); i++)
  {
    if (gpios_available[i] == gpio)
    {
      return i;
    }
  }
  return 0xFF;
}

void set_gpio(uint8_t gpio, uint8_t state, uint32_t duration, uint8_t blink)
{
  uint8_t idx = is_gpio_available(gpio);

  if (idx == 0xFF)
  {
    gpio_new_state[idx] = 0xFF;
    return;
  }

 
  gpio_new_state[idx] = state;

  if (duration)
  {
    gpio_end_time[idx] = millis() + duration;
  }
  else
  {
    gpio_end_time[idx] = 0;
  }

  gpio_blink[idx] = blink;

  Serial.printf("get new value. gpio: %d, state: %d, duration: %ld, blink: %d\n", gpio, gpio_new_state[idx], duration, gpio_blink[idx]);
}

bool set_outputs()
{
  uint32_t t = millis();

  if ((t - last_blink_timer) > BLINK_INTERVAL)
  {
    last_blink_timer = t;
    blink_state = !blink_state;
  }

  for (uint8_t i = 0; i < sizeof(gpios_available); i++)
  {
    // update new values
    if (gpio_new_state[i] != 0xFF)
    {
      if (gpio_blink[i] == 0 || (gpio_blink[i] == 1 && blink_state))
      {
        digitalWrite(gpios_available[i], gpio_new_state[i]);
      }
      gpio_state[i] = gpio_new_state[i];
      gpio_new_state[i] = 0xFF;
      gpios_data_available = true;
      Serial.printf("set new value for gpio: %d value: %d\n", gpios_available[i], gpio_state[i]);
    }

    // check timer
    if (t > gpio_end_time[i] && gpio_end_time[i] > 0)
    {
      digitalWrite(gpios_available[i], LOW);
      gpio_state[i] = 0;
      gpio_end_time[i] = 0;
      gpio_blink[i] = 0;
      gpios_data_available = true;
      Serial.printf("delete timer fpr gpio: %d\n", gpios_available[i]);
    }

    // blink
    if (blink_state != blink_old_state)
    {
      if (gpio_blink[i] != 0)
      {
        Serial.printf("blink - gpio %d --> %d\n", gpios_available[i], blink_state);
        digitalWrite(gpios_available[i], blink_state);
      }
    }
  }
  
  if (blink_state != blink_old_state)
  {
    blink_old_state = blink_state;
  }
}
