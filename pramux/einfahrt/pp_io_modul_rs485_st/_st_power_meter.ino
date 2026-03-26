#ifdef MOD_POWER_METER


void power_meter_isr0()
{
  uint8_t pm_value = digitalRead(power_meter_pins[0]);

  Serial.print("interrupt at pm0 ");

  if (pm_value == 0 && (power_meter_s0_value[0] == 1 || power_meter_s0_value[0] == 0xFF))
  {
    // S0 impulse change detected
    // measure impuls start time
    power_meter_s0_duration[0] = millis();
  }

  if (pm_value == 1)
  {
    uint32_t pm_duration = millis() - power_meter_s0_duration[0];

    if ((pm_duration > POWER_METER_S0_MIN && pm_duration < POWER_METER_S0_MAX))
    {
      power_meter_value[0]++;
      power_meter_update_flag |= 0x01;
      Serial.print(power_meter_update_flag);
      Serial.print(" ");
      Serial.print(power_meter_value[0]);
      Serial.print(" impulse duration ");
      Serial.println(pm_duration);
    }
  }
}

void power_meter_isr1()
{
  uint8_t pm_value = digitalRead(power_meter_pins[1]);

  Serial.print("interrupt at pm1 ");

  if (pm_value == 0 && (power_meter_s0_value[1] == 1 || power_meter_s0_value[1] == 0xFF))
  {
    // S0 impulse change detected
    // measure impuls start time
    power_meter_s0_duration[1] = millis();
  }

  if (pm_value == 1)
  {
    uint32_t pm_duration = millis() - power_meter_s0_duration[1];

    if ((pm_duration > POWER_METER_S0_MIN && pm_duration < POWER_METER_S0_MAX))
    {
      power_meter_value[1]++;
      power_meter_update_flag |= 0x02;
      Serial.print(power_meter_update_flag);
      Serial.print(" ");
      Serial.print(power_meter_value[1]);
      Serial.print(" impulse duration ");
      Serial.println(pm_duration);
    }
  }
}


void init_power_meter()
{
  // reset values
  for (uint8_t i = 0; i < POWER_METER_COUNT; i++)
  {
    power_meter_s0_value[i] = 0xFF;
  }
  // enable interrupt pins
  pinMode(power_meter_pins[0], INPUT);
  pinMode(power_meter_pins[1], INPUT);
  attachInterrupt(digitalPinToInterrupt(power_meter_pins[0]), power_meter_isr0, CHANGE);
  attachInterrupt(digitalPinToInterrupt(power_meter_pins[1]), power_meter_isr1, CHANGE);
}

void loop_power_meter()
{
  if (0 == power_meter_update_flag)
  {
    return;
  }

  power_meter_data_available = true;

}







#endif
