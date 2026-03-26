#define WATCHDOG_TIMEOUT 30000

// watchdog task
void task_watchdog(void *param)
{
  for (;;)
  {
    uint8_t active_device_count = 0;
    uint8_t inactive_device_count = 0;
    uint8_t seen_device_count = 0;

    uint32_t akt_time = millis();
    
    for (int i = 0; i < NUM_DEVICES; i++)
    {
      if (devices[i].last_seen > 0)
      {
        seen_device_count++;
        
        if (devices[i].last_seen + WATCHDOG_TIMEOUT > akt_time) //after 30 seconds a device is considered inactive       
        {
          active_device_count++;
        }
        else
        {
          inactive_device_count++;
        }
      }
    }

#ifdef RESTART_CLIENTS_PORT

    if (inactive_device_count > 0) //at least one non-active client
    {
      // possible problem with rs485 client
      Serial.println("possible problem with rs485 client, restart all clients...");

      port_exp_output_0.digitalWrite(RESTART_CLIENTS_PORT, HIGH);

      vTaskDelay(250 / portTICK_PERIOD_MS);      

      for (int i = 0; i < NUM_DEVICES; i++) //reset last_seen
        {
          if (devices[i].last_seen + WATCHDOG_TIMEOUT < akt_time) //reset last seen of inactive clients
          {
            devices[i].last_seen = 0;
          }
        }

      vTaskDelay(1750 / portTICK_PERIOD_MS);
      
      port_exp_output_0.digitalWrite(RESTART_CLIENTS_PORT, LOW);     
    }

#endif

    if ((active_device_count == 0) && (seen_device_count > 0)) //no active active client
    {
      // possible problem with rs485 communication
      rebootEspWithReason("possible problem with rs485 communication, restart esp...");
    }
    
    vTaskDelay(5000 / portTICK_PERIOD_MS);
  }
}
