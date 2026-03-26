void io_cb(uint8_t port, uint16_t change_mask, uint16_t state)
{
#ifdef GWV2
  uint8_t value;
  value = port_exp_input_0.read_all();
  Serial.printf("State 0x%02x\n", value);
  
  for (int i = 0; i < 8; i++)
  {
    if (change_mask & (1 << i))
    {
      char topic[128];
      char payload[128];
      uint8_t channel;

      uint8_t state = ((value  & (1 << i)) >> i) == 1 ? 0 : 1;

      channel = get_device_num(PP_SEN_INP_INT);

      if (channel != 0xFF) //input io handling
      {
        sprintf(topic, "%s/%s/channel/%02d/gpios/%02d", mqtt_topic_praefix, mqtt_device_serial, channel, i);
        sprintf(payload, "state=%d", state);
        publish(topic, payload, true);
        Serial.printf("change state to %d on pin %d - channel %d\n", state, i, channel);  
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }
      
      channel = get_device_num_actor(i, PP_SEN_CNT_INT);
      if (channel == 0xFF) channel = get_device_num(PP_SEN_CNT_INT); 
      
      if (channel != 0xFF && state == 1) //input counter handling
      {
        if(devices[channel].actor_addr == 0xff) 
        {
          sprintf(topic, "%s/%s/channel/%02d/counter/%02d", mqtt_topic_praefix, mqtt_device_serial, channel, i);
        }
        else {
          sprintf(topic, "%s/%s/channel/%02d/counter/00", mqtt_topic_praefix, mqtt_device_serial, channel);
        }
      
        if (devices[channel].counter_value[i] == 0xFFFFFFFF)
        {
          devices[channel].counter_value[i] = 1;
        }
        else
        {
          devices[channel].counter_value[i]++;
        }
        sprintf(payload, "value=%d", devices[channel].counter_value[i]);
        publish(topic, payload, true);
        Serial.printf("change counter %d to %d - channel %d\n", i, devices[channel].counter_value[i], channel);
        vTaskDelay(10 / portTICK_PERIOD_MS);
      }
    }
  }
#endif  
}

void init_io()
{
#ifndef GWV2
  port_exp_0.begin(IO_I2C_LOW, IO_I2C_HIGH, 0xFFF0);
  port_exp_0.enable_interrupt(33, io_cb);
  port_exp_0.set_change_event(io_change);
#else
  Wire.begin();
  port_exp_output_0.set_change_event(0, io_change);
  port_exp_output_0.begin(0, &Wire);
  for (uint8_t i = 0; i < 16; i++)
  {
    port_exp_output_0.pinMode(i, OUTPUT);
  }

  port_exp_input_0.enable_interrupt(0, 0, io_cb);
  port_exp_input_0.begin(1, &Wire);

  for (int i = 0; i < 8; i++)
  {
    port_exp_input_0.pinMode(i, INPUT);
  }

  
#endif
}

void io_change(uint8_t port, uint8_t pin, uint8_t value, uint32_t duration)
{
  uint8_t channel;
  char topic[128];
  char payload[128];
    
  channel = get_device_num(PP_ACT_REL_INT);

  sprintf(topic, "%s/%s/channel/%02d/gpios/%02d", mqtt_topic_praefix, mqtt_device_serial, channel, pin);  
  sprintf(payload, "state=%s", value ? "1" : "0");
  publish(topic, payload, true);
  
  Serial.printf("change output state to %s on pin %d - channel %d\n", value ? "1" : "0", pin, channel); 
}

void task_io_modul(void *param)
{
  for (;;)
  {
#ifdef GWV2
    port_exp_output_0.run();
    port_exp_input_0.run();
#endif
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
