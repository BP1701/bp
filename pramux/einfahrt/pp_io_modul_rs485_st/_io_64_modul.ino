#define I2C_SDA 21
#define I2C_SCL 22

#ifdef MOD_IO64
void init_io()
{
  Wire.begin(I2C_SDA, I2C_SCL, 100000);
  port_exp_output_0.set_change_event(0, io_change);
  port_exp_output_1.set_change_event(1, io_change);

  port_exp_output_0.begin(2, &Wire);
  port_exp_output_1.begin(3, &Wire);

  for (uint8_t i = 0; i < 16; i++)
  {
    port_exp_output_0.pinMode(i, OUTPUT);
    port_exp_output_1.pinMode(i, OUTPUT);
    port_exp_output_0.digitalWrite(i, LOW);
    port_exp_output_1.digitalWrite(i, LOW);
  }

  port_exp_input_0.begin(0, &Wire);
  port_exp_input_1.begin(1, &Wire);

  for (int i = 0; i < 16; i++)
  {
    port_exp_input_0.enable_interrupt(0, i, io_cb);
    port_exp_input_1.enable_interrupt(1, i, io_cb);
    iomodul_counter_value_tmp[i] = 0;
    iomodul_counter_value_tmp[i + 16] = 0;
  }

  if (outputs_undef)
  {
    iomodul_output_available = true;
    iomodul_output_data = (0xFF << 8) + 0x0;
  }
}

void io_cb(uint8_t port, uint16_t change_mask, uint16_t state)
{
  //Serial.printf("interrupt event 0x%02x @ %d\n", change_mask, port);

  /*
    // reading values
    uint16_t state = 0;
    if (port == 0)
    {
      state = port_exp_input_0.read_all();
    }
    else if (port == 1)
    {
      state = port_exp_input_1.read_all();
    }
    else
    {
      return;
    }
  */

  bool change = false;
  for (uint8_t i = 0; i < 16; i++)
  {

    if (change_mask & (1 << i))
    {
      //Serial.printf("io %d @ port %d changed to %d\n", i, port, (state & (1 << i)) > 0);

      // only watch rising edge
      if ((state & (1 << i)) > 0)
      {
        if (port == 0)
        {
          iomodul_counter_value_tmp[i]++;
          iomodul_counter_values[i]++;
          //iomodul_data |= state & (1 << i);
        }
        else if (port == 1)
        {
          iomodul_counter_value_tmp[i + 16]++;
          iomodul_counter_values[i + 16]++;
          //iomodul_data |= ((state & (1 << i)) << 16);
        }

        //Serial.printf("id % 2d @ port %d  changed to %d\n", i, port, (state & (1 << i)) > 0);
      }
    }
  }
}

void io_change(uint8_t port, uint8_t pin, uint8_t value, uint32_t duration)
{
  Serial.printf("IO change! port: %d pin: %d value: %d\n", port, pin, value);
  if (!outputs_undef)
  {
    iomodul_output_data = 0x0;
    iomodul_output_data = (pin + (port * 16) << 8) + value;
    iomodul_output_available = true;
  }
}

void io_modul_run()
{
  port_exp_output_0.run();
  port_exp_output_1.run();
  port_exp_input_0.run();
  port_exp_input_1.run();

  for (int i = 0; i < 32; i++)
  {
    if ((iomodul_counter_value_tmp[i] > 0) && (iomodul_counter_value_tmp[i] % 10) == 0)
    {
      iomodul_input_data |= (1 << i);
      iomodul_counter_value_tmp[i] -= 10;
      iomodul_input_data_available = true;
    }
  }

  long count1 = 0;
  long count2 = 0;
  for (int i = 0; i < 16; i++)
  {
    count1 += iomodul_counter_values[i];
    count2 += iomodul_counter_values[i + 16];
  }

}
#endif
