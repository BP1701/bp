
#define DEBOUNCE_TIME 20
#ifdef MOD_EV_CHARG
uint16_t input_gpios_available[] = {36, 39}; //IO-Modul EV
uint8_t input_gpios_count = 4;
#else
//uint16_t input_gpios_available[] = {34, 35, 36, 39};
uint16_t input_gpios_available[] = {36, 39}; //pap
uint8_t input_gpios_count = 2;
#endif


uint32_t input_last_change[16];


uint16_t input_gpios_state = 0x0;
uint16_t input_gpios_state_old = 0x0;
uint16_t input_change_mask = 0x0;

void init_inputs()
{
  for (int i = 0; i < 16; i++)
  {
    input_last_change[i] = 0;
  }
  for (int i = 0; i < input_gpios_count; i++)
  {
    pinMode(input_gpios_available[i], INPUT);
  }
  input_gpios_state = read_inputs();
  input_gpios_state_old = input_gpios_state;

  input_gpios_data_available = true;
  input_change_mask = 0xFFFF;
}

uint16_t read_inputs()
{

  // read current status
  uint16_t tmp_gpios_state = input_gpios_state;
  for (int i = 0; i < input_gpios_count; i++)
  {

    // only read once every <DEBOUNCE_TIME> ms
    if (((millis() - input_last_change[i]) < DEBOUNCE_TIME) && input_last_change[i] > 0)
    {
      continue;
    }

    input_last_change[i] = millis();

    uint8_t tmp_read = !digitalRead(input_gpios_available[i]);
    
    if (tmp_read)
    {
      tmp_gpios_state |= (1 << i);
    }
    else
    {
      tmp_gpios_state &= ~(1 << i);
    }
  }

  return tmp_gpios_state;
}

void loop_inputs()
{
  if (!input_gpios_data_available)
  {
    input_change_mask = 0x0;
    input_gpios_state_old = input_gpios_state;

    input_gpios_state = read_inputs();
    if (input_gpios_state_old != input_gpios_state)
    {
      for (int i = 0; i < input_gpios_count; i++)
      {
        if ((1 << i) & (input_gpios_state ^ input_gpios_state_old))
        {
          input_change_mask |= (1 << i);
        }
      }

      if (input_change_mask)
      {
        Serial.println("inputs changed");
        for (int i = 0; i < input_gpios_count; i++)
        {
          if ((1 << i) & input_change_mask)
          {
            Serial.print("  --> change at pin ");
            Serial.print(input_gpios_available[i]);
            Serial.print(" - new value ");
            uint8_t state = ((input_gpios_state  & (1 << i)) >> i) == 1 ? 1 : 0;
            Serial.println(state);

            input_gpios_data_available = true;
          }
        }
      }
    }
  }
}
